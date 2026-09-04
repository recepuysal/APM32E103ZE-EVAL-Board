/**
 ******************************************************************************
 * @file    menu.c
 * @brief   KEY1/KEY2/KEY3-driven LCD menu body.
 *
 * Navigation matches Geehy's official SPI_LCD demo (bsp_key.c/main.c,
 * GeehySemi/APM32E10x_EVAL_SDK): KEY1 cycles the highlighted item, KEY2
 * enters it, KEY3 returns to the home screen. Button pins/polarity
 * confirmed from the SDK's Board_APM32E103_EVAL.h:
 *   KEY1=PF9 (active low, pull-up) KEY2=PC13 (active low, pull-up)
 *   KEY3=PA0 (active high, pull-down)
 * Debounced with a non-blocking tick-based edge detector (same pattern as
 * UniBoard's power-button handling), not the SDK's TMR7-ISR + blocking-delay
 * approach.
 *
 * Visual style: flat accent-color title/footer bars, full-width selection
 * rows (no numbering - the highlighted bar is the only affordance needed),
 * one accent color for "good"/neutral values and red reserved for failures.
 ******************************************************************************
 */

#include "menu.h"
#include "lcd.h"
#include "temp.h"
#include "sdlog.h"
#include "spiflash.h"
#include "demo.h"
#include "video.h"
#include "livestream.h"
#include "apm32e10x_gpio.h"
#include "apm32e10x_rcm.h"
#include <stdio.h>
#include <string.h>

#define KEY1_PORT GPIOF
#define KEY1_PIN  GPIO_PIN_9
#define KEY2_PORT GPIOC
#define KEY2_PIN  GPIO_PIN_13
#define KEY3_PORT GPIOA
#define KEY3_PIN  GPIO_PIN_0

#define LED_PORT  GPIOD
#define LED1_PIN  GPIO_PIN_13
#define LED2_PIN  GPIO_PIN_14
#define LED3_PIN  GPIO_PIN_15

#define KEY_DEBOUNCE_MS 30U

/* Palette: one accent color carries both branding and "good/neutral" state;
 * red is reserved for failures only, so it always means something. */
#define COLOR_ACCENT RGB2RGB565(0, 150, 136)
#define COLOR_TEXT   RGB2RGB565(33, 33, 33)
#define COLOR_MUTED  RGB2RGB565(150, 150, 150)
#define COLOR_BG     LCD_COLOR_WHITE
#define COLOR_WHITE  LCD_COLOR_WHITE
#define COLOR_FAIL   LCD_COLOR_RED

#define TITLE_HEIGHT 38U
#define ROW_Y_BASE   40U
#define ROW_HEIGHT   24U
#define ROW_STEP     26U
#define ROW_MARGIN   12U
#define FOOTER_TOP   250U
#define LINE_CONTENT 54U

extern volatile uint32_t g_tickMs;

typedef struct
{
	GPIO_T *port;
	uint16_t pin;
	uint8_t activeLow;
	uint8_t pressedState;
	uint32_t lastChangeTick;
} Key_T;

static Key_T s_key1 = { KEY1_PORT, KEY1_PIN, 1U, 0U, 0U };
static Key_T s_key2 = { KEY2_PORT, KEY2_PIN, 1U, 0U, 0U };
static Key_T s_key3 = { KEY3_PORT, KEY3_PIN, 0U, 0U, 0U };

static const char *s_itemTitle[8] =
{
	"LED Durumu", "Sayac", "Kart Bilgisi", "SD Kart", "SPI Flash", "Demo",
	"Video", "Canli Yayin"
};
#define MENU_ITEM_COUNT 8U

typedef enum
{
	TEST_NONE,
	TEST_PASS,
	TEST_FAIL,
} TestResult_T;

static TestResult_T s_flashResult = TEST_NONE;
static uint32_t s_flashId = 0U;

typedef enum
{
	MENU_STATE_HOME,
	MENU_STATE_SUB,
} MenuState_T;

static MenuState_T s_state = MENU_STATE_HOME;
static uint8_t s_selected = 0U;
static uint8_t s_formatArmCount = 0U; /* KEY2 presses while viewing an unformatted SD card - 2 required */

static void KeyGpioInit(void);
static uint8_t KeyPressedEdge(Key_T *key);
static void DrawHome(void);
static void DrawItem(uint8_t index, uint8_t selected);
static void DrawSub(uint8_t index);
static void UpdateSubContent(uint8_t index);
static void DrawCenteredString(uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint8_t fontSize);
static void DrawResult(uint16_t y, TestResult_T result);
static void RunFlashTest(void);

void Menu_Init(void)
{
	KeyGpioInit();
	DrawHome();
}

