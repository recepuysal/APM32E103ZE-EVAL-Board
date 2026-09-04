/**
 ******************************************************************************
 * @file    livestream.c
 * @brief   Live screen-mirror playback body (see livestream.h).
 *
 * Wire protocol per update (see livestream_send.py):
 *   4 bytes  sync marker 0xAA 0x55 0xAA 0x55
 *   2 bytes  x1 (uint16 LE)      - dirty-rect top-left, in LIVESTREAM_FRAME
 *   2 bytes  y1 (uint16 LE)        coordinates (native panel resolution)
 *   2 bytes  w  (uint16 LE)      - dirty-rect width
 *   2 bytes  h  (uint16 LE)      - dirty-rect height
 *   w*h*2    RGB565 pixel bytes (native byte order, row-major)
 *
 * The PC sender diffs each capture against the previous one and only sends
 * the bounding box of what actually changed - full quality (native
 * resolution, no upscaling) with a much shorter wire transfer whenever the
 * screen is mostly static (desktop, text, a paused window). A fully
 * changed frame (e.g. full-motion video) just degenerates back to the
 * whole 280x240 rectangle, same cost as before.
 ******************************************************************************
 */

#include "livestream.h"
#include "lcd.h"
#include "apm32e10x.h"
#include "apm32e10x_rcm.h"
#include "apm32e10x_usart.h"
#include "apm32e10x_dma.h"

#define LS_USART      USART1
#define LS_DMA_RX_CH  DMA1_Channel5 /* USART1_RX - classic STM32F1-family mapping */
#define LS_DMA_RX_TC  DMA1_FLAG_TC5

/* Sized to absorb a stall several times longer than any single chunk blit
 * takes (a few ms) - at 2,000,000 baud (~200KB/s) this is ~40ms of slack,
 * so occasional SD-log/other main-loop work doesn't overrun it. */
#define RING_SIZE     8192U

/* Streamed straight into the LCD in chunks, like video.c's SD reads - never
 * buffers a whole frame in RAM, so resolution isn't RAM-limited. */
#define CHUNK_BYTES   4096U

#define HEADER_BYTES  8U /* x1,y1,w,h - uint16 LE each */

#define LS_STATE_SYNC    0U
#define LS_STATE_HEADER  1U
#define LS_STATE_PAYLOAD 2U

static const uint8_t SYNC[4] = { 0xAAU, 0x55U, 0xAAU, 0x55U };

static uint8_t s_ring[RING_SIZE];
static uint16_t s_readPos;
static uint8_t s_state;
static uint8_t s_syncMatch;
static uint8_t s_headerBuf[HEADER_BYTES];
static uint8_t s_headerPos;
static uint32_t s_frameBytesLeft;
static uint16_t s_chunkFill;
static uint8_t s_chunkBuf[CHUNK_BYTES];

static uint16_t RingAvailable(void)
{
	uint16_t writePos = (uint16_t)(RING_SIZE - DMA_ReadDataNumber(LS_DMA_RX_CH));

	return (uint16_t)((writePos + RING_SIZE - s_readPos) % RING_SIZE);
}

static uint8_t RingReadByte(void)
{
	uint8_t b = s_ring[s_readPos];

	s_readPos = (uint16_t)((s_readPos + 1U) % RING_SIZE);
	return b;
}

