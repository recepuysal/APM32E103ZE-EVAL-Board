/**
 ******************************************************************************
 * @file    spiflash.h
 * @brief   W25Q16 SPI NOR flash (2MB) driver, ported from Geehy's official
 *          bsp_w25q16.c (GeehySemi/APM32E10x_EVAL_SDK, Examples/SPI/SPI_Flash).
 *
 * Pins confirmed from the schematic: SPI3 CS=PA15 SCK=PB3 MISO=PB4 MOSI=PB5.
 * PA15/PB3/PB4 double as JTAG pins (TDI/TDO/TRST) - SPIFlash_Init() disables
 * JTAG (keeping SWD) via GPIO_ConfigPinRemap(GPIO_REMAP_SWJ_JTAGDISABLE) so
 * these pins are free for SPI3. ST-Link debugging over SWD still works.
 ******************************************************************************
 */

#ifndef __SPIFLASH_H
#define __SPIFLASH_H

#include <stdint.h>

#define W25Q16_JEDEC_ID       0xEF4015U
#define W25Q16_PAGE_SIZE      256U
#define W25Q16_SECTOR_SIZE    4096U

void SPIFlash_Init(void);
uint32_t SPIFlash_ReadJedecID(void);
void SPIFlash_EraseSector(uint32_t addr);
void SPIFlash_WriteBuffer(const uint8_t *buf, uint32_t addr, uint16_t len);
void SPIFlash_ReadBuffer(uint8_t *buf, uint32_t addr, uint16_t len);

#endif
