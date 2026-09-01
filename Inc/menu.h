/**
 ******************************************************************************
 * @file    menu.h
 * @brief   KEY1/KEY2/KEY3-driven LCD menu, ported from Geehy's official
 *          SPI_LCD demo (GeehySemi/APM32E10x_EVAL_SDK) navigation scheme:
 *          KEY1 = cycle selection, KEY2 = enter, KEY3 = return.
 ******************************************************************************
 */

#ifndef __MENU_H
#define __MENU_H

void Menu_Init(void);
void Menu_Poll(void);

#endif
