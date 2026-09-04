/**
 ******************************************************************************
 * @file    livestream.h
 * @brief   Live "screen mirror" over USART1 - a PC script captures a window
 *          (browser, webcam, whatever) and streams frames over the same
 *          serial link already used for status logging (CH340, see
 *          serial.c). Each frame is a 4-byte sync marker followed by raw
 *          RGB565 pixel data; USART1 RX runs via circular DMA so bytes keep
 *          arriving in the background even while the CPU is busy blitting
 *          the previous frame to the LCD.
 ******************************************************************************
 */

#ifndef __LIVESTREAM_H
#define __LIVESTREAM_H

/* Native panel resolution - no upscale, no softness. The tradeoff is frame
 * rate: at 2,000,000 baud the USART1/CH340 link moves ~200KB/s, and one
 * full 280x240 RGB565 frame is 134400 bytes, so this lands around 1.5fps.
 * That's the deliberate choice here (max image quality over motion). */
#define LIVESTREAM_FRAME_W 280U
#define LIVESTREAM_FRAME_H 240U

void LiveStream_Init(void);  /* sets up USART1 RX DMA - call once at boot */
void LiveStream_Start(void); /* landscape + reset the frame parser - call on entering the page */
void LiveStream_Step(void);  /* call every main-loop iteration; blits each frame as it completes */
void LiveStream_Stop(void);  /* back to portrait - call on leaving the page */

#endif
