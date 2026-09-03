#ifndef __VIDEO_DATA_H
#define __VIDEO_DATA_H

/* Landscape native resolution (LCD_LANDSCAPE_WIDTH/HEIGHT - same physical
 * glass as the portrait menu, rotated 90 deg via LCD_SetOrientation()).
 * Frames are generated at this exact size and streamed from the SD card's
 * VIDEO.BIN (see extract_native.py used to generate it) instead of being
 * embedded in MCU flash. */
#define VIDEO_FRAME_W     280U
#define VIDEO_FRAME_H     240U
#define VIDEO_FRAME_COUNT 9744U
#define VIDEO_FPS         12U

#endif
