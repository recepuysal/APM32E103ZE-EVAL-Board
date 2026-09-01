/**
 ******************************************************************************
 * @file    lcd.h
 * @brief   ST7789 240x320 SPI TFT driver (SPI1 hardware SPI).
 *
 * Pin map matches the eval board's own LCD header, as used by Geehy's
 * official SPI_LCD demo (GeehySemi/APM32E10x_EVAL_SDK, Examples/SPI/SPI_LCD):
 *   MOSI=PA7  SCK=PA5  CS=PA4  DC=PA6  RES=PA1  BLK=PA8   (all GPIOA)
 * If the panel is wired to different pins, update the defines below.
 *
 * NOTE: Geehy's own demo drives an ILI9341 panel, not ST7789 - its init
 * register sequence (0xCF/0xED/0xE8/0xCB/0xEA power-timing regs) is
 * ILI9341-specific and does not apply to ST7789. Everything else (SPI
 * transport, MIPI DCS draw/window commands 0x2A/0x2B/0x2C, font renderer)
 * is controller-agnostic and reused as-is; only LCD_Init()'s register
 * sequence below is written fresh for ST7789.
 ******************************************************************************
 */

#ifndef __LCD_H
#define __LCD_H

#include "apm32e10x.h"

/* 1.69" round-corner ST7789 panel: 240x280 visible area inside a 240x320
 * GRAM, vertically centered -> 20px row offset. If edges still get clipped
 * or content is off-center, adjust LCD_Y_OFFSET/LCD_X_OFFSET in lcd.c. */
#define LCD_WIDTH   240U
#define LCD_HEIGHT  280U

#define RGB2RGB565(R, G, B) ((((R) & 0xF8) << 8) | (((G) & 0xFC) << 3) | (((B) & 0xF8) >> 3))

#define LCD_COLOR_BLACK   RGB2RGB565(0, 0, 0)
#define LCD_COLOR_WHITE   RGB2RGB565(255, 255, 255)
#define LCD_COLOR_RED     RGB2RGB565(255, 0, 0)
#define LCD_COLOR_GREEN   RGB2RGB565(0, 255, 0)
#define LCD_COLOR_BLUE    RGB2RGB565(0, 0, 255)
#define LCD_COLOR_YELLOW  RGB2RGB565(255, 255, 0)
#define LCD_COLOR_CYAN    RGB2RGB565(0, 255, 255)

void LCD_Init(void);
void LCD_Clear(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint16_t color);
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void LCD_DrawCircle(uint16_t x0, uint16_t y0, uint8_t radius, uint16_t color);
void LCD_DisplayString(uint16_t x, uint16_t y, const char *p, uint16_t fc, uint16_t bc, uint8_t fontSize);
void LCD_DisplayIntNum(uint16_t x, uint16_t y, uint16_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t fontSize);
void LCD_DisplayFloatNum(uint16_t x, uint16_t y, float num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t fontSize);

#endif
