/*
 * 文件用途：CM7 应用入口：板级初始化、SDRAM/显示屏、业务与 IPC 初始化、CM4 放行、LVGL 和调度器启动。
 * 编译归属：CM7 应用。
 */
#include "bsp_system.h"
#include "bsp_board.h"
#include "bsp_cabinet.h"
#include "bsp_cabinet_ui.h"
#include "gpio.h"
#include "dma.h"
#include "fmc.h"
extern void lv_port_disp_init(void);
extern void lv_port_indev_init(void);

int main(void)
{
    //系统初始化
    HAL_Init();
    bsp_mpu_init();
    bsp_clock_init();
    SCB_EnableICache();
    __HAL_RCC_D2SRAM1_CLK_ENABLE();
    __HAL_RCC_D2SRAM2_CLK_ENABLE();
    __HAL_RCC_D2SRAM3_CLK_ENABLE();
    MX_GPIO_Init();
    bsp_board_outputs_init();
    bsp_watchdog_start();
    MX_DMA_Init();
    MX_FMC_Init();
    MX_USART1_UART_Init();
    MX_I2C2_Init();
    uart_init();

    //初始化SDRAM和DSI/LTDC/MIPI显示屏
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    if (sdram_init(&hsdram2))
        bsp_fatal();
    /* Preserve the board's verified SDRAM -> DSI -> LTDC -> panel -> touch sequence. */
    MX_DSIHOST_DSI_Init();
    MX_LTDC_Init();
    MX_DMA2D_Init();
    if (MIPI_LCD_Init() != HAL_OK || Touch_Init() != HAL_OK)
        bsp_fatal();

    //初始化机柜状态、存储资源、日志队列和双核通信
    /* 该初始化会先建立 OpenAMP 共享资源；随后释放 CM4，让它安全接入同一份资源。 */
    bsp_cabinet_init(); 
    bsp_cm4_release();  //释放CM4

    //LVGL初始化
    lv_init();
    lv_tick_set_cb(HAL_GetTick);
    lv_port_disp_init();
    lv_port_indev_init();
    bsp_ui_init();

    //RTOS任务创建和调度器启动
    bsp_cabinet_start();
    vTaskStartScheduler();
    bsp_fatal();
}

void Error_Handler(void)
{
    bsp_fatal();
}
