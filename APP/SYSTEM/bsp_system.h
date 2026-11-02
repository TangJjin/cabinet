/*
 * 文件用途：接口与类型声明：显示屏参数：保存原板卡已经验证的 ILI9881C 10.1 寸 MIPI 屏初始化命令表。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef BSP_SYSTEM__
#define BSP_SYSTEM__

#include "main.h"
#include "usart.h"
#include "dsihost.h"
#include "dma2d.h"
#include "i2c.h"
#include "ltdc.h"
#include "lvgl.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>
#include "string.h"
#include "stdio.h"
#include "stdarg.h"
#include "stdbool.h"
#include <math.h>

#include "bsp_led.h"
#include "bsp_uart.h"
#include "ringbuffer.h"
#include "bsp_sdram.h"
#include "bsp_mipi_lcd.h"
#include "bsp_touch_i2c.h"
#include "bsp_touch.h"


#include "bsp_cabinet_ui.h"

typedef struct
{
    uint8_t cmd;         /* 命令 */
    const uint8_t *data; /* 参数数据 */
    uint8_t data_bytes;  /* 参数字节数 */
} mipi_lcd_init_cmd_t;

extern uint8_t uart_rx_dma_buffer[128];

extern const mipi_lcd_init_cmd_t ili9881c10_init_cmd_tbl[];
extern const uint16_t ili9881c10_init_cmd_count;

extern volatile uint32_t g_flush_last_us;
extern volatile uint32_t g_flush_max_us;
extern volatile uint32_t g_flush_total_us;
extern volatile uint32_t g_flush_count;
extern volatile uint32_t g_flush_max_pixels;

#endif
