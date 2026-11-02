/*
 * 文件用途：FreeRTOS 错误钩子：内存申请失败或任务栈溢出时进入统一故障处理。
 * 编译归属：CM7 应用、CM4 应用。
 */
#include "FreeRTOS.h"
#include "task.h"
#include "bsp_board.h"

void vApplicationMallocFailedHook(void)
{
    bsp_fatal();
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    bsp_fatal();
}
