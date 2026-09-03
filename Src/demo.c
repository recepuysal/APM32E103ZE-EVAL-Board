/**
 ******************************************************************************
 * @file    demo.c
 * @brief   Bouncing-box animation body (see demo.h).
 ******************************************************************************
 */

#include "demo.h"
#include "lcd.h"

#define BOX_W 70U
#define BOX_H 34U
#define MIN_X 14
#define MAX_X ((int16_t)(LCD_WIDTH - 14 - BOX_W))
#define MIN_Y 44
#define MAX_Y ((int16_t)(250 - 8 - BOX_H))
#define STEP_MS 30U /* ~33 redraws/sec */

#define BG_COLOR LCD_COLOR_WHITE
#define TEXT_COLOR LCD_COLOR_WHITE

static const uint16_t s_palette[] =
{
	RGB2RGB565(0, 150, 136),   /* teal */
	RGB2RGB565(230, 74, 25),   /* orange */
	RGB2RGB565(94, 53, 177),   /* purple */
	RGB2RGB565(21, 101, 192),  /* blue */
	RGB2RGB565(216, 27, 96),   /* pink */
	RGB2RGB565(67, 160, 71),   /* green */
};
#define PALETTE_LEN (sizeof(s_palette) / sizeof(s_palette[0]))

extern volatile uint32_t g_tickMs;

static int16_t s_x, s_y;
static int16_t s_dx, s_dy;
static uint8_t s_colorIdx;
static uint32_t s_lastStep;

static void DrawBox(uint16_t color)
{
	LCD_Clear((uint16_t)s_x, (uint16_t)s_y, (uint16_t)(s_x + BOX_W), (uint16_t)(s_y + BOX_H), color);
	LCD_DisplayString((uint16_t)(s_x + 6), (uint16_t)(s_y + 9), "APM32", TEXT_COLOR, color, 16);
}

void Demo_Start(void)
{
	s_x = MIN_X;
	s_y = MIN_Y;
	s_dx = 3;
	s_dy = 2;
	s_colorIdx = 0;
	s_lastStep = g_tickMs;

	DrawBox(s_palette[s_colorIdx]);
}

void Demo_Step(void)
{
	uint8_t bounced = 0U;

	if ((g_tickMs - s_lastStep) < STEP_MS)
	{
		return;
	}
	s_lastStep = g_tickMs;

	DrawBox(BG_COLOR); /* erase at the old position */

	s_x = (int16_t)(s_x + s_dx);
	s_y = (int16_t)(s_y + s_dy);

	if (s_x <= MIN_X || s_x >= MAX_X)
	{
		s_dx = (int16_t)(-s_dx);
		s_x = (s_x <= MIN_X) ? MIN_X : MAX_X;
		bounced = 1U;
	}

	if (s_y <= MIN_Y || s_y >= MAX_Y)
	{
		s_dy = (int16_t)(-s_dy);
		s_y = (s_y <= MIN_Y) ? MIN_Y : MAX_Y;
		bounced = 1U;
	}

	if (bounced)
	{
		s_colorIdx = (uint8_t)((s_colorIdx + 1U) % PALETTE_LEN);
	}

	DrawBox(s_palette[s_colorIdx]);
}
