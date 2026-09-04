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
      `KEY3=PA0` (aktif-yüksek, pull-down).
      SDK'nın TMR7-kesmesi + blocking-delay yöntemi yerine, UniBoard'daki
      power-button ile aynı non-blocking tick-tabanlı debounce kullanıldı.
      5 madde: LED Durumu, Sayac, Kart Bilgisi, SD Kart, SPI Flash.
      **Donanımda test edildi (2026-09-01), çalışıyor.**
      - **Görsel tasarım (2026-09-02 güncellendi):** tek vurgu rengi (koyu
        camgöbeği) hem marka hem "iyi/nötr" durum anlamına geliyor, kırmızı
        sadece hatalar için ayrılmış. Seçili madde artık sadece metin rengi
        değil, tam genişlikte dolu bir şerit ile vurgulanıyor. Numaralandırma
        ve ayırıcı çizgi kaldırıldı — sadelik için gereksizdi.
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
      - **Donanımda test edildi (2026-09-01), çalışıyor:** kart tespit edildi,
        `f_mount` başarılı, seri log ile SD karttaki `LOG.TXT`'e yazılan kayıt
        sayısı ve menüdeki "SD Kart" sayfasındaki canlı sayaç birebir eşleşti
        (7 kayıt).
- [x] **SPI NOR Flash** (`Src/spiflash.c`/`Inc/spiflash.h`) — W25Q16 (2MB),
      Geehy'nin `bsp_w25q16.c`'sinden portlandı, sadece kullanılan
      fonksiyonlarla (JEDEC ID, sektör silme, sayfa yazma, okuma) sadeleştirildi.
      SPI3 pinleri (`CS=PA15 SCK=PB3 MISO=PB4 MOSI=PB5`) JTAG ile çakışıyor
      (TDI/TDO/TRST) — `GPIO_ConfigPinRemap(GPIO_REMAP_SWJ_JTAGDISABLE)` ile
      JTAG kapatılıp SWD (ST-Link ile flaşlama) açık bırakıldı. Menüdeki
      "SPI Flash" sayfasında KEY2 ile JEDEC ID okuma + yaz/oku doğrulama testi
      çalışıyor. **Donanımda test edildi (2026-09-02), çalışıyor.**
- [x] **Demo (sekip duran logo)** (`Src/demo.c`/`Inc/demo.h`) — "DVD ekran
      koruyucusu" tarzı, her duvara çarpışta renk değiştiren, içinde "APM32"
      yazan bir kutu. LCD sürücüsünün gerçek zamanlı yeniden çizim hızını
      gösteren küçük bir vitrin.
- [x] **Video oynatma** (`Src/video.c`/`Inc/video.h` + `Inc/video_data.h`) —
      kullanıcının kendi mp4'ünden (Avatar: The Last Airbender, "Aang vs.
      Ozai" final sahnesi, **tam uzunluk, ~812 saniye**), **yatay (280x240,
      native çözünürlük), 12 fps** döngüsel oynatım, durdur/duraklat
      kontrolleriyle (KEY2=duraklat/devam, KEY3=durdur+geri). Host tarafında
      Python + imageio/ffmpeg ile "cover" kırpma yapılıp RGB565'e çevriliyor.
      - **Depolama:** ilk denemede kareler MCU flash'ına C dizisi olarak
        gömülüyordu (512KB'lık flash'ta ancak birkaç saniyeye yer vardı).
        Kareler artık **microSD karta** (`0:VIDEO.BIN`, host'tan
        `extract_native.py` scripti ile doğrudan karta yazılıyor) konup
        FatFs ile akış halinde okunuyor, flash sadece ~85KB kullanıyor.
      - **Yatay döndürme:** `LCD_SetOrientation()` (ST7789 MADCTL'nin MV
        bitiyle) ile aynı fiziksel panel, video sayfasında 90° döndürülüp
        menüye dönünce dikeye geri alınıyor (`lcd.c`).
      - **Performans serüveni** (hepsi donanımda ölçüldü, `lcd.c`):
        1) Metin/menü yolundaki `LCD_WriteData` her byte için CS pinini
           ayrı aç/kapatıyor → tam kare (134KB) için **352ms**.
        2) Sadece TXBE (BSY değil) bekleyen "hızlı" byte yazımı → **244ms**
           (hâlâ CPU'nun byte-byte pollediği, boru hattı kurulmamış bir yol).
        3) **DMA1 Kanal3 + SPI'nin geçici olarak 16-bit çerçeve moduna
           alınması** (`LCD_BlitBegin/Pixels/End`) — CPU'yu döngüden tamamen
           çıkarıp donanımın kendi hızında akıtmasını sağlıyor → **~28ms**
           (teorik SPI limitine çok yakın). Menüdeki metin çizimi hâlâ eski
           (yavaş ama basit) yolu kullanıyor, sadece video/demo/canlı yayın
           blit yolu DMA'ya taşındı — regresyon riski yok.
      - **SD kart notu:** kullanılan 32GB etiketli kart aslında sahte
        (gerçek kapasitesi ~500MB) çıktı — yazma sırasında 3 kare noktasında
        (baş/orta/son) MD5 checksum ile yaz-sonra-oku doğrulaması yapılarak
        sessiz veri bozulması olmadığı teyit edildi.
      - **Donanımda test edildi (2026-09-03), akıcı çalışıyor.**
