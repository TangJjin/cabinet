/*
 * 文件用途：CM4 应用入口：更新本核时钟信息，初始化 HAL，创建采集任务并启动本核 FreeRTOS。
 * 编译归属：CM4 应用。
 */
#include "bsp_board.h"
#include "bsp_sensor.h"
#include "FreeRTOS.h"
#include "task.h"

int main(void)
{
    /* 全局系统时钟由 CM7 配好；CM4 更新本核频率信息后，创建自己的任务并启动自己的调度器。 */
    SystemCoreClockUpdate();
    HAL_Init();
    bsp_sensor_start();
    vTaskStartScheduler();
    bsp_fatal();
}

void Error_Handler(void)
{
    bsp_fatal();
}
