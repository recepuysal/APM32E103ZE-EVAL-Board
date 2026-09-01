/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include "sdcard.h"

#define USB_SECTOR_SIZE     512//4096//512
#define USB_BLOCK_SIZE      512//4096//512

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	return 0;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
    return 0;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	DRESULT res = RES_OK;

    sector *= 512;
    if (count == 1)
    {
        if (SD_ReadBlock(buff, sector, 512) != SD_OK) // THH
        {
            res = RES_ERROR;
        }
    }
    else if (count > 1)
    {
        if (SD_ReadMultiBlocks(buff, sector, 512, count) != SD_OK)
        {
            res = RES_ERROR;
        }
    }
    else
    {
        res = RES_PARERR;
    }

	return res;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	DRESULT res = RES_OK;
    
    sector *= 512;
    if (count == 1)
    {
        if (SD_WriteBlock((uint8_t *)buff, sector, 512) != SD_OK) // THH
        {
            res = RES_ERROR;
        }
    }
    else if (count > 1)
    {
        if (SD_WriteMultiBlocks((uint8_t *)buff, sector, 512, count) != SD_OK)
        {
            res = RES_ERROR;
        }
    }
    else
    {
        res = RES_PARERR;
    }

	return res;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
    DRESULT res = RES_ERROR;;


    switch(cmd)
    {
        case CTRL_SYNC:
            res = RES_OK;
            break;
        
        case GET_SECTOR_SIZE:
            *(WORD*)buff = 512;
            res = RES_OK;
            break;
        
        case GET_BLOCK_SIZE:
            *(DWORD*)buff = SDCardInfo.CardBlockSize;
            res = RES_OK;
            break;
        
        case GET_SECTOR_COUNT:
            *(DWORD*)buff = SDCardInfo.CardCapacity>>20;
            res = RES_OK;
            break;
        default :
            res = RES_PARERR;
            break;
    }

    return res;
}
