/**
 ******************************************************************************
 * @file    lcd.c
 * @brief   ST7789 240x320 SPI TFT driver body.
 *
 * SPI transport, GPIO setup, MIPI DCS window/write commands and the ASCII
 * font renderer are ported from Geehy's official SPI_LCD demo (bsp_lcd.c,
 * GeehySemi/APM32E10x_EVAL_SDK) - those parts are controller-agnostic.
 * LCD_Init()'s register sequence below is written fresh for ST7789 (Geehy's
 * demo targets ILI9341, a different chip with an incompatible power/gamma
 * register map - see lcd.h note).
 ******************************************************************************
 */

#include "lcd.h"
#include "lcd_font.h"
#include "apm32e10x_gpio.h"
#include "apm32e10x_rcm.h"
#include "apm32e10x_spi.h"
#include "apm32e10x_dma.h"

/* Backlight (PA8) is owned by backlight.c (TIM1_CH1 PWM, pot-controlled) -
 * not driven here. */
#define LCD_SPI_BUS        SPI1
#define LCD_SPI_GPIO_PORT  GPIOA
#define LCD_SPI_MOSI_PIN   GPIO_PIN_7
#define LCD_SPI_SCK_PIN    GPIO_PIN_5
#define LCD_SPI_CS_PIN     GPIO_PIN_4
#define LCD_DC_PIN         GPIO_PIN_6
#define LCD_RES_PIN        GPIO_PIN_1

#define LCD_CS_CLR()  GPIO_ResetBit(LCD_SPI_GPIO_PORT, LCD_SPI_CS_PIN)
#define LCD_CS_SET()  GPIO_SetBit(LCD_SPI_GPIO_PORT, LCD_SPI_CS_PIN)
#define LCD_DC_CLR()  GPIO_ResetBit(LCD_SPI_GPIO_PORT, LCD_DC_PIN)
#define LCD_DC_SET()  GPIO_SetBit(LCD_SPI_GPIO_PORT, LCD_DC_PIN)
#define LCD_RES_CLR() GPIO_ResetBit(LCD_SPI_GPIO_PORT, LCD_RES_PIN)
#define LCD_RES_SET() GPIO_SetBit(LCD_SPI_GPIO_PORT, LCD_RES_PIN)

extern volatile uint32_t g_tickMs;

static void LCD_DelayMs(uint32_t ms)
{
	uint32_t target = g_tickMs + ms;

	while (g_tickMs < target)
	{
	}
}

static void LCD_SPI_Init(void)
{
	GPIO_Config_T gpioConfig;
	SPI_Config_T spiConfig;

	RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_GPIOA | RCM_APB2_PERIPH_SPI1);

	gpioConfig.pin = LCD_SPI_MOSI_PIN | LCD_SPI_SCK_PIN;
	gpioConfig.mode = GPIO_MODE_AF_PP;
	gpioConfig.speed = GPIO_SPEED_50MHz;
	GPIO_Config(LCD_SPI_GPIO_PORT, &gpioConfig);

	gpioConfig.pin = LCD_SPI_CS_PIN | LCD_DC_PIN | LCD_RES_PIN;
	gpioConfig.mode = GPIO_MODE_OUT_PP;
	GPIO_Config(LCD_SPI_GPIO_PORT, &gpioConfig);

	LCD_CS_SET();
	LCD_DC_SET();
	LCD_RES_SET();

	SPI_ConfigStructInit(&spiConfig);
	spiConfig.direction = SPI_DIRECTION_2LINES_FULLDUPLEX;
	spiConfig.mode = SPI_MODE_MASTER;
	spiConfig.length = SPI_DATA_LENGTH_8B;
	spiConfig.polarity = SPI_CLKPOL_HIGH;
	spiConfig.phase = SPI_CLKPHA_2EDGE;
	spiConfig.nss = SPI_NSS_SOFT;
	spiConfig.baudrateDiv = SPI_BAUDRATE_DIV_2;
	spiConfig.firstBit = SPI_FIRSTBIT_MSB;
	spiConfig.crcPolynomial = 7;
	SPI_Config(LCD_SPI_BUS, &spiConfig);

	SPI_Enable(LCD_SPI_BUS);
}