void Menu_Poll(void)
{
	uint8_t k1 = KeyPressedEdge(&s_key1);
	uint8_t k2 = KeyPressedEdge(&s_key2);
	uint8_t k3 = KeyPressedEdge(&s_key3);

	if (s_state == MENU_STATE_HOME)
	{
		if (k1)
		{
			uint8_t prev = s_selected;
			s_selected = (uint8_t)((s_selected + 1U) % MENU_ITEM_COUNT);
			DrawItem(prev, 0U);
			DrawItem(s_selected, 1U);
		}
		else if (k2)
		{
			s_state = MENU_STATE_SUB;
			DrawSub(s_selected);
		}
	}
	else
	{
		if (k3)
		{
			if (s_selected == 6U)
			{
				Video_Stop();
			}
			else if (s_selected == 7U)
			{
				LiveStream_Stop();
			}

			s_state = MENU_STATE_HOME;
			DrawHome();
		}
		else if (s_selected == 5U)
		{
			Demo_Step(); /* free-running animation - not gated by K2 or the content refresh throttle */
		}
		else if (s_selected == 6U)
		{
			if (k2)
			{
				Video_TogglePause();
			}

			Video_Step(); /* no-ops internally while paused */
		}
		else if (s_selected == 7U)
		{
			LiveStream_Step();
		}
		else
		{
			if (k2)
			{
				if (s_selected == 3U)
				{
					if (SdLog_GetState() == SDLOG_NO_FILESYSTEM)
					{
						s_formatArmCount++;
						if (s_formatArmCount >= 2U)
						{
							SdLog_TryFormat();
							s_formatArmCount = 0U;
						}
					}
				}
				else if (s_selected == 4U)
				{
					RunFlashTest();
				}
			}

			UpdateSubContent(s_selected);
		}
	}
}

static void KeyGpioInit(void)
{
	GPIO_Config_T gpioConfig;

	RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_GPIOF | RCM_APB2_PERIPH_GPIOC | RCM_APB2_PERIPH_GPIOA);

	gpioConfig.speed = GPIO_SPEED_50MHz;

	gpioConfig.pin = KEY1_PIN;
	gpioConfig.mode = GPIO_MODE_IN_PU;
	GPIO_Config(KEY1_PORT, &gpioConfig);

	gpioConfig.pin = KEY2_PIN;
	gpioConfig.mode = GPIO_MODE_IN_PU;
	GPIO_Config(KEY2_PORT, &gpioConfig);

	gpioConfig.pin = KEY3_PIN;
	gpioConfig.mode = GPIO_MODE_IN_PD;
	GPIO_Config(KEY3_PORT, &gpioConfig);
}

/* Returns 1 once on the press transition (not while held), debounced. */
static uint8_t KeyPressedEdge(Key_T *key)
{
	uint8_t raw = (GPIO_ReadInputBit(key->port, key->pin) == 0U) ? 0U : 1U;
	uint8_t rawPressed = key->activeLow ? (raw == 0U) : (raw == 1U);

	if ((rawPressed != key->pressedState) && ((g_tickMs - key->lastChangeTick) > KEY_DEBOUNCE_MS))
	{
		key->pressedState = rawPressed;
		key->lastChangeTick = g_tickMs;
		return rawPressed;
	}

	return 0U;
}

/* Centers a string horizontally within LCD_WIDTH (clamped to x=0 if wider
 * than the screen). */
static void DrawCenteredString(uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint8_t fontSize)
{
	uint16_t textWidth = (uint16_t)(strlen(s) * (fontSize / 2U));
	uint16_t x = (textWidth < LCD_WIDTH) ? (uint16_t)((LCD_WIDTH - textWidth) / 2U) : 0U;

	LCD_DisplayString(x, y, s, fc, bc, fontSize);
}

/* A full-width rounded-feel row: selection is a solid accent bar, not just
 * recolored text, so the current item reads clearly at a glance. */
static void DrawItem(uint8_t index, uint8_t selected)
{
	uint16_t top = (uint16_t)(ROW_Y_BASE + index * ROW_STEP);
	uint16_t bg = selected ? COLOR_ACCENT : COLOR_BG;
	uint16_t fg = selected ? COLOR_WHITE : COLOR_TEXT;

	LCD_Clear(ROW_MARGIN, top, LCD_WIDTH - ROW_MARGIN, (uint16_t)(top + ROW_HEIGHT), bg);
	LCD_DisplayString(ROW_MARGIN + 12U, top, s_itemTitle[index], fg, bg, 24);
}

