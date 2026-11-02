/*
 * 文件用途：当前应用中断入口：SysTick 同时服务 HAL/FreeRTOS，按核接入 HSEM 和 CM7 调试串口中断。
 * 编译归属：CM7 应用、CM4 应用。
 */
#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "bsp_board.h"

void NMI_Handler(void)
{
    bsp_fatal();
}

void HardFault_Handler(void)
{
    bsp_fatal();
}

void MemManage_Handler(void)
{
    bsp_fatal();
}

void BusFault_Handler(void)
{
    bsp_fatal();
}

void UsageFault_Handler(void)
{
    bsp_fatal();
}

void DebugMon_Handler(void)
{
}

/* SVC and PendSV are defined by the FreeRTOS port through FreeRTOSConfig.h. */
/* HAL 毫秒计数始终推进；只有本核调度器已经启动时，才把节拍交给 FreeRTOS。 */
void SysTick_Handler(void)
{
    extern void xPortSysTickHandler(void);
    HAL_IncTick();
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        xPortSysTickHandler();
}
#ifdef CORE_CM7
#include "usart.h"

void HSEM1_IRQHandler(void)
{
    HAL_HSEM_IRQHandler();
}

void DMA1_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_rx);
}

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
#else
void HSEM2_IRQHandler(void)
{
    HAL_HSEM_IRQHandler();
}
#endif