static void LCD_SPI_WriteByte(uint8_t data)
{
	uint32_t timeout = 0;

	while (SPI_I2S_ReadStatusFlag(LCD_SPI_BUS, SPI_FLAG_TXBE) == RESET)
	{
		if (++timeout >= 20000U)
		{
			return;
		}
	}

	SPI_I2S_TxData(LCD_SPI_BUS, data);

	timeout = 0;
	while (SPI_I2S_ReadStatusFlag(LCD_SPI_BUS, SPI_FLAG_BSY) == SET)
	{
		if (++timeout >= 20000U)
		{
			return;
		}
	}
}

static void LCD_WriteData(uint8_t data)
{
	LCD_CS_CLR();
	LCD_SPI_WriteByte(data);
	LCD_CS_SET();
}

static void LCD_WriteReg(uint8_t reg)
{
	LCD_DC_CLR();
	LCD_WriteData(reg);
	LCD_DC_SET();
}

static void LCD_WriteHalfword(uint16_t data)
{
	LCD_WriteData((uint8_t)(data >> 8));
	LCD_WriteData((uint8_t)data);
}

static uint32_t LCD_CalPow(uint8_t m, uint8_t n)
{
	uint32_t result = 1U;

	while (n--)
	{
		result *= m;
	}

	return result;
}

/* 240x280 visible window centered in the panel's 240x320 GRAM. */
#define LCD_X_OFFSET 0U
#define LCD_Y_OFFSET 20U

/* Portrait (default) MADCTL: MY|MX - tuned/verified against real hardware
 * earlier (180 deg flip to match physical mounting). Landscape adds MV (row/
 * column exchange) on top - rotates the same physical glass 90 deg. Because
 * MV swaps which register is "row" vs "column", the 20px GRAM crop that
 * normally sits on RASET has to move to CASET whenever MV is active - see
 * LCD_AddressSet(). If the picture comes out mirrored/upside-down in
 * landscape, try MADCTL 0x60 (MX|MV) instead of 0xA0 (MY|MV) below; either
 * way the offset-swap logic here is unaffected since both carry MV. */
#define LCD_MADCTL_PORTRAIT  0xC0U
#define LCD_MADCTL_LANDSCAPE 0xA0U

static uint8_t s_landscape = 0U;

static void LCD_AddressSet(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	uint16_t xOffset = s_landscape ? LCD_Y_OFFSET : LCD_X_OFFSET;
	uint16_t yOffset = s_landscape ? LCD_X_OFFSET : LCD_Y_OFFSET;

	LCD_WriteReg(0x2A);
	LCD_WriteHalfword((uint16_t)(x1 + xOffset));
	LCD_WriteHalfword((uint16_t)(x2 + xOffset));

	LCD_WriteReg(0x2B);
	LCD_WriteHalfword((uint16_t)(y1 + yOffset));
	LCD_WriteHalfword((uint16_t)(y2 + yOffset));

	LCD_WriteReg(0x2C);
}

/* Switches between portrait (menu/text - default) and landscape (video) -
 * call LCD_SetOrientation(1) before drawing in landscape and (0) to restore
 * portrait for the menu afterward. */
void LCD_SetOrientation(uint8_t landscape)
{
	s_landscape = landscape ? 1U : 0U;

	LCD_WriteReg(0x36);
	LCD_WriteData(s_landscape ? LCD_MADCTL_LANDSCAPE : LCD_MADCTL_PORTRAIT);
}

void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
	LCD_AddressSet(x, y, x, y);
	LCD_WriteHalfword(color);
}

void LCD_Clear(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint16_t color)
{
	uint32_t pixelCount = (uint32_t)(xEnd - xStart) * (uint32_t)(yEnd - yStart);

	LCD_AddressSet(xStart, yStart, xEnd - 1U, yEnd - 1U);

	while (pixelCount--)
	{
		LCD_WriteHalfword(color);
	}
}

