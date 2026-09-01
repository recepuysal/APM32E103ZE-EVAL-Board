/**
 ******************************************************************************
 * @file    sdlog.h
 * @brief   microSD (TF card, SDIO) status logging to LOG.TXT via FatFs.
 *
 * Card-detect pin (PC7, confirmed from schematic: R53 pull-up to +3.3V,
 * switch pulls low when a card is seated) is checked once at boot - no
 * hot-plug re-mount, matching this project's other "check once at startup"
 * bring-up modules.
 ******************************************************************************
 */

#ifndef __SDLOG_H
#define __SDLOG_H

#include <stdint.h>

typedef enum
{
	SDLOG_NO_CARD,
	SDLOG_INIT_FAIL,
	SDLOG_NO_FILESYSTEM, /* card present but unformatted/foreign FS - call SdLog_TryFormat() to fix */
	SDLOG_MOUNT_FAIL,
	SDLOG_MOUNTED,
} SdLog_State_T;

void SdLog_Init(void);
void SdLog_Append(const char *line);
SdLog_State_T SdLog_GetState(void);
uint32_t SdLog_GetFreeKB(void);
uint32_t SdLog_GetWriteCount(void);

/* Formats the card as FAT and (re)mounts it. DESTRUCTIVE - erases the card.
 * Only takes effect when GetState() == SDLOG_NO_FILESYSTEM; call only after
 * explicit user confirmation (see menu.c's double-press-KEY2 gate). */
void SdLog_TryFormat(void);

#endif
