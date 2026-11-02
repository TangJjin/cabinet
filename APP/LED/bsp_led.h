/*
 * 文件用途：接口与类型声明：原板卡 LED BSP：封装绿色、紫色通道 GPIO 控制；现用报警节奏由 bsp_board.c 管理。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef BSP_LED__
#define BSP_LED__

#include "bsp_system.h"

void green_led(uint8_t flag);
void purple_led(uint8_t flag);

#endif