/* Bulk-pixel path for animation/video. Two prior attempts and why they
 * weren't enough (measured on hardware, 134KB native-res frame):
 *   1) LCD_WriteData toggles CS around every byte - fine for text, 352ms/frame.
 *   2) A "fast" byte writer that only waits TXBE (not BSY) before the next
 *      byte, still CPU-polled one byte at a time - 244ms/frame.
 * Both are CPU-bound on the TXBE/BSY polling loop, nowhere near the SPI
 * clock's actual bit-time budget (~30ms theoretical for 134KB at 36MHz).
 * This version hands the whole burst to DMA1 Channel3 (SPI1_TX) with the
 * SPI temporarily switched to 16-bit frames, so the peripheral clocks
 * pixels out back-to-back with no per-byte CPU involvement at all. */
#define LCD_DMA_CHANNEL   DMA1_Channel3
#define LCD_DMA_FLAG_TC   DMA1_FLAG_TC3

void LCD_BlitBegin(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	LCD_AddressSet(x1, y1, x2, y2);

	RCM_EnableAHBPeriphClock(RCM_AHB_PERIPH_DMA1);

	SPI_Disable(LCD_SPI_BUS);
	SPI_ConfigDataSize(LCD_SPI_BUS, SPI_DATA_LENGTH_16B);
	SPI_Enable(LCD_SPI_BUS);

	LCD_CS_CLR();
}

void LCD_BlitPixels(const uint16_t *pixels, uint32_t count)
{
	DMA_Config_T dmaConfig;
	uint32_t timeout = 0;

	DMA_Disable(LCD_DMA_CHANNEL);
	DMA_ClearStatusFlag(LCD_DMA_FLAG_TC);

	dmaConfig.peripheralBaseAddr = (uint32_t)&LCD_SPI_BUS->DATA;
	dmaConfig.memoryBaseAddr = (uint32_t)pixels;
	dmaConfig.dir = DMA_DIR_PERIPHERAL_DST;
	dmaConfig.bufferSize = count;
	dmaConfig.peripheralInc = DMA_PERIPHERAL_INC_DISABLE;
	dmaConfig.memoryInc = DMA_MEMORY_INC_ENABLE;
	dmaConfig.peripheralDataSize = DMA_PERIPHERAL_DATA_SIZE_HALFWORD;
	dmaConfig.memoryDataSize = DMA_MEMORY_DATA_SIZE_HALFWORD;
	dmaConfig.loopMode = DMA_MODE_NORMAL;
	dmaConfig.priority = DMA_PRIORITY_HIGH;
	dmaConfig.M2M = DMA_M2MEN_DISABLE;
	DMA_Config(LCD_DMA_CHANNEL, &dmaConfig);

	SPI_I2S_EnableDMA(LCD_SPI_BUS, SPI_I2S_DMA_REQ_TX);
	DMA_Enable(LCD_DMA_CHANNEL);

	while (DMA_ReadStatusFlag(LCD_DMA_FLAG_TC) == RESET)
	{
		if (++timeout >= 2000000U)
		{
			break;
		}
	}

	SPI_I2S_DisableDMA(LCD_SPI_BUS, SPI_I2S_DMA_REQ_TX);
}

void LCD_BlitEnd(void)
{
	uint32_t timeout = 0;

	while (SPI_I2S_ReadStatusFlag(LCD_SPI_BUS, SPI_FLAG_BSY) == SET)
	{
		if (++timeout >= 20000U)
		{
			break;
		}
	}

	SPI_Disable(LCD_SPI_BUS);
	SPI_ConfigDataSize(LCD_SPI_BUS, SPI_DATA_LENGTH_8B);
	SPI_Enable(LCD_SPI_BUS);

	LCD_CS_SET();
}

void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
	int xDelta = x2 - x1;
	int yDelta = y2 - y1;
	int uRow = x1;
	int uCol = y1;
	int xInc, yInc, distance;
	int xErr = 0, yErr = 0;

	xInc = (xDelta > 0) ? 1 : ((xDelta == 0) ? 0 : -1);
	yInc = (yDelta > 0) ? 1 : ((yDelta == 0) ? 0 : -1);
	xDelta = (xDelta < 0) ? -xDelta : xDelta;
	yDelta = (yDelta < 0) ? -yDelta : yDelta;
	distance = (xDelta > yDelta) ? xDelta : yDelta;

	for (int i = 0; i <= distance; i++)
	{
		LCD_DrawPoint((uint16_t)uRow, (uint16_t)uCol, color);
		xErr += xDelta;
		yErr += yDelta;

		if (xErr > distance)
		{
			xErr -= distance;
			uRow += xInc;
		}

		if (yErr > distance)
		{
			yErr -= distance;
			uCol += yInc;
		}
	}
}

