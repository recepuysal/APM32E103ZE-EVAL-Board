/**
 ******************************************************************************
 * @file    video.h
 * @brief   Full-screen "video" playback, streamed from the SD card
 *          (0:VIDEO.BIN, native 240x280 RGB565 frames - see video_data.h)
 *          rather than embedded in MCU flash. The card has room for far
 *          more/bigger frames than the MCU's 512KB flash ever would;
 *          streaming trades some of that for read latency per frame.
 ******************************************************************************
 */

#ifndef __VIDEO_H
#define __VIDEO_H

void Video_Start(void);       /* opens 0:VIDEO.BIN - call while the SD card is mounted */
void Video_Step(void);        /* call every main-loop iteration; no-ops while paused */
void Video_Stop(void);        /* closes the file (also acts as "stop") - call when leaving the video page */
void Video_TogglePause(void); /* pause/resume in place, current frame stays on screen */

#endif