static void DrawHome(void)
{
	LCD_Clear(0, 0, LCD_WIDTH, LCD_HEIGHT, COLOR_BG);

	LCD_Clear(0, 0, LCD_WIDTH, TITLE_HEIGHT, COLOR_ACCENT);
	DrawCenteredString(11, "APM32E103ZE", COLOR_WHITE, COLOR_ACCENT, 16);

	for (uint8_t i = 0; i < MENU_ITEM_COUNT; i++)
	{
		DrawItem(i, i == s_selected);
	}

	LCD_Clear(0, FOOTER_TOP, LCD_WIDTH, LCD_HEIGHT, COLOR_ACCENT);
	DrawCenteredString(FOOTER_TOP + 7U, "Sec / Ac / Geri", COLOR_WHITE, COLOR_ACCENT, 16);
}

static void DrawSub(uint8_t index)
{
	LCD_Clear(0, 0, LCD_WIDTH, LCD_HEIGHT, COLOR_BG);

	LCD_Clear(0, 0, LCD_WIDTH, TITLE_HEIGHT, COLOR_ACCENT);
	DrawCenteredString(11, s_itemTitle[index], COLOR_WHITE, COLOR_ACCENT, 16);

	if (index == 2U)
	{
		LCD_DisplayString(20, LINE_CONTENT, "APM32E103ZE", COLOR_TEXT, COLOR_BG, 24);
		LCD_DisplayString(20, LINE_CONTENT + 26U, "Cortex-M3 @72MHz", COLOR_MUTED, COLOR_BG, 16);
		LCD_DisplayString(20, LINE_CONTENT + 44U, "Flash 512K  RAM 128K", COLOR_MUTED, COLOR_BG, 16);
		LCD_DisplayString(20, LINE_CONTENT + 74U, "Sicaklik", COLOR_TEXT, COLOR_BG, 24);
	}
	else if (index == 3U)
	{
		s_formatArmCount = 0U;
	}
	else if (index == 4U)
	{
		s_flashResult = TEST_NONE;
		s_flashId = 0U;
		LCD_DisplayString(20, LINE_CONTENT, "K2 ile testi baslat", COLOR_MUTED, COLOR_BG, 16);
	}

	LCD_Clear(0, FOOTER_TOP, LCD_WIDTH, LCD_HEIGHT, COLOR_ACCENT);
	DrawCenteredString(FOOTER_TOP + 7U, "Geri", COLOR_WHITE, COLOR_ACCENT, 16);

	if (index == 5U)
	{
		Demo_Start(); /* draws its own first frame over the content area */
	}
	else if (index == 6U)
	{
		Video_Start();
	}
	else if (index == 7U)
	{
		LiveStream_Start();
	}
	else
	{
		UpdateSubContent(index);
	}
}

static void DrawResult(uint16_t y, TestResult_T result)
{
	const char *text;
	uint16_t color;

	switch (result)
	{
	case TEST_PASS: text = "Basarili   "; color = COLOR_ACCENT; break;
	case TEST_FAIL: text = "Basarisiz  "; color = COLOR_FAIL;   break;
	default:        text = "-          "; color = COLOR_MUTED;  break;
	}

	LCD_DisplayString(20, y, text, color, COLOR_BG, 16);
}

static void RunFlashTest(void)
{
	uint8_t wbuf[16];
	uint8_t rbuf[16];
	uint8_t ok;

	for (uint8_t i = 0; i < sizeof(wbuf); i++)
	{
		wbuf[i] = (uint8_t)(i ^ 0xA5U);
	}

	s_flashId = SPIFlash_ReadJedecID();
	ok = (s_flashId == W25Q16_JEDEC_ID) ? 1U : 0U;

	if (ok)
	{
		SPIFlash_EraseSector(0x000000U);
		SPIFlash_WriteBuffer(wbuf, 0x000000U, sizeof(wbuf));
		SPIFlash_ReadBuffer(rbuf, 0x000000U, sizeof(rbuf));

		for (uint8_t i = 0; i < sizeof(wbuf); i++)
		{
			if (rbuf[i] != wbuf[i])
			{
				ok = 0U;
			}
		}
	}

	s_flashResult = ok ? TEST_PASS : TEST_FAIL;
}

