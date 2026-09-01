/**
 ******************************************************************************
 * @file    temp.h
 * @brief   APM32E103ZE internal die temperature sensor (ADC1 channel 16).
 ******************************************************************************
 */

#ifndef __TEMP_H
#define __TEMP_H

void Temp_Init(void);
float Temp_ReadCelsius(void);

#endif
