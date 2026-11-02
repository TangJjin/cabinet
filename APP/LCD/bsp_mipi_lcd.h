/*
 * 文件用途：接口与类型声明：MIPI 屏 BSP：复位、面板命令、背光控制和基础绘图；与 DSI/LTDC 配置配合。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef BSP_MIPI_LCD_H
#define BSP_MIPI_LCD_H

#include "bsp_system.h"

#define LCD_WIDTH 800U
#define LCD_HEIGHT 1280U
#define LCD_PHYSICAL_WIDTH LCD_HEIGHT
#define LCD_PHYSICAL_HEIGHT LCD_WIDTH
#define LCD_PIXEL_BYTES 2U
#define LCD_FRAMEBUFFER 0xD0000000UL
#define LCD_FB ((volatile uint16_t *)0xD0000000UL)

#define LCD_COLOR_BLACK 0x0000U
#define LCD_COLOR_WHITE 0xFFFFU
#define LCD_COLOR_RED 0xF800U
#define LCD_COLOR_GREEN 0x07E0U
#define LCD_COLOR_BLUE 0x001FU

HAL_StatusTypeDef BSP_ILI9881C10_Init(void);
HAL_StatusTypeDef MIPI_LCD_Init(void);
HAL_StatusTypeDef MIPI_LCD_SendCommand(uint16_t command, const uint8_t *data, uint16_t length);

void MIPI_LCD_Clear(uint16_t color);
void MIPI_LCD_SetBacklight(uint8_t enable);

void MIPI_LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);

void LCD_Test_Corners(void);

#endif
