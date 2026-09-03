/**
 ******************************************************************************
 * @file    demo.h
 * @brief   Real-time bouncing-box animation ("DVD screensaver" style) - a
 *          small showcase of the LCD driver's redraw speed. Full video
 *          playback isn't realistic on a 72MHz MCU driving an SPI panel;
 *          a small-footprint sprite redrawn every frame is, and looks just
 *          as lively.
 ******************************************************************************
 */

#ifndef __DEMO_H
#define __DEMO_H

void Demo_Start(void);
void Demo_Step(void); /* call every main-loop iteration; internally rate-limited */

#endif