void LCD_DrawCircle(uint16_t x0, uint16_t y0, uint8_t radius, uint16_t color)
{
	int a = 0;
	int b = radius;

	while (a <= b)
	{
		LCD_DrawPoint((uint16_t)(x0 - b), (uint16_t)(y0 - a), color);
		LCD_DrawPoint((uint16_t)(x0 + b), (uint16_t)(y0 - a), color);
		LCD_DrawPoint((uint16_t)(x0 - a), (uint16_t)(y0 + b), color);
		LCD_DrawPoint((uint16_t)(x0 - a), (uint16_t)(y0 - b), color);
		LCD_DrawPoint((uint16_t)(x0 + b), (uint16_t)(y0 + a), color);
		LCD_DrawPoint((uint16_t)(x0 + a), (uint16_t)(y0 - b), color);
		LCD_DrawPoint((uint16_t)(x0 + a), (uint16_t)(y0 + b), color);
		LCD_DrawPoint((uint16_t)(x0 - b), (uint16_t)(y0 + a), color);
		a++;

		if ((a * a + b * b) > (radius * radius))
		{
			b--;
		}
	}
}

static void LCD_DisplayChar(uint16_t x, uint16_t y, uint8_t ch, uint16_t fc, uint16_t bc, uint8_t fontSize)
{
	uint8_t xSize = fontSize / 2U;
	uint16_t characterSize = (uint16_t)((xSize / 8U + ((xSize % 8U) ? 1U : 0U)) * fontSize);
	uint8_t index = (uint8_t)(ch - ' ');
	uint8_t m = 0U;

	LCD_AddressSet(x, y, (uint16_t)(x + xSize - 1U), (uint16_t)(y + fontSize - 1U));

	for (uint16_t i = 0; i < characterSize; i++)
	{
		uint8_t temp;

		switch (fontSize)
		{
		case 12: temp = asciiFont_1206[index][i]; break;
		case 16: temp = asciiFont_1608[index][i]; break;
		case 24: temp = asciiFont_2412[index][i]; break;
		case 32: temp = asciiFont_3216[index][i]; break;
		default: return;
		}

		for (uint8_t t = 0; t < 8U; t++)
		{
			LCD_WriteHalfword((temp & (0x01U << t)) ? fc : bc);
			m++;

			if ((m % xSize) == 0U)
			{
				m = 0U;
				break;
			}
		}
	}
}

void LCD_DisplayString(uint16_t x, uint16_t y, const char *p, uint16_t fc, uint16_t bc, uint8_t fontSize)
{
	while (*p != '\0')
	{
		LCD_DisplayChar(x, y, (uint8_t)*p, fc, bc, fontSize);
		x = (uint16_t)(x + fontSize / 2U);
		p++;
	}
}

void LCD_DisplayIntNum(uint16_t x, uint16_t y, uint16_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t fontSize)
{
	uint8_t xSize = fontSize / 2U;
	uint8_t enshow = 0U;

	for (uint8_t t = 0; t < len; t++)
	{
		uint8_t digit = (uint8_t)((num / LCD_CalPow(10U, (uint8_t)(len - t - 1U))) % 10U);

		if ((enshow == 0U) && (t < (len - 1U)))
		{
			if (digit == 0U)
			{
				LCD_DisplayChar((uint16_t)(x + t * xSize), y, ' ', fc, bc, fontSize);
				continue;
			}
			enshow = 1U;
		}

		LCD_DisplayChar((uint16_t)(x + t * xSize), y, (uint8_t)(digit + 48U), fc, bc, fontSize);
	}
}

