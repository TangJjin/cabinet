/*
 * 文件用途：板级基础：系统时钟、MPU、看门狗、报警输出、CM4 放行和应用跳转；按编译宏选择核及启动阶段。
 * 编译归属：CM7 应用、CM4 应用、CM7 Bootloader、CM4 启动门。
 */
#include "bsp_board.h"

/* CM7 启动 IWDG1；应用运行后由监督任务决定是否喂狗，Bootloader 则在自己的启动/存储流程维护。 */
void bsp_watchdog_start(void)
{
#ifdef CORE_CM7
    IWDG_HandleTypeDef h = {0};
    h.Instance = IWDG1;
    h.Init.Prescaler = IWDG_PRESCALER_256;
    h.Init.Reload = 4095;
    h.Init.Window = IWDG_WINDOW_DISABLE;
    (void)HAL_IWDG_Init(&h);
#endif
}

void bsp_watchdog_feed(void)
{
#ifdef CORE_CM7
    IWDG1->KR = IWDG_KEY_RELOAD;
#endif
}

void bsp_fatal(void)
{
    __disable_irq();
    for (;;)
    {
    }
}

#ifdef CORE_CM7
void bsp_clock_init(void)
{
    RCC_OscInitTypeDef o = {0};
    RCC_ClkInitTypeDef c = {0};
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
    {
    }
    o.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    o.HSEState = RCC_HSE_ON;
    o.PLL.PLLState = RCC_PLL_ON;
    o.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    o.PLL.PLLM = 5;
    o.PLL.PLLN = 192;
    o.PLL.PLLP = 2;
    o.PLL.PLLQ = 8;
    o.PLL.PLLR = 2; /* PLL1Q 120 MHz: SPI/SDMMC kernel. */
    o.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
    o.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    if (HAL_RCC_OscConfig(&o) != HAL_OK)
        bsp_fatal();
    c.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 |
                  RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    c.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    c.SYSCLKDivider = RCC_SYSCLK_DIV1;
    c.AHBCLKDivider = RCC_HCLK_DIV2;
    c.APB3CLKDivider = RCC_APB3_DIV2;
    c.APB1CLKDivider = RCC_APB1_DIV2;
    c.APB2CLKDivider = RCC_APB2_DIV2;
    c.APB4CLKDivider = RCC_APB4_DIV2;
    if (HAL_RCC_ClockConfig(&c, FLASH_LATENCY_4) != HAL_OK)
        bsp_fatal();
    SystemCoreClockUpdate();
}

/* 共享 SRAM4 配置为不可缓存、可共享，避免两核读到各自缓存中的旧消息；DMB 只负责访问顺序。 */
void bsp_mpu_init(void)
{
    MPU_Region_InitTypeDef r = {0};
    HAL_MPU_Disable();
    r.Enable = MPU_REGION_ENABLE;
    r.Number = MPU_REGION_NUMBER0;
    r.BaseAddress = 0;
    r.Size = MPU_REGION_SIZE_4GB;
    r.SubRegionDisable = 0x87;
    r.AccessPermission = MPU_REGION_NO_ACCESS;
    r.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    r.IsShareable = MPU_ACCESS_SHAREABLE;
    HAL_MPU_ConfigRegion(&r);
    r.Number = MPU_REGION_NUMBER1;
    r.BaseAddress = 0xD0000000;
    r.Size = MPU_REGION_SIZE_32MB;
    r.SubRegionDisable = 0;
    r.AccessPermission = MPU_REGION_FULL_ACCESS;
    r.TypeExtField = MPU_TEX_LEVEL1;
    HAL_MPU_ConfigRegion(&r);
    /* OpenAMP resource table, vrings and boot mailbox: normal, shareable, noncached. */
    r.Number = MPU_REGION_NUMBER2;
    r.BaseAddress = 0x38000000;
    r.Size = MPU_REGION_SIZE_64KB;
    HAL_MPU_ConfigRegion(&r);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

void bsp_board_outputs_init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    /* Board RGB LED is common-anode: PA3/PB1 are active low. */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
    HAL_GPIO_WritePin(CAB_BUZZER_PORT, CAB_BUZZER_PIN, GPIO_PIN_RESET);
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pull = GPIO_NOPULL;
    g.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_1;
    HAL_GPIO_Init(GPIOB, &g);
    g.Pin = CAB_BUZZER_PIN;
    HAL_GPIO_Init(CAB_BUZZER_PORT, &g);
}

void bsp_board_alarm_outputs(uint32_t alarms, int audible, uint32_t now)
{
    int fault = (alarms & (CAB_ALARM_SENSOR | CAB_ALARM_IPC)) != 0;
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, ((now / 500U) & 1U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1,
                      (fault ? ((now / 200U) & 1U) : (alarms != 0)) ? GPIO_PIN_RESET
                                                                    : GPIO_PIN_SET);
    HAL_GPIO_WritePin(CAB_BUZZER_PORT, CAB_BUZZER_PIN,
                      (audible && ((now / 500U) & 1U)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* 先写应用地址和反码，最后提交 magic，再释放 HSEM 15；必须在 CM7 的 OpenAMP 资源准备好后调用。 */
void bsp_cm4_release(void)
{
    CabinetGate *g = (CabinetGate *)CABINET_GATE_ADDRESS;
    g->address = CABINET_CM4_BASE;
    g->inverse = ~CABINET_CM4_BASE;
    __DMB();
    g->magic = CABINET_GATE_MAGIC;
    __DSB();
    __HAL_RCC_HSEM_CLK_ENABLE();
    if (HAL_HSEM_FastTake(CABINET_GATE_HSEM) == HAL_OK)
        HAL_HSEM_Release(CABINET_GATE_HSEM, 0);
}
#endif

/* The final stack change and branch must not return through a C stack frame. */
#if defined(__CC_ARM)
/* ARM Compiler 5 内联汇编使用换行分隔指令，禁止格式化器合并这些行。 */
// clang-format off
__asm static void enter_image(uint32_t stack, uint32_t reset)
{
    MSR MSP, r0
    MOVS r2, #0
    MSR CONTROL, r2
    ISB
    CPSIE i
    BX r1
}
// clang-format on
#else
__attribute__((naked, noreturn)) static void enter_image(uint32_t stack, uint32_t reset)
{
    __asm volatile("msr msp, r0\n movs r2, #0\n msr control, r2\n isb\n cpsie i\n bx r1\n");
}
#endif
/* 跳转另一镜像前清理中断/滴答并切换向量表，最后由无栈帧汇编切换 MSP 并进入复位入口。 */
void bsp_jump(uint32_t address)
{
    unsigned i;
    uint32_t stack = *(const uint32_t *)address, reset = *(const uint32_t *)(address + 4);
    __disable_irq();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    for (i = 0; i < 8; ++i)
    {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    SCB->VTOR = address;
    __DSB();
    __ISB();
    enter_image(stack, reset);
}