- [x] **Canlı Yayın** (`Src/livestream.c`/`Inc/livestream.h` +
      `tools/livestream_send.py`) — PC ekranını (tam ekran veya seçilen bir
      dikdörtgen, örn. bir tarayıcı penceresi) gerçek zamanlı olarak karta
      yansıtıyor. Aynı USART1/CH340 hattı (durum logunun kullandığı port)
      üzerinden akıyor, baud 115200 → 921600 → **2.000.000**'e çıkarıldı
      (72MHz PCLK2'yi tam bölüyor, sıfır hata payı).
      - **Mimari:** USART1 RX, DMA1 Kanal5 ile döngüsel bir halka tampona
        akıyor (klasik STM32F1 eşlemesi); `LiveStream_Step()` bu tamponu
        byte byte tarayan bir senkron-işareti + başlık ayrıştırıcısı,
        gelen veriyi video.c'deki gibi küçük parçalar (chunk) hâlinde
        doğrudan `LCD_BlitBegin/Pixels/End` DMA yoluna akıtıyor — tüm kareyi
        RAM'de tutmuyor, çözünürlük RAM'le sınırlı değil.
      - **Kalite kararı:** çözünürlük panelin native yatay boyutuyla birebir
        (**280x240, büyütme/küçültme yok**) — bant genişliği kısıtı bu
        yüzden çözünürlükten değil, kare hızından kısılıyor.
      - **Gerçek darboğaz:** SD kart üzerinden okunan video (SDIO, saniyede
        birkaç MB) ile kıyaslanınca USART/CH340 hattı 10-30 kat daha yavaş
        (ölçülen gerçek verim ~150KB/s, nominal 2Mbaud'un altında — CH340'ın
        USB paket yükü nedeniyle). Bu yüzden tam kare native çözünürlükte
        ~1-1.5fps'e sıkışıyor.
      - **Delta (fark) kodlama:** bu darboğazı aşmak için PC tarafı her
        yakalamayı bir önceki kareyle karşılaştırıp **sadece değişen
        piksellerin sınırlayıcı dikdörtgenini** gönderiyor — masaüstü/metin
        gibi çoğunlukla durağan içerikte kare hızı belirgin artıyor, tam
        hareketli video gibi içerikte otomatik olarak tam kareye geri
        düşüyor. Bozulmaya karşı her 30 güncellemede bir zorunlu tam kare
        (self-healing keyframe) gönderiliyor. Tel protokolü:
        `sync(4B) + x1,y1,w,h(uint16 LE) + w*h*2 byte RGB565`.
      - **Bulunan/Düzeltilen sorunlar:**
        1) İlk sürümde her çıktı satırı ayrı `LCD_BlitPixels` çağrısıyla
           (240 çağrı/kare) gönderiliyordu — DMA kurulum yükü 240 kat
           tekrarlanınca blit süresi ciddi uzuyor, bu da 4KB'lık halka
           tamponunu her karede taşırıp donma/bozulmaya yol açıyordu. Çözüm:
           video.c'deki gibi büyük parça (4KB) bazlı akış mimarisine geçildi.
        2) PC tarafında piksel byte sırası yanlıştı (`>u2` büyük-uç yazılmıştı,
           MCU küçük-uç `uint16_t*` olarak okuyor) — renkler karışık
           görünüyordu. `extract_native.py`'nin (video.c'nin çalışan
           kaynağı) kullandığı doğal (küçük-uç) sıraya (`<u2`) çevrilerek
           düzeltildi.
      - **Donanımda test edildi (2026-09-04), delta kodlamayla akıcı ve
        renkleri doğru çalışıyor.**

### Denenip geri alınanlar

- **I2C EEPROM (AT24C02)** ve **harici SDRAM (DMC)** eklenip test edildi,
  ikisi de donanımda çalışmadı (EEPROM testi "Başarısız" dönüyordu, SDRAM
  testi kartı kilitliyordu) — kullanıcı isteğiyle firmware'den tamamen
  çıkarıldı (2026-09-02). Not: SDRAM ile SD kart (SDIO) tam olarak aynı 3
  MCU pinini paylaşıyor (`PC10/PC11/PD2` — DMC'nin DQ8/DQ9/DQ10'u, SDIO'nun
  D2/D3/CMD'siyle aynı iz), şemadan doğrulandı — ikisi asla aynı anda aktif
  olamaz, ileride SDRAM'e dönülürse bu unutulmamalı.

## Yapılacak / Bilinmeyenler

- [ ] RTC (CR1220 yedek pilli) henüz bağlanmadı — SD dosya zaman damgaları
      şu an sabit.
- [ ] CAN1/CAN2 (TJA1050 transceiver'lar) henüz kullanılmadı.
- [ ] Native USB Device (OTG FS, PA11/PA12) henüz kullanılmadı.
- [ ] I2C EEPROM (AT24C02) ve harici SDRAM (DMC) — bkz. "Denenip geri
      alınanlar", ileride tekrar denenebilir ama kök neden bulunmadan değil.

## Dökümanlar

`Documents/` klasöründe Geehy'nin resmi sayfasından indirilenler:
- `APM32E103ZE_EVALBOARD_V1.0_Schematic.pdf` — kart şeması (7 sayfa: Power
  Supply, MCU, MCU_Connector, Communication, Memory, Display&Button, JTAG&SWD).
- `APM32E103ZE_EVAL_Board_User_Manual_V1.0.pdf` — kullanım kılavuzu (19 sayfa).
- `APM32E103_Datasheet.pdf` — Geehy'nin SDK deposundaki datasheet dosyası
  (tek sayfa, gerçek datasheet'e yönlendirme linki içeriyor:
  https://geehy.com/support/apm32?id=191).
