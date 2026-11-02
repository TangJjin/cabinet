/*
 * 文件用途：CM7 固定 Bootloader：等待 CM4 启动门停驻，校验或安装双核固件，成功后跳转 CM7 应用。
 * 编译归属：CM7 Bootloader。
 */
#include "bsp_board.h"
#include "bsp_qspi.h"
#include "bsp_update.h"
/* Keil Watch: 1=entry, 2=CM4 wait, 3=clock, 4=QSPI, 5=images, 6=jump;
 * 0xE1=QSPI failure, 0xE2=image/metadata failure. Result preserves return code. */
volatile uint32_t cabinet_boot_stage;
volatile int32_t cabinet_boot_result;

/* 先让 CM4 留在固定启动门；升级状态机校验通过后只跳转 CM7 应用，CM4 由应用准备 IPC 后再放行。 */
int main(void)
{
    uint32_t start;
    CabinetGate *gate = (CabinetGate *)CABINET_GATE_ADDRESS;
    cabinet_boot_stage = 1;
    HAL_Init();
    gate->magic = 0;
    __DSB();
    cabinet_boot_stage = 2;
    start = HAL_GetTick();
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) != RESET)
    {
        if ((uint32_t)(HAL_GetTick() - start) > 2000U)
            bsp_fatal();
    }
    cabinet_boot_stage = 3;
    bsp_clock_init();
    bsp_board_outputs_init();
    bsp_watchdog_start();
    cabinet_boot_stage = 4;
    cabinet_boot_result = bsp_qspi_init();
    if (!cabinet_boot_result)
    {
        cabinet_boot_stage = 5;
        cabinet_boot_result = cabinet_boot_run(&cabinet_boot_ops);
        if (!cabinet_boot_result)
        {
            cabinet_boot_stage = 6;
            bsp_jump(CABINET_CM7_BASE);
        }
        cabinet_boot_stage = 0xE2;
    }
    else
        cabinet_boot_stage = 0xE1;
    /* Keep both applications stopped on invalid images/storage. ST-LINK remains available. */
    for (;;)
    {
        bsp_board_alarm_outputs(CAB_ALARM_IPC, 0, HAL_GetTick());
        bsp_watchdog_feed();
        HAL_Delay(50);
    }
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void HardFault_Handler(void)
{
    for (;;)
    {
    }
}

void Error_Handler(void)
{
    bsp_fatal();
}
