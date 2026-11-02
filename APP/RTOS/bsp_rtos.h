/*
 * 文件用途：接口与类型声明：保留的旧演示任务，当前四个 Keil 目标均未编译；现用任务在 bsp_cabinet.c、bsp_sensor.c 等文件。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef BSP_RTOS
#define BSP_RTOS

#include "bsp_system.h"

void freertos_test_task(void *argument);
void lvgl_task(void *argument);

#endif
