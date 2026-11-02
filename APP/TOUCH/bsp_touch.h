/*
 * 文件用途：接口与类型声明：触摸屏 BSP：初始化触摸控制器、读取触点并转换坐标，供 LVGL 输入适配使用。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef BSP_TOUCH_H
#define BSP_TOUCH_H

#include "bsp_system.h"

#define TOUCH_GT9XXX_ADDRESS (0x14U << 1U)
#define TOUCH_FT5206_ADDRESS (0x38U << 1U)

#define TOUCH_CONTROLLER_NONE 0U
#define TOUCH_CONTROLLER_GT9XXX 1U
#define TOUCH_CONTROLLER_FT5206 2U

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint8_t pressed;
    uint8_t points;
} Touch_DataTypeDef;

HAL_StatusTypeDef Touch_Init(void);
uint8_t Touch_Read(Touch_DataTypeDef *touch);
uint8_t Touch_GetController(void);

#endif
