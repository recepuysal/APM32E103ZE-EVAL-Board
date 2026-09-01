/**
 ******************************************************************************
 * @file    backlight.h
 * @brief   LCD backlight (PA8, TIM1_CH1 PWM) brightness set by the onboard
 *          potentiometer (RV1, wiper on PC0/ADC12_IN10).
 ******************************************************************************
 */

#ifndef __BACKLIGHT_H
#define __BACKLIGHT_H

/* Requires ADC1 already configured+enabled - call after Temp_Init(). */
void Backlight_Init(void);
void Backlight_Poll(void);

#endif
