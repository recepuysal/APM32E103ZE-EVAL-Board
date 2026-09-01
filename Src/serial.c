/**
 ******************************************************************************
 * @file    serial.c
 * @brief   USART1 status log output body.
 *
 * Pins/baud match Geehy's official SPI_LCD demo COM1 config (Board_APM32E103_EVAL.h):
 * TX=PA9, RX=PA10, 115200 8N1, no hardware flow control.
 ******************************************************************************
 */

#include "serial.h"
#include "apm32e10x.h"
#include "apm32e10x_gpio.h"
#include "apm32e10x_rcm.h"
#include "apm32e10x_usart.h"

#define SERIAL_USART   USART1
#define SERIAL_PORT    GPIOA
#define SERIAL_TX_PIN  GPIO_PIN_9
#define SERIAL_RX_PIN  GPIO_PIN_10

void Serial_Init(void)
{
	GPIO_Config_T gpioConfig;
	USART_Config_T usartConfig;

	RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_GPIOA | RCM_APB2_PERIPH_USART1);

	gpioConfig.pin = SERIAL_TX_PIN;
	gpioConfig.mode = GPIO_MODE_AF_PP;
	gpioConfig.speed = GPIO_SPEED_50MHz;
	GPIO_Config(SERIAL_PORT, &gpioConfig);

	gpioConfig.pin = SERIAL_RX_PIN;
	gpioConfig.mode = GPIO_MODE_IN_FLOATING;
	GPIO_Config(SERIAL_PORT, &gpioConfig);

	usartConfig.baudRate = 115200;
	usartConfig.wordLength = USART_WORD_LEN_8B;
	usartConfig.stopBits = USART_STOP_BIT_1;
	usartConfig.parity = USART_PARITY_NONE;
	usartConfig.mode = USART_MODE_TX_RX;
	usartConfig.hardwareFlow = USART_HARDWARE_FLOW_NONE;
	USART_Config(SERIAL_USART, &usartConfig);

	USART_Enable(SERIAL_USART);
}

void Serial_Print(const char *s)
{
	while (*s != '\0')
	{
		while (USART_ReadStatusFlag(SERIAL_USART, USART_FLAG_TXBE) == RESET)
		{
		}

		USART_TxData(SERIAL_USART, (uint16_t)(uint8_t)*s);
		s++;
	}
}