static void UpdateSubContent(uint8_t index)
{
	static uint32_t lastRefresh = 0U;

	if ((g_tickMs - lastRefresh) < 200U)
	{
		return;
	}
	lastRefresh = g_tickMs;

	if (index == 0U)
	{
		/* GPIO pin HIGH -> LED physically off (active-low drive), confirmed
		 * against real hardware. */
		uint16_t out = GPIO_ReadOutputPort(LED_PORT);
		static const char *names[3] = { "LED1", "LED2", "LED3" };
		uint16_t pins[3] = { LED1_PIN, LED2_PIN, LED3_PIN };

		for (uint8_t i = 0; i < 3U; i++)
		{
			uint16_t y = (uint16_t)(LINE_CONTENT + i * 34U);
			uint8_t on = (out & pins[i]) ? 0U : 1U;

			LCD_DisplayString(20, y, names[i], COLOR_TEXT, COLOR_BG, 24);
			LCD_DisplayString(130, y, on ? "Acik " : "Kapali", on ? COLOR_ACCENT : COLOR_MUTED, COLOR_BG, 24);
		}
	}
	else if (index == 1U)
	{
		LCD_DisplayString(20, LINE_CONTENT, "Gecen sure", COLOR_TEXT, COLOR_BG, 16);
		LCD_DisplayIntNum(20, LINE_CONTENT + 24U, (uint16_t)(g_tickMs % 60000U), 5, COLOR_ACCENT, COLOR_BG, 24);
		LCD_DisplayString(140, LINE_CONTENT + 24U, "ms", COLOR_MUTED, COLOR_BG, 24);
	}
	else if (index == 2U)
	{
		char buf[16];
		float celsius = Temp_ReadCelsius();
		int whole = (int)celsius;
		int frac = (int)((celsius - (float)whole) * 10.0f);

		if (frac < 0)
		{
			frac = -frac;
		}

		snprintf(buf, sizeof(buf), "%d.%dC ", whole, frac);
		LCD_DisplayString(138, LINE_CONTENT + 74U, buf, COLOR_ACCENT, COLOR_BG, 24);
	}
	else if (index == 3U)
	{
		static SdLog_State_T lastDrawnState = (SdLog_State_T)0xFFU;
		SdLog_State_T state = SdLog_GetState();
		const char *stateText;
		uint16_t stateColor;
		char buf[24];

		if (state != lastDrawnState)
		{
			lastDrawnState = state;
			LCD_Clear(20, LINE_CONTENT + 24U, LCD_WIDTH - 20, FOOTER_TOP, COLOR_BG);
		}

		switch (state)
		{
		case SDLOG_NO_CARD:       stateText = "Kart yok       "; stateColor = COLOR_MUTED; break;
		case SDLOG_INIT_FAIL:     stateText = "Baslatma hatasi"; stateColor = COLOR_FAIL;  break;
		case SDLOG_NO_FILESYSTEM: stateText = "Bicimsiz kart  "; stateColor = COLOR_FAIL;  break;
		case SDLOG_MOUNT_FAIL:    stateText = "Baglama hatasi "; stateColor = COLOR_FAIL;  break;
		case SDLOG_MOUNTED:       stateText = "Hazir          "; stateColor = COLOR_ACCENT; break;
		default:                  stateText = "?              "; stateColor = COLOR_MUTED; break;
		}

		LCD_DisplayString(20, LINE_CONTENT, "Durum", COLOR_TEXT, COLOR_BG, 16);
		LCD_DisplayString(100, LINE_CONTENT, stateText, stateColor, COLOR_BG, 16);

		if (state == SDLOG_MOUNTED)
		{
			snprintf(buf, sizeof(buf), "Bos alan: %lu KB", (unsigned long)SdLog_GetFreeKB());
			LCD_DisplayString(20, LINE_CONTENT + 26U, buf, COLOR_MUTED, COLOR_BG, 16);

			snprintf(buf, sizeof(buf), "Kayit: %lu", (unsigned long)SdLog_GetWriteCount());
			LCD_DisplayString(20, LINE_CONTENT + 46U, buf, COLOR_MUTED, COLOR_BG, 16);
		}
		else if (state == SDLOG_NO_FILESYSTEM)
		{
			LCD_DisplayString(20, LINE_CONTENT + 26U, "K2'ye 2 kez bas:", COLOR_TEXT, COLOR_BG, 16);
			LCD_DisplayString(20, LINE_CONTENT + 46U, "bicimlendirir (siler)", COLOR_MUTED, COLOR_BG, 16);
		}
	}
	else if (index == 4U)
	{
		char buf[24];

		LCD_DisplayString(20, LINE_CONTENT + 26U, "Sonuc", COLOR_TEXT, COLOR_BG, 16);
		DrawResult(LINE_CONTENT + 46U, s_flashResult);

		if (s_flashResult != TEST_NONE)
		{
			snprintf(buf, sizeof(buf), "JEDEC ID: 0x%06lX", (unsigned long)s_flashId);
			LCD_DisplayString(20, LINE_CONTENT + 66U, buf, COLOR_MUTED, COLOR_BG, 16);
		}
	}
}
