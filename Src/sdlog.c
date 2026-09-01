/**
 ******************************************************************************
 * @file    sdlog.c
 * @brief   microSD status logging body.
 ******************************************************************************
 */

#include "sdlog.h"
#include "sdcard.h"
#include "ff.h"
#include "apm32e10x_gpio.h"
#include "apm32e10x_rcm.h"
#include <string.h>

#define CD_PORT   GPIOC
#define CD_PIN    GPIO_PIN_7
#define LOG_PATH  "0:LOG.TXT"

static FATFS s_fatFs;
static SdLog_State_T s_state = SDLOG_NO_CARD;
static uint32_t s_writeCount = 0U;

static uint8_t CardPresent(void)
{
	return (GPIO_ReadInputBit(CD_PORT, CD_PIN) == 0U) ? 1U : 0U;
}

static void TryMount(void)
{
	FRESULT fr = f_mount(&s_fatFs, "0:", 1);

	if (fr == FR_OK)
	{
		s_state = SDLOG_MOUNTED;
	}
	else if (fr == FR_NO_FILESYSTEM)
	{
		s_state = SDLOG_NO_FILESYSTEM;
	}
	else
	{
		s_state = SDLOG_MOUNT_FAIL;
	}
}

void SdLog_Init(void)
{
	GPIO_Config_T gpioConfig;

	RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_GPIOC);

	gpioConfig.pin = CD_PIN;
	gpioConfig.mode = GPIO_MODE_IN_PU;
	gpioConfig.speed = GPIO_SPEED_2MHz;
	GPIO_Config(CD_PORT, &gpioConfig);

	if (!CardPresent())
	{
		s_state = SDLOG_NO_CARD;
		return;
	}

	if (SD_Init() != SD_OK)
	{
		s_state = SDLOG_INIT_FAIL;
		return;
	}

	TryMount();
}

void SdLog_TryFormat(void)
{
	static BYTE s_work[FF_MAX_SS];

	if (s_state != SDLOG_NO_FILESYSTEM)
	{
		return;
	}

	if (f_mkfs("0:", NULL, s_work, sizeof(s_work)) == FR_OK)
	{
		TryMount();
	}
}

void SdLog_Append(const char *line)
{
	FIL file;
	UINT written;

	if (s_state != SDLOG_MOUNTED)
	{
		return;
	}

	if (f_open(&file, LOG_PATH, FA_OPEN_APPEND | FA_WRITE) != FR_OK)
	{
		return;
	}

	f_write(&file, line, (UINT)strlen(line), &written);
	f_close(&file);
	s_writeCount++;
}

SdLog_State_T SdLog_GetState(void)
{
	return s_state;
}

uint32_t SdLog_GetWriteCount(void)
{
	return s_writeCount;
}

uint32_t SdLog_GetFreeKB(void)
{
	FATFS *fs;
	DWORD freeClusters;

	if (s_state != SDLOG_MOUNTED)
	{
		return 0U;
	}

	if (f_getfree("0:", &freeClusters, &fs) != FR_OK)
	{
		return 0U;
	}

	return (uint32_t)((freeClusters * fs->csize) / 2U); /* 512B sectors -> KB */
}
