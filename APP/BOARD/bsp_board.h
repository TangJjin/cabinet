/*
 * 文件用途：接口与类型声明：板级基础：系统时钟、MPU、看门狗、报警输出、CM4 放行和应用跳转；按编译宏选择核及启动阶段。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef BSP_BOARD_H
#define BSP_BOARD_H
#include "stm32h7xx_hal.h"
#include "cabinet.h"
#include <string.h>
#include <stdio.h>

/* Header pins: all are independent of FMC, DSI, touch, SDMMC1 and QSPI. */
#define CAB_BUZZER_PORT GPIOG
#define CAB_BUZZER_PIN GPIO_PIN_13
#define CAB_WATER_PORT GPIOC
#define CAB_WATER_PIN GPIO_PIN_13
#define CAB_RS485_DE_PORT GPIOG
#define CAB_RS485_DE_PIN GPIO_PIN_12

void bsp_clock_init(void);
void bsp_mpu_init(void);
void bsp_watchdog_start(void);
void bsp_watchdog_feed(void);
void bsp_board_outputs_init(void);
void bsp_board_alarm_outputs(uint32_t alarms, int audible, uint32_t now);
void bsp_cm4_release(void);
void bsp_jump(uint32_t address);
void bsp_fatal(void);
#endif
