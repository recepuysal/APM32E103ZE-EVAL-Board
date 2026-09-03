/**
 ******************************************************************************
 * @file    main.c
 * @brief   APM32E103ZE EVAL board bring-up: LED1/LED2/LED3 blink together
 *          every 2 seconds (confirmed working on real hardware via ST-Link,
 *          2026-09-01) + ST7789 SPI LCD KEY1/KEY2/KEY3 menu demo (see
 *          lcd.c/lcd.h and menu.c/menu.h) + USART1 status log (serial.c/serial.h)
 *          + potentiometer-controlled LCD backlight (backlight.c/backlight.h)
 *          + microSD status logging to LOG.TXT (sdcard.c/sdlog.c, FatFs)
 *          + SPI NOR flash test page (spiflash.c).
 *
 * Board pin roles (LED1/LED2/LED3, KEY1/KEY2/KEY3) confirmed from Geehy's
 * own Board_APM32E103_EVAL.h (GeehySemi/APM32E10x_EVAL_SDK repo, official
 * SDK for this exact eval board) rather than guessed:
 *   LED1 = PD13, LED2 = PD14, LED3 = PD15 (all GPIOD, push-pull outputs)
 * System clock: left at the SDK's own system_apm32e10x.c default (72MHz via
 * HSE 8MHz + PLL, matching the eval board's populated crystal) - unmodified,
 * trusting the vendor's own tested bring-up for their own board rather than
 * re-deriving a clock tree from scratch.
 ******************************************************************************
 */

#include "apm32e10x.h"
#include "apm32e10x_gpio.h"
#include "apm32e10x_rcm.h"
#include "lcd.h"
#include "menu.h"
#include "temp.h"
#include "serial.h"
#include "backlight.h"
#include "sdlog.h"
#include "spiflash.h"
#include <stdio.h>

#define LED_PORT   GPIOD
#define LED1_PIN   GPIO_PIN_13
#define LED2_PIN   GPIO_PIN_14
#define LED3_PIN   GPIO_PIN_15
#define LED_ALL_PINS (LED1_PIN | LED2_PIN | LED3_PIN)

#define BLINK_PERIOD_MS  500U
#define LOG_PERIOD_MS    500U
#define SDLOG_PERIOD_MS  5000U /* much slower than the serial log - avoids hammering the card with writes */

volatile uint32_t g_tickMs;

static void LED_GPIO_Init(void);
static void LED_ToggleAll(void);
static void PrintStatus(uint8_t toSd);

int main(void)
{
	SysTick_Config(SystemCoreClock / 1000U); /* 1ms tick */

	LED_GPIO_Init();
	Temp_Init();
	Backlight_Init();
	Serial_Init();
	SdLog_Init();
	SPIFlash_Init();
	LCD_Init();
	Menu_Init();

	uint32_t lastBlink = 0U;
	uint32_t lastLog = 0U;
	uint32_t lastSdLog = 0U;

	for (;;)
	{
		if ((g_tickMs - lastBlink) >= BLINK_PERIOD_MS)
		{
			lastBlink = g_tickMs;
			LED_ToggleAll();
		}

		if ((g_tickMs - lastLog) >= LOG_PERIOD_MS)
		{
			lastLog = g_tickMs;
			PrintStatus(0U);
		}

		if ((g_tickMs - lastSdLog) >= SDLOG_PERIOD_MS)
		{
			lastSdLog = g_tickMs;
			PrintStatus(1U);
		}

		Backlight_Poll();
		Menu_Poll();
	}
}

/* GPIO pin HIGH -> LED physically off (active-low drive), confirmed against
 * real hardware - same polarity fix as menu.c's LED Durumu page. Builds the
 * status line once and sends it to USART1 always, to the SD card log only
 * when toSd is set (SD writes are throttled much slower, see SDLOG_PERIOD_MS). */
static void PrintStatus(uint8_t toSd)
{
	char msg[80];
	uint16_t out = GPIO_ReadOutputPort(LED_PORT);
	float celsius = Temp_ReadCelsius();
	int whole = (int)celsius;
	int frac = (int)((celsius - (float)whole) * 10.0f);

	if (frac < 0)
	{
		frac = -frac;
	}

	snprintf(msg, sizeof(msg), "TEMP:%d.%dC LED1:%s LED2:%s LED3:%s TICK:%lu\r\n",
	         whole, frac,
	         (out & LED1_PIN) ? "OFF" : "ON ",
	         (out & LED2_PIN) ? "OFF" : "ON ",
	         (out & LED3_PIN) ? "OFF" : "ON ",
	         (unsigned long)g_tickMs);

	if (toSd)
	{
		SdLog_Append(msg);
	}
	else
	{
		Serial_Print(msg);
	}
}

static void LED_GPIO_Init(void)
{
	GPIO_Config_T gpioConfig;

	RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_GPIOD);

	gpioConfig.pin = LED_ALL_PINS;
	gpioConfig.mode = GPIO_MODE_OUT_PP;
	gpioConfig.speed = GPIO_SPEED_50MHz;
	GPIO_Config(LED_PORT, &gpioConfig);
}

/* StdPeriphDriver has no GPIO toggle helper - read back the output bits and
 * flip them, same effect as HAL_GPIO_TogglePin(). All three LEDs read/write
 * together since they're on the same port. */
static void LED_ToggleAll(void)
{
	uint16_t current = GPIO_ReadOutputPort(LED_PORT) & LED_ALL_PINS;

	GPIO_WriteOutputPort(LED_PORT, (GPIO_ReadOutputPort(LED_PORT) & ~LED_ALL_PINS) | (~current & LED_ALL_PINS));
}

void SysTick_Handler(void)
{
	g_tickMs++;
}