static uint16_t LoadU16LE(const uint8_t *p)
{
	return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* Header bytes come straight off the wire - clamp everything to the panel
 * bounds so a corrupted/desynced header (lost bytes, stale backlog) can
 * never point the LCD address window or the byte counter somewhere wrong. */
static void BeginDirtyRect(void)
{
	uint16_t x1 = LoadU16LE(&s_headerBuf[0]);
	uint16_t y1 = LoadU16LE(&s_headerBuf[2]);
	uint16_t w  = LoadU16LE(&s_headerBuf[4]);
	uint16_t h  = LoadU16LE(&s_headerBuf[6]);

	if (x1 >= LIVESTREAM_FRAME_W) { x1 = 0U; }
	if (y1 >= LIVESTREAM_FRAME_H) { y1 = 0U; }
	if (w == 0U) { w = 1U; }
	if (h == 0U) { h = 1U; }
	if ((uint32_t)x1 + w > LIVESTREAM_FRAME_W) { w = (uint16_t)(LIVESTREAM_FRAME_W - x1); }
	if ((uint32_t)y1 + h > LIVESTREAM_FRAME_H) { h = (uint16_t)(LIVESTREAM_FRAME_H - y1); }

	LCD_BlitBegin(x1, y1, (uint16_t)(x1 + w - 1U), (uint16_t)(y1 + h - 1U));
	s_frameBytesLeft = (uint32_t)w * h * 2UL;
	s_chunkFill = 0U;
	s_state = LS_STATE_PAYLOAD;
}

void LiveStream_Init(void)
{
	DMA_Config_T dmaConfig;

	/* USART1 pins/baud already brought up by Serial_Init() (shared link) -
	 * just add the RX DMA path here. */
	RCM_EnableAHBPeriphClock(RCM_AHB_PERIPH_DMA1);

	DMA_ConfigStructInit(&dmaConfig);
	dmaConfig.peripheralBaseAddr = (uint32_t)&LS_USART->DATA;
	dmaConfig.memoryBaseAddr = (uint32_t)s_ring;
	dmaConfig.dir = DMA_DIR_PERIPHERAL_SRC;
	dmaConfig.bufferSize = RING_SIZE;
	dmaConfig.peripheralInc = DMA_PERIPHERAL_INC_DISABLE;
	dmaConfig.memoryInc = DMA_MEMORY_INC_ENABLE;
	dmaConfig.peripheralDataSize = DMA_PERIPHERAL_DATA_SIZE_BYTE;
	dmaConfig.memoryDataSize = DMA_MEMORY_DATA_SIZE_BYTE;
	dmaConfig.loopMode = DMA_MODE_CIRCULAR;
	dmaConfig.priority = DMA_PRIORITY_HIGH;
	dmaConfig.M2M = DMA_M2MEN_DISABLE;
	DMA_Config(LS_DMA_RX_CH, &dmaConfig);

	USART_EnableDMA(LS_USART, USART_DMA_RX);
	DMA_Enable(LS_DMA_RX_CH);
}

void LiveStream_Start(void)
{
	LCD_SetOrientation(1);
	LCD_Clear(0, 0, LCD_LANDSCAPE_WIDTH, LCD_LANDSCAPE_HEIGHT, LCD_COLOR_BLACK);

	s_readPos = (uint16_t)(RING_SIZE - DMA_ReadDataNumber(LS_DMA_RX_CH)); /* start at "now", ignore stale backlog */
	s_state = LS_STATE_SYNC;
	s_syncMatch = 0U;
	s_headerPos = 0U;
	s_frameBytesLeft = 0U;
	s_chunkFill = 0U;
}

void LiveStream_Step(void)
{
	uint16_t available = RingAvailable();

	while (available > 0U)
	{
		uint8_t b = RingReadByte();
		available--;

		if (s_state == LS_STATE_SYNC)
		{
			if (b == SYNC[s_syncMatch])
			{
				s_syncMatch++;
				if (s_syncMatch >= 4U)
				{
					s_state = LS_STATE_HEADER;
					s_headerPos = 0U;
				}
			}
			else
			{
				s_syncMatch = (b == SYNC[0]) ? 1U : 0U;
			}
		}
		else if (s_state == LS_STATE_HEADER)
		{
			s_headerBuf[s_headerPos] = b;
			s_headerPos++;

			if (s_headerPos >= HEADER_BYTES)
			{
				BeginDirtyRect(); /* -> LS_STATE_PAYLOAD */
			}
		}
		else /* LS_STATE_PAYLOAD */
		{
			s_chunkBuf[s_chunkFill] = b;
			s_chunkFill++;
			s_frameBytesLeft--;

			if ((s_chunkFill >= CHUNK_BYTES) || (s_frameBytesLeft == 0U))
			{
				LCD_BlitPixels((const uint16_t *)(const void *)s_chunkBuf, s_chunkFill / 2U);
				s_chunkFill = 0U;
			}

			if (s_frameBytesLeft == 0U)
			{
				LCD_BlitEnd();
				s_state = LS_STATE_SYNC;
				s_syncMatch = 0U;
				return; /* update complete - let the main loop breathe */
			}
		}
	}
}

void LiveStream_Stop(void)
{
	LCD_SetOrientation(0);
}
