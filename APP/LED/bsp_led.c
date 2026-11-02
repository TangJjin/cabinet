/*
 * 文件用途：原板卡 LED BSP：封装绿色、紫色通道 GPIO 控制；现用报警节奏由 bsp_board.c 管理。
 * 编译归属：CM7 应用。
 */
#include "bsp_led.h"

void green_led(uint8_t flag)
{
    if (flag == 2)
    {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_3);
    }
    else
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, flag);
}

void purple_led(uint8_t flag)
{
    if (flag == 2)
    {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1);
    }
    else
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, flag);
}
