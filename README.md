# APM32E103ZE EVAL Board

Geehy APM32E103ZE (Cortex-M3, pin/register-compatible with STM32F103 "High
Density") üzerinde, resmi Geehy eval kartı için bring-up projesi. UniBoard'da
kullanılan yöntemle (STM32CubeIDE tarzı, elle CMake+GCC) kuruldu — PlatformIO'nun
Geehy/APM32 için resmi desteği yok.

## Kaynak

Tüm sürücü/CMSIS/başlangıç dosyaları Geehy'nin resmi deposundan alındı:
https://github.com/GeehySemi/APM32E10x_EVAL_SDK

- `Drivers/CMSIS` — ARM CMSIS-Core (v5.1.0) + Geehy'nin cihaz dosyaları (apm32e10x.h, system_apm32e10x.c)
- `Drivers/APM32E10x_StdPeriphDriver` — Geehy'nin Standart Çevrebirim Kütüphanesi (STM32 HAL değil, eski nesil StdPeriph tarzı)
- `Src/startup_apm32e10x_hd.S` — SDK'nın Keil/armasm formatındaki `startup_apm32e10x_hd.s` dosyasından GNU assembler sözdizimine elle çevrildi (SDK sadece MDK/IAR proje dosyaları içeriyor, GCC yok)
- `apm32e10x_flash.ld` — Elle yazıldı (SDK'nın Keil `.uvprojx` dosyasından FLASH=0x08000000/512K, RAM=0x20000000/128K bellek haritası doğrulandı)

## Bulunan/Düzeltilen Sorun

`core_cm3.h` (muhtemelen daha yeni bir CMSIS-Core sürümünden), `__COMPILER_BARRIER`
makrosunu kullanıyor ama SDK'nın kendi `cmsis_gcc.h`'ı (v5.1.0) bu makroyu
tanımlamıyor — derleme hatası veriyordu. `cmsis_gcc.h`'a upstream CMSIS'teki
karşılığıyla aynı tanımı elle eklendi (`__ASM volatile("":::"memory")`).

## Pin Haritası (Geehy'nin Board_APM32E103_EVAL.h dosyasından doğrulandı)

    LED1 = PD13   LED2 = PD14   LED3 = PD15
    KEY1 = PF9    KEY2 = ...    KEY3 = ...   (bkz. Board_APM32E103_EVAL.h, henüz kullanılmadı)

## Sistem Saati

`system_apm32e10x.c` SDK'dan olduğu gibi (değiştirilmeden) alındı — varsayılan
72MHz (HSE 8MHz + PLL), eval kartının kendi kristaline göre üretici tarafından
zaten test edilmiş. Sıfırdan saat konfigürasyonu yazılmadı.

## Yapıldı

- [x] **Blink (LED1/LED2/LED3, PD13/14/15)** — ST-Link ile flaşlanıp donanımda
      doğrulandı (2026-09-01): üç LED birlikte 2 saniyede bir yanıp sönüyor.
      Flaşlama ST-Link ile sorunsuz çalıştı, ayrı bir OpenOCD target config'ine
      gerek kalmadı.
- [x] **ST7789 SPI LCD sürücüsü** (`Src/lcd.c`/`Inc/lcd.h`) — donanımda
      doğrulandı (2026-09-01): SPI1 üzerinden ekran çalışıyor.
      - Pin haritası (Geehy'nin resmi SPI_LCD demosuyla aynı LCD konnektörü):
        `MOSI=PA7 SCK=PA5 CS=PA4 DC=PA6 RES=PA1 BLK=PA8`.
      - **Önemli:** Geehy'nin kendi SPI_LCD demosu (`Examples/SPI/SPI_LCD`)
        aslında ILI9341 kontrolörü için yazılmış (init komutlarında ILI9341'e
        özgü `0xCF/0xED/0xE8/0xCB` kayıtları var), ST7789 değil. SPI/GPIO
        altyapısı, font ve çizim fonksiyonları (kontrolörden bağımsız) demodan
        aynen alındı; sadece `LCD_Init()`'teki register dizisi gerçek ST7789
        için sıfırdan yazıldı.
      - Panel 1.69" yuvarlak köşeli, gerçek görünür alanı 240x280 (320
        yükseklikte GRAM'ın ortasına oturuyor) — kenarların taşması/kesilmesi
        buradan kaynaklanıyordu, `LCD_Y_OFFSET=20` ile düzeltildi
        (`lcd.c`, `LCD_AddressSet`).
      - Ekranda renkler ters/karışık görünürse: kırmızı-mavi yer değiştiyse
        `lcd.c`'deki MADCTL değerini (`0x00` ↔ `0x08`) değiştir; genel renk
        tersliği için `0x21` (INVON) yerine `0x20` (INVOFF) dene.
      - 180° döndürüldü (`MADCTL = 0xC0`, MY|MX bitleri) — donanımdaki fiziksel
        montaja göre kullanıcı isteğiyle ayarlandı.
- [x] **KEY1/KEY2/KEY3 menü arayüzü** (`Src/menu.c`/`Inc/menu.h`) — Geehy'nin
      SPI_LCD demosundaki menü mantığının (KEY1: seç, KEY2: gir, KEY3: geri)
      portu. Buton pinleri Board_APM32E103_EVAL.h'den doğrulandı:
      `KEY1=PF9` (aktif-düşük, pull-up), `KEY2=PC13` (aktif-düşük, pull-up),
      `KEY3=PA0` (aktif-yüksek, pull-down). 3 madde: LED Durumu (canlı
      LED1/2/3 okuma), Sayac (canlı ms tick), Kart Bilgisi (statik metin).
      SDK'nın TMR7-kesmesi + blocking-delay yöntemi yerine, UniBoard'daki
      power-button ile aynı non-blocking tick-tabanlı debounce kullanıldı.
      Menü başlıkları (üst çubuk) ekranda ortalanıyor.
      **Donanımda test edildi (2026-09-01), çalışıyor.**
- [x] **İşlemci sıcaklığı** (`Src/temp.c`/`Inc/temp.h`) — ADC1 kanal 16
      (dahili sıcaklık sensörü) üzerinden okunuyor, "Kart Bilgisi" menü
      sayfasında canlı gösteriliyor.
      - **Bulunan/Düzeltilen sorun:** STM32F103'ün tipik datasheet sabitleri
        (`V25=1.43V, Avg_Slope=4.3mV/°C`) ile ilk denemede oda sıcaklığında
        ~8°C gibi yanlış bir değer okundu. Bu sensör tipinde (F103 tarzı,
        TS_CAL1/TS_CAL2 fabrika kalibrasyon register'ı olmayan eski nesil)
        çip-çip üretim sapması onlarca °C olabiliyor — kod hatası değil,
        sensörün kendi doğal belirsizliği.
      - **Çözüm:** açılışta tek noktalı çalışma zamanı kalibrasyonu —
        kartın açılış anında ~25°C (tipik oda sıcaklığı) civarında olduğu
        varsayılıp ilk ortalama okuma bu çipin V25 referansı olarak alınıyor
        (`Temp_Init()`). Eğim (Avg_Slope) sabit tutuluyor, çünkü aynı üretim
        ailesinde eğim sapması offset'ten çok daha az.

- [x] **USART1 durum logu** (`Src/serial.c`/`Inc/serial.h`) — TX=PA9, RX=PA10,
      115200 8N1. Her 500ms'de bir satır: `TEMP:22.5C LED1:OFF LED2:OFF
      LED3:OFF TICK:58317\r\n`. Donanımda USB-TTL adaptörle test edildi
      (2026-09-01) — sıcaklık, LED durumları ve tick sayacı doğru.
- [x] **Potansiyometreyle ekran parlaklığı** (`Src/backlight.c`/`Inc/backlight.h`)
      — RV1 (10K pot) wiper'ı şematikten doğrulandı: PC0 (ADC12_IN10).
      Arka ışık pini PA8, TIM1_CH1 PWM'e çevrildi (1kHz, 1000 kademe,
      %2 minimum duty ile tam kapanmıyor). `lcd.c` artık PA8'i sürmüyor —
      pin sahipliği tamamen `backlight.c`'ye taşındı. ADC1'i `temp.c` ile
      paylaşıyor (aynı tek-conversion-per-call deseni), bu yüzden
      `Temp_Init()`'ten sonra çağrılmalı.
- [x] **microSD (TF kart) loglama** (`Src/sdcard.c`/`Src/sdlog.c` + `Middlewares/FatFs`)
      — SDIO arayüzü, kart tespiti ve FatFs entegrasyonu.
      - Şematikten doğrulanan pinler: `D0=PC8 D1=PC9 D2=PC10 D3=PC11 CLK=PC12
        CMD=PD2`, kart-algılama `CD=PC7` (R53 pull-up +3.3V, kart takılınca
        switch GND'ye çekiyor → LOW = kart var).
      - `Src/sdcard.c`/`Inc/sdcard.h` — Geehy'nin resmi `bsp_sdio.c`
        sürücüsünün (SDIO komut dizisi, DMA transfer, CSD/CID parse) neredeyse
        birebir portu. Uyarlamalar: header adı değişti, `APM_EVAL_DelayMs`
        yerine projenin kendi tick-tabanlı gecikmesi kullanıldı, printf'li iki
        test-demo fonksiyonu (kullanılmıyordu) kaldırıldı.
      - **Bulunan/Düzeltilen derleme sorunu:** orijinal dosyada `tempbuff` ve
        `SDIO_DATA_BUFFER` değişkenleri sadece `__CC_ARM` (Keil) ve
        `__ICCARM__` (IAR) derleyicileri için tanımlıydı — GCC için hiçbir dal
        yoktu, `tempbuff` derleme hatası veriyordu. GCC için üçüncü bir dal
        eklendi.
      - `Middlewares/FatFs` — ChaN'ın resmi FatFs'i (Geehy'nin SDK'sında
        `Middlewares/fat_fs` altında hazır bulundu). `ffconf.h` bu proje için
        sadeleştirildi: `FF_USE_LFN=0` (uzun dosya adı yok, sadece `LOG.TXT`
        gibi 8.3 kısa adlar), `FF_CODE_PAGE=437`, `FF_VOLUMES=1`,
        `FF_MAX_SS=512` sabit — bu sayede ~2MB'lık Unicode/DBCS tablosu içeren
        `ffunicode.c`'ye hiç ihtiyaç kalmadı.
      - `Src/sdlog.c`/`Inc/sdlog.h` — projeye özel: açılışta kart var mı bak,
        yoksa SDIO'yu hiç başlatma (gereksiz timeout'tan kaçın); varsa
        `SD_Init()` + `f_mount()`. Kart biçimlendirilmemişse (`FR_NO_FILESYSTEM`)
        **otomatik biçimlendirmez** — kullanıcı verisini riske atmamak için,
        menüdeki "SD Kart" sayfasında KEY2'ye üst üste 2 kez basılmasını
        bekler (`SdLog_TryFormat()`), tıpkı Geehy'nin kendi demosundaki
        onay deseni gibi.
      - Durum satırı hem seri porta (500ms) hem SD karta (`LOG.TXT`'e ekleme,
        5000ms — flash aşınmasını azaltmak için seri loglamadan çok daha
        seyrek) yazılıyor.
      - Menüye 4. madde eklendi: "SD Kart" — bağlı/bağlı-değil durumu, boş
        alan (KB), yazma sayısı canlı gösteriliyor.
      - Henüz gerçek zamanlı saat (RTC) bağlanmadığı için dosya zaman damgaları
        sabit (`FF_FS_NORTC=1`) — kartta RTC yedek pil yuvası (CR1220) var,
        ileride bağlanabilir.
      - **Donanımda henüz test edilmedi.**

## Yapılacak / Bilinmeyenler

- [ ] microSD loglama donanımda test bekliyor (kart tespiti, mount, LOG.TXT
      yazımı, biçimlendirme onay akışı).
- [ ] RTC (CR1220 yedek pilli) henüz bağlanmadı — SD dosya zaman damgaları
      şu an sabit.
- [ ] CAN1/CAN2 (TJA1050 transceiver'lar) henüz kullanılmadı.
- [ ] Native USB Device (OTG FS, PA11/PA12) henüz kullanılmadı.
- [ ] SPI NOR Flash (W25Q16, 2MB) ve I2C EEPROM (AT24C02) henüz kullanılmadı.
- [ ] 16MB harici SDRAM (DMC/FSMC arabirimi) henüz kullanılmadı.

## Dökümanlar

`Documents/` klasöründe Geehy'nin resmi sayfasından indirilenler:
- `APM32E103ZE_EVALBOARD_V1.0_Schematic.pdf` — kart şeması (7 sayfa: Power
  Supply, MCU, MCU_Connector, Communication, Memory, Display&Button, JTAG&SWD).
- `APM32E103ZE_EVAL_Board_User_Manual_V1.0.pdf` — kullanım kılavuzu (19 sayfa).
- `APM32E103_Datasheet.pdf` — Geehy'nin SDK deposundaki datasheet dosyası
  (tek sayfa, gerçek datasheet'e yönlendirme linki içeriyor:
  https://geehy.com/support/apm32?id=191).
