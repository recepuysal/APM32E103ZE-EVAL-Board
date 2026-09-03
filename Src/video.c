/**
 ******************************************************************************
 * @file    video.c
 * @brief   SD-card-streamed video playback body (see video.h).
 ******************************************************************************
 */

#include "video.h"
#include "video_data.h"
#include "lcd.h"
#include "serial.h"
#include "ff.h"
#include <stdio.h>

#define FRAME_BYTES  (VIDEO_FRAME_W * VIDEO_FRAME_H * 2UL)
#define CHUNK_PIXELS 8192U /* 16KB read chunks - was 2KB; fewer, bigger f_read() calls per frame */
#define STEP_MS      (1000U / VIDEO_FPS)

extern volatile uint32_t g_tickMs;

static FIL s_file;
static uint8_t s_ok;
static uint8_t s_paused;
static uint16_t s_frameIndex;
static uint32_t s_lastStep;
static uint16_t s_chunkBuf[CHUNK_PIXELS];

/* Timed separately so we know whether the SD card or the LCD SPI bus is the
 * bottleneck, instead of guessing - printed once per frame while the video
 * page is open. */
static void DrawNextFrame(void)
{
	uint32_t remaining = FRAME_BYTES;
	uint32_t sdMs = 0U;
	uint32_t lcdMs = 0U;
	uint32_t t0;
	char msg[64];

	LCD_BlitBegin(0, 0, (uint16_t)(VIDEO_FRAME_W - 1U), (uint16_t)(VIDEO_FRAME_H - 1U));

	while (remaining > 0U)
	{
		UINT chunkBytes = (remaining < sizeof(s_chunkBuf)) ? (UINT)remaining : (UINT)sizeof(s_chunkBuf);
		UINT bytesRead;

		t0 = g_tickMs;
		if ((f_read(&s_file, s_chunkBuf, chunkBytes, &bytesRead) != FR_OK) || (bytesRead == 0U))
		{
			break; /* short read - leave the rest of the window as-is rather than hang */
		}
		sdMs += (g_tickMs - t0);

		t0 = g_tickMs;
		LCD_BlitPixels(s_chunkBuf, bytesRead / 2U);
		lcdMs += (g_tickMs - t0);

		remaining -= bytesRead;
	}

	LCD_BlitEnd();

	snprintf(msg, sizeof(msg), "VIDEO sd=%lums lcd=%lums total=%lums\r\n",
	         (unsigned long)sdMs, (unsigned long)lcdMs, (unsigned long)(sdMs + lcdMs));
	Serial_Print(msg);
}

void Video_Start(void)
{
	LCD_SetOrientation(1); /* landscape - see lcd.h/lcd.c; restored on Video_Stop() */

	s_frameIndex = 0U;
	s_lastStep = g_tickMs;
	s_paused = 0U;
	s_ok = (f_open(&s_file, "0:VIDEO.BIN", FA_READ) == FR_OK) ? 1U : 0U;

	if (s_ok)
	{
		DrawNextFrame();
	}
	else
	{
		LCD_Clear(0, 0, VIDEO_FRAME_W, VIDEO_FRAME_H, LCD_COLOR_BLACK);
		LCD_DisplayString(20, 130, "VIDEO.BIN bulunamadi", LCD_COLOR_WHITE, LCD_COLOR_BLACK, 16);
	}
}

void Video_Step(void)
{
	if (!s_ok || s_paused)
	{
		return;
	}

	if ((g_tickMs - s_lastStep) < STEP_MS)
	{
		return;
	}
	s_lastStep = g_tickMs;

	s_frameIndex++;
	if (s_frameIndex >= VIDEO_FRAME_COUNT)
	{
		s_frameIndex = 0U;
		f_lseek(&s_file, 0);
	}

	DrawNextFrame();
}

void Video_Stop(void)
{
	if (s_ok)
	{
		f_close(&s_file);
		s_ok = 0U;
	}

	LCD_SetOrientation(0); /* back to portrait for the menu */
}

void Video_TogglePause(void)
{
	if (!s_ok)
	{
		return;
	}

	s_paused = !s_paused;

	if (s_paused)
	{
		/* Small readable badge over whatever's on screen; the next
		 * DrawNextFrame() on resume overwrites the whole frame anyway, so
		 * nothing needs to erase this explicitly. */
		LCD_Clear(8, 8, 140, 32, LCD_COLOR_BLACK);
		LCD_DisplayString(12, 12, "DURAKLATILDI", LCD_COLOR_WHITE, LCD_COLOR_BLACK, 16);
	}
	else
	{
		s_lastStep = g_tickMs; /* don't fast-forward through the paused duration */
	}
}
