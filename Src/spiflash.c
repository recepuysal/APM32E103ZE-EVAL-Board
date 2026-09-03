/**
 ******************************************************************************
 * @file    spiflash.c
 * @brief   W25Q16 SPI NOR flash driver body - ported from Geehy's official
 *          bsp_w25q16.c (see spiflash.h for details). Trimmed to the
 *          functions this project actually uses (JEDEC ID, sector erase,
 *          buffered write/read) - power-down and single-byte read/write
 *          helpers from the original weren't needed and were dropped.
 ******************************************************************************
 */

#include "spiflash.h"
#include "apm32e10x_gpio.h"
#include "apm32e10x_rcm.h"
#include "apm32e10x_spi.h"

#define FLASH_SPI_BUS       SPI3
#define FLASH_SPI_PORT      GPIOB
#define FLASH_SPI_MOSI_PIN  GPIO_PIN_5
#define FLASH_SPI_MISO_PIN  GPIO_PIN_4
#define FLASH_SPI_SCK_PIN   GPIO_PIN_3
#define FLASH_CS_PORT       GPIOA
#define FLASH_CS_PIN        GPIO_PIN_15

#define FLASH_CS_CLR() GPIO_ResetBit(FLASH_CS_PORT, FLASH_CS_PIN)
#define FLASH_CS_SET() GPIO_SetBit(FLASH_CS_PORT, FLASH_CS_PIN)

#define CMD_WRITE_ENABLE     0x06U
#define CMD_READ_STATUS_REG  0x05U
#define CMD_READ_DATA        0x03U
#define CMD_PAGE_PROGRAM     0x02U
#define CMD_SECTOR_ERASE     0x20U
#define CMD_JEDEC_ID         0x9FU
#define WIP_FLAG             0x01U
#define DUMMY_BYTE           0xFFU

static uint8_t SendByte(uint8_t data)
{
	while (SPI_I2S_ReadStatusFlag(FLASH_SPI_BUS, SPI_FLAG_TXBE) == RESET)
	{
	}

	SPI_I2S_TxData(FLASH_SPI_BUS, data);

	while (SPI_I2S_ReadStatusFlag(FLASH_SPI_BUS, SPI_FLAG_RXBNE) == RESET)
	{
	}

	return SPI_I2S_RxData(FLASH_SPI_BUS);
}

static void WaitWriteEnd(void)
{
	uint8_t status;

	FLASH_CS_CLR();
	SendByte(CMD_READ_STATUS_REG);

	do
	{
		status = SendByte(DUMMY_BYTE);
	}
	while ((status & WIP_FLAG) != 0U);

	FLASH_CS_SET();
}

static void EnableWrite(void)
{
	FLASH_CS_CLR();
	SendByte(CMD_WRITE_ENABLE);
	FLASH_CS_SET();
}

static void WritePage(const uint8_t *buf, uint32_t addr, uint16_t len)
{
	EnableWrite();
	FLASH_CS_CLR();

	SendByte(CMD_PAGE_PROGRAM);
	SendByte((uint8_t)(addr >> 16));
	SendByte((uint8_t)(addr >> 8));
	SendByte((uint8_t)addr);

	if (len > W25Q16_PAGE_SIZE)
	{
		len = W25Q16_PAGE_SIZE;
	}

	for (uint16_t i = 0; i < len; i++)
	{
		SendByte(buf[i]);
	}

	FLASH_CS_SET();
	WaitWriteEnd();
}

void SPIFlash_Init(void)
{
	GPIO_Config_T gpioConfig;
	SPI_Config_T spiConfig;

	RCM_EnableAPB1PeriphClock(RCM_APB1_PERIPH_SPI3);
	RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_GPIOB | RCM_APB2_PERIPH_GPIOA | RCM_APB2_PERIPH_AFIO);

	/* PA15/PB3/PB4 double as JTAG (TDI/TDO/TRST) - free them for SPI3, SWD
	 * (PA13/PA14) is unaffected. */
	GPIO_ConfigPinRemap(GPIO_REMAP_SWJ_JTAGDISABLE);

	gpioConfig.pin = FLASH_SPI_SCK_PIN | FLASH_SPI_MOSI_PIN;
	gpioConfig.mode = GPIO_MODE_AF_PP;
	gpioConfig.speed = GPIO_SPEED_50MHz;
	GPIO_Config(FLASH_SPI_PORT, &gpioConfig);

	gpioConfig.pin = FLASH_SPI_MISO_PIN;
	gpioConfig.mode = GPIO_MODE_IN_FLOATING;
	GPIO_Config(FLASH_SPI_PORT, &gpioConfig);

	gpioConfig.pin = FLASH_CS_PIN;
	gpioConfig.mode = GPIO_MODE_OUT_PP;
	GPIO_Config(FLASH_CS_PORT, &gpioConfig);

	FLASH_CS_SET();

	SPI_ConfigStructInit(&spiConfig);
	spiConfig.direction = SPI_DIRECTION_2LINES_FULLDUPLEX;
	spiConfig.mode = SPI_MODE_MASTER;
	spiConfig.length = SPI_DATA_LENGTH_8B;
	spiConfig.polarity = SPI_CLKPOL_HIGH;
	spiConfig.phase = SPI_CLKPHA_2EDGE;
	spiConfig.nss = SPI_NSS_SOFT;
	spiConfig.baudrateDiv = SPI_BAUDRATE_DIV_4;
	spiConfig.firstBit = SPI_FIRSTBIT_MSB;
	spiConfig.crcPolynomial = 7;
	SPI_Config(FLASH_SPI_BUS, &spiConfig);

	SPI_Enable(FLASH_SPI_BUS);
}

uint32_t SPIFlash_ReadJedecID(void)
{
	uint32_t id;

	FLASH_CS_CLR();
	SendByte(CMD_JEDEC_ID);
	id = (uint32_t)SendByte(DUMMY_BYTE) << 16;
	id |= (uint32_t)SendByte(DUMMY_BYTE) << 8;
	id |= SendByte(DUMMY_BYTE);
	FLASH_CS_SET();

	return id;
}

void SPIFlash_EraseSector(uint32_t addr)
{
	EnableWrite();
	FLASH_CS_CLR();

	SendByte(CMD_SECTOR_ERASE);
	SendByte((uint8_t)(addr >> 16));
	SendByte((uint8_t)(addr >> 8));
	SendByte((uint8_t)addr);

	FLASH_CS_SET();
	WaitWriteEnd();
}

void SPIFlash_WriteBuffer(const uint8_t *buf, uint32_t addr, uint16_t len)
{
	while (len > 0U)
	{
		uint16_t pageRemain = (uint16_t)(W25Q16_PAGE_SIZE - (addr % W25Q16_PAGE_SIZE));
		uint16_t chunk = (len < pageRemain) ? len : pageRemain;

		WritePage(buf, addr, chunk);
		addr += chunk;
		buf += chunk;
		len = (uint16_t)(len - chunk);
	}
}

void SPIFlash_ReadBuffer(uint8_t *buf, uint32_t addr, uint16_t len)
{
	FLASH_CS_CLR();

	SendByte(CMD_READ_DATA);
	SendByte((uint8_t)(addr >> 16));
	SendByte((uint8_t)(addr >> 8));
	SendByte((uint8_t)addr);

	for (uint16_t i = 0; i < len; i++)
	{
		buf[i] = SendByte(DUMMY_BYTE);
	}

	FLASH_CS_SET();
}
