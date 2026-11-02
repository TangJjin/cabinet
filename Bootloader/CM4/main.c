/*
 * 文件用途：CM4 固定启动门：在 STOP 中等待 HSEM 15，校验 CM7 放行信息后跳转 CM4 应用。
 * 编译归属：CM4 启动门。
 */
#include "bsp_board.h"

/* CM4 启动门不运行 RTOS；等待事件唤醒后同时检查 magic、地址反码和向量表，满足条件才进入采集应用。 */
int main(void)
{
    CabinetGate *gate = (CabinetGate *)CABINET_GATE_ADDRESS;
    __HAL_RCC_HSEM_CLK_ENABLE();
    for (;;)
    {
        HAL_HSEM_ActivateNotification(1UL << CABINET_GATE_HSEM);
        HAL_PWREx_ClearPendingEvent();
        HAL_PWREx_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFE, PWR_D2_DOMAIN);
        __HAL_HSEM_CLEAR_FLAG(1UL << CABINET_GATE_HSEM);
        __DMB();
        if (gate->magic == CABINET_GATE_MAGIC && gate->address == CABINET_CM4_BASE &&
            gate->inverse == ~CABINET_CM4_BASE)
        {
            uint32_t stack = *(const uint32_t *)CABINET_CM4_BASE;
            uint32_t reset = *(const uint32_t *)(CABINET_CM4_BASE + 4);
            if (cabinet_vector_valid(4, CABINET_CM4_BASE, CABINET_APP_CAPACITY, stack, reset))
            {
                gate->magic = 0;
                __DSB();
                bsp_jump(CABINET_CM4_BASE);
            }
        }
    }
}

void Error_Handler(void)
{
    bsp_fatal();
}
