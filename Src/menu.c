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
 ******************************************************************************
 */

#include "menu.h"
#include "lcd.h"
#include "temp.h"
#include "sdlog.h"
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

#define MENU_FCOLOR     RGB2RGB565(0, 222, 152)
#define MENU_BCOLOR     LCD_COLOR_WHITE
#define MENU_SEL_FCOLOR LCD_COLOR_WHITE
#define MENU_SEL_BCOLOR MENU_FCOLOR

#define LINE_TITLE 8
#define LINE_0     44
#define LINE_1     70
#define LINE_2     96
#define LINE_3     122
#define LINE_SEP   148
#define LINE_HINT  168
#define LINE_CONTENT 90
#define BOTTOM_TOP 256

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

static const char *s_itemTitle[4] = { "LED Durumu", "Sayac (ms)", "Kart Bilgisi", "SD Kart" };
#define MENU_ITEM_COUNT 4U

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
			s_state = MENU_STATE_HOME;
			DrawHome();
		}
		else
		{
			if ((s_selected == 3U) && k2 && (SdLog_GetState() == SDLOG_NO_FILESYSTEM))
			{
				s_formatArmCount++;
				if (s_formatArmCount >= 2U)
				{
					SdLog_TryFormat();
					s_formatArmCount = 0U;
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

static void DrawItem(uint8_t index, uint8_t selected)
{
	uint16_t y = (index == 0U) ? LINE_0 : ((index == 1U) ? LINE_1 : ((index == 2U) ? LINE_2 : LINE_3));
	char buf[20];

	/* Fixed text per index (never changes) - safe to redraw without a
	 * pre-clear, the new glyphs fully cover the old ones at the same cells. */
	snprintf(buf, sizeof(buf), "%u.%s", (unsigned)(index + 1U), s_itemTitle[index]);
	LCD_DisplayString(10, y, buf, selected ? MENU_SEL_FCOLOR : MENU_FCOLOR, selected ? MENU_SEL_BCOLOR : MENU_BCOLOR, 24);
}

/* Centers a string horizontally within LCD_WIDTH (clamped to x=0 if wider
 * than the screen). */
static void DrawCenteredString(uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint8_t fontSize)
{
	uint16_t textWidth = (uint16_t)(strlen(s) * (fontSize / 2U));
	uint16_t x = (textWidth < LCD_WIDTH) ? (uint16_t)((LCD_WIDTH - textWidth) / 2U) : 0U;

	LCD_DisplayString(x, y, s, fc, bc, fontSize);
}

static void DrawHome(void)
{
	LCD_Clear(0, 0, LCD_WIDTH, LCD_HEIGHT, MENU_BCOLOR);

	LCD_Clear(0, 0, LCD_WIDTH, 36, MENU_FCOLOR);
	DrawCenteredString(LINE_TITLE, "APM32E103ZE MENU", MENU_SEL_FCOLOR, MENU_FCOLOR, 16);

	for (uint8_t i = 0; i < MENU_ITEM_COUNT; i++)
	{
		DrawItem(i, i == s_selected);
	}

	LCD_DrawLine(10, LINE_SEP, LCD_WIDTH - 10, LINE_SEP, MENU_FCOLOR);
	LCD_DisplayString(10, LINE_HINT, "K1:Sec K2:Gir K3:Geri", LCD_COLOR_BLACK, MENU_BCOLOR, 16);

	LCD_Clear(0, BOTTOM_TOP, LCD_WIDTH, LCD_HEIGHT, MENU_FCOLOR);
	LCD_DisplayString(45, BOTTOM_TOP + 6, "APM32 EVAL BOARD", MENU_SEL_FCOLOR, MENU_FCOLOR, 16);
}

static void DrawSub(uint8_t index)
{
	LCD_Clear(0, 0, LCD_WIDTH, LCD_HEIGHT, MENU_BCOLOR);

	LCD_Clear(0, 0, LCD_WIDTH, 36, MENU_FCOLOR);
	DrawCenteredString(LINE_TITLE, s_itemTitle[index], MENU_SEL_FCOLOR, MENU_FCOLOR, 16);

	if (index == 2U)
	{
		LCD_DisplayString(10, LINE_CONTENT, "APM32E103ZE", MENU_FCOLOR, MENU_BCOLOR, 24);
		LCD_DisplayString(10, LINE_CONTENT + 30, "Cortex-M3 @72MHz", MENU_FCOLOR, MENU_BCOLOR, 16);
		LCD_DisplayString(10, LINE_CONTENT + 50, "Flash 512K RAM 128K", MENU_FCOLOR, MENU_BCOLOR, 16);
		LCD_DisplayString(10, LINE_CONTENT + 80, "Sicaklik:", MENU_FCOLOR, MENU_BCOLOR, 24);
	}
	else if (index == 3U)
	{
		s_formatArmCount = 0U;
	}

	LCD_Clear(0, BOTTOM_TOP, LCD_WIDTH, LCD_HEIGHT, MENU_FCOLOR);
	LCD_DisplayString(35, BOTTOM_TOP + 6, "K3: Geri Don", MENU_SEL_FCOLOR, MENU_FCOLOR, 16);

	UpdateSubContent(index);
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
		 * against real hardware - so the text is the inverse of the bit. */
		uint16_t out = GPIO_ReadOutputPort(LED_PORT);

		LCD_DisplayString(10, LINE_CONTENT, "LED1:", MENU_FCOLOR, MENU_BCOLOR, 24);
		LCD_DisplayString(110, LINE_CONTENT, (out & LED1_PIN) ? "OFF" : "ON ", LCD_COLOR_RED, MENU_BCOLOR, 24);

		LCD_DisplayString(10, LINE_CONTENT + 30, "LED2:", MENU_FCOLOR, MENU_BCOLOR, 24);
		LCD_DisplayString(110, LINE_CONTENT + 30, (out & LED2_PIN) ? "OFF" : "ON ", LCD_COLOR_RED, MENU_BCOLOR, 24);

		LCD_DisplayString(10, LINE_CONTENT + 60, "LED3:", MENU_FCOLOR, MENU_BCOLOR, 24);
		LCD_DisplayString(110, LINE_CONTENT + 60, (out & LED3_PIN) ? "OFF" : "ON ", LCD_COLOR_RED, MENU_BCOLOR, 24);
	}
	else if (index == 1U)
	{
		LCD_DisplayString(10, LINE_CONTENT, "Tick:", MENU_FCOLOR, MENU_BCOLOR, 24);
		LCD_DisplayIntNum(110, LINE_CONTENT, (uint16_t)(g_tickMs % 60000U), 5, LCD_COLOR_RED, MENU_BCOLOR, 24);
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

		snprintf(buf, sizeof(buf), "%d.%dC  ", whole, frac);
		LCD_DisplayString(130, LINE_CONTENT + 80, buf, LCD_COLOR_RED, MENU_BCOLOR, 24);
	}
	else if (index == 3U)
	{
		static SdLog_State_T lastDrawnState = (SdLog_State_T)0xFFU;
		SdLog_State_T state = SdLog_GetState();
		const char *stateText;
		char buf[24];

		if (state != lastDrawnState)
		{
			lastDrawnState = state;
			LCD_Clear(10, LINE_CONTENT + 24, LCD_WIDTH - 10, BOTTOM_TOP, MENU_BCOLOR);
		}

		switch (state)
		{
		case SDLOG_NO_CARD:        stateText = "Kart yok       "; break;
		case SDLOG_INIT_FAIL:      stateText = "Baslatma hatasi"; break;
		case SDLOG_NO_FILESYSTEM:  stateText = "Bicimsiz kart  "; break;
		case SDLOG_MOUNT_FAIL:     stateText = "Baglama hatasi "; break;
		case SDLOG_MOUNTED:        stateText = "Hazir - LOG.TXT"; break;
		default:                   stateText = "?              "; break;
		}

		LCD_DisplayString(10, LINE_CONTENT, "Durum:", MENU_FCOLOR, MENU_BCOLOR, 24);
		LCD_DisplayString(130, LINE_CONTENT, stateText, LCD_COLOR_RED, MENU_BCOLOR, 16);

		if (state == SDLOG_MOUNTED)
		{
			snprintf(buf, sizeof(buf), "Bos: %lu KB", (unsigned long)SdLog_GetFreeKB());
			LCD_DisplayString(10, LINE_CONTENT + 30, buf, MENU_FCOLOR, MENU_BCOLOR, 16);

			snprintf(buf, sizeof(buf), "Kayit sayisi: %lu", (unsigned long)SdLog_GetWriteCount());
			LCD_DisplayString(10, LINE_CONTENT + 50, buf, MENU_FCOLOR, MENU_BCOLOR, 16);
		}
		else if (state == SDLOG_NO_FILESYSTEM)
		{
			LCD_DisplayString(10, LINE_CONTENT + 30, "K2'ye 2 kez bas:", MENU_FCOLOR, MENU_BCOLOR, 16);
			LCD_DisplayString(10, LINE_CONTENT + 50, "Bicimlendir (SILER!)", MENU_FCOLOR, MENU_BCOLOR, 16);
		}
	}
}
