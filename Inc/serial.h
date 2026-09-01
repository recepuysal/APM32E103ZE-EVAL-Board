/**
 ******************************************************************************
 * @file    serial.h
 * @brief   USART1 (PA9 TX / PA10 RX, 115200 8N1) status log output.
 ******************************************************************************
 */

#ifndef __SERIAL_H
#define __SERIAL_H

void Serial_Init(void);
void Serial_Print(const char *s);

#endif