void LCD_DisplayFloatNum(uint16_t x, uint16_t y, float num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t fontSize)
{
	uint8_t xSize = fontSize / 2U;
	uint16_t numTemp = (uint16_t)(num * 100.0f);

	for (uint8_t t = 0; t < len; t++)
	{
		uint8_t digit = (uint8_t)((numTemp / LCD_CalPow(10U, (uint8_t)(len - t - 1U))) % 10U);

		if (t == (len - 2U))
		{
			LCD_DisplayChar((uint16_t)(x + (len - 2U) * xSize), y, '.', fc, bc, fontSize);
			t++;
			len++;
		}

		LCD_DisplayChar((uint16_t)(x + t * xSize), y, (uint8_t)(digit + 48U), fc, bc, fontSize);
	}
}

/* Standard ST7789 240x320 init sequence (SLPOUT/COLMOD/MADCTL/porch+gamma
 * regs/INVON/DISPON) - unrelated to Geehy's ILI9341-specific demo sequence.
 * If colors look wrong: swapped red/blue -> toggle bit3 of the MADCTL value
 * below (0x00 <-> 0x08). Washed-out/inverted colors -> swap 0x21 (INVON) for
 * 0x20 (INVOFF). */
void LCD_Init(void)
{
	LCD_SPI_Init();

	LCD_RES_CLR();
	LCD_DelayMs(50);
	LCD_RES_SET();
	LCD_DelayMs(120);

	LCD_WriteReg(0x01); /* Software reset */
	LCD_DelayMs(150);

	LCD_WriteReg(0x11); /* Sleep out */
	LCD_DelayMs(120);

	LCD_WriteReg(0x3A); /* Pixel format */
	LCD_WriteData(0x55); /* 16 bpp */

	LCD_WriteReg(0x36); /* MADCTL: MY|MX set -> 180 deg rotation (portrait default) */
	LCD_WriteData(LCD_MADCTL_PORTRAIT);

	LCD_WriteReg(0xB2); /* Porch control */
	LCD_WriteData(0x0C);
	LCD_WriteData(0x0C);
	LCD_WriteData(0x00);
	LCD_WriteData(0x33);
	LCD_WriteData(0x33);

	LCD_WriteReg(0xB7); /* Gate control */
	LCD_WriteData(0x35);

	LCD_WriteReg(0xBB); /* VCOM setting */
	LCD_WriteData(0x19);

	LCD_WriteReg(0xC0); /* LCM control */
	LCD_WriteData(0x2C);

	LCD_WriteReg(0xC2); /* VDV/VRH command enable */
	LCD_WriteData(0x01);

	LCD_WriteReg(0xC3); /* VRH set */
	LCD_WriteData(0x12);

	LCD_WriteReg(0xC4); /* VDV set */
	LCD_WriteData(0x20);

	LCD_WriteReg(0xC6); /* Frame rate control */
	LCD_WriteData(0x0F);

	LCD_WriteReg(0xD0); /* Power control 1 */
	LCD_WriteData(0xA4);
	LCD_WriteData(0xA1);

	LCD_WriteReg(0xE0); /* Positive voltage gamma */
	LCD_WriteData(0xD0); LCD_WriteData(0x04); LCD_WriteData(0x0D); LCD_WriteData(0x11);
	LCD_WriteData(0x13); LCD_WriteData(0x2B); LCD_WriteData(0x3F); LCD_WriteData(0x54);
	LCD_WriteData(0x4C); LCD_WriteData(0x18); LCD_WriteData(0x0D); LCD_WriteData(0x0B);
	LCD_WriteData(0x1F); LCD_WriteData(0x23);

	LCD_WriteReg(0xE1); /* Negative voltage gamma */
	LCD_WriteData(0xD0); LCD_WriteData(0x04); LCD_WriteData(0x0C); LCD_WriteData(0x11);
	LCD_WriteData(0x13); LCD_WriteData(0x2C); LCD_WriteData(0x3F); LCD_WriteData(0x44);
	LCD_WriteData(0x51); LCD_WriteData(0x2F); LCD_WriteData(0x1F); LCD_WriteData(0x1F);
	LCD_WriteData(0x20); LCD_WriteData(0x23);

	LCD_WriteReg(0x21); /* Display inversion on */
	LCD_WriteReg(0x13); /* Normal display mode on */

	LCD_WriteReg(0x29); /* Display on */
	LCD_DelayMs(50);
}
