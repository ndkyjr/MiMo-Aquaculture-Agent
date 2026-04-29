#ifndef __FLASH_H
#define __FLASH_H 			   

#include "stm32f10x.h"                  // Device header

// ??:MiMo Token Flash????(???????,????)
// STM32F103C8T6 Flash??:0x08000000~0x0800FFFF(64KB),??1KB
#define MIMO_FLASH_TOKEN_ADDR 0x08007000

void FLASH_W(u32 add,u16 dat,u16 dat2,u16 dat3,u16 dat4,u16 dat5);
u16 FLASH_R(u32 add);
// ??:??????(?????)
void FLASH_WriteBuffer(u32 addr, u16 *pBuf, u16 len);
void FLASH_ReadBuffer(u32 addr, u16 *pBuf, u16 len);

#endif