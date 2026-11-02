/*
 * 文件用途：外部 W25Q256 Flash 驱动：初始化、读取、按页写入及扇区擦除，供配置和固件暂存使用。
 * 编译归属：CM7 应用、CM7 Bootloader。
 */
#include "bsp_board.h"
#include "bsp_qspi.h"
static QSPI_HandleTypeDef qspi;
static int ready;
volatile uint32_t bsp_qspi_jedec_id;
volatile int32_t bsp_qspi_init_status;
volatile uint32_t bsp_qspi_status3;

static int init_result(int status)
{
    bsp_qspi_init_status = status;
    return status;
}

/* Bootloader 使用此弱实现维护看门狗；CM7 应用由 bsp_cabinet.c 的强实现只更新存储心跳。 */
__weak void bsp_storage_progress(void)
{
    bsp_watchdog_feed();
}

static int command(uint8_t op, uint32_t address, uint32_t n, uint32_t dummy)
{
    QSPI_CommandTypeDef c = {0};
    c.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    c.Instruction = op;
    c.AddressMode = address == 0xFFFFFFFFUL ? QSPI_ADDRESS_NONE : QSPI_ADDRESS_1_LINE;
    /* Dedicated 4-byte opcodes below ignore the persisted ADP/current ADS mode. */
    c.AddressSize = QSPI_ADDRESS_32_BITS;
    c.Address = address;
    c.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    c.DataMode = n ? QSPI_DATA_1_LINE : QSPI_DATA_NONE;
    c.NbData = n;
    c.DummyCycles = dummy;
    c.DdrMode = QSPI_DDR_MODE_DISABLE;
    c.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    c.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    return HAL_QSPI_Command(&qspi, &c, 1000) == HAL_OK ? 0 : -1;
}

static int wait_ready(uint32_t timeout)
{
    uint8_t status;
    uint32_t start = HAL_GetTick();
    do
    {
        if (command(5, 0xFFFFFFFFUL, 1, 0) || HAL_QSPI_Receive(&qspi, &status, 100) != HAL_OK)
            return -1;
        if (!(status & 1))
            return 0;
        bsp_storage_progress();
    } while ((uint32_t)(HAL_GetTick() - start) < timeout);
    return -1;
}

static int write_enable(void)
{
    uint8_t status;
    if (wait_ready(5000) || command(6, 0xFFFFFFFFUL, 0, 0) || command(5, 0xFFFFFFFFUL, 1, 0))
        return -1;
    if (HAL_QSPI_Receive(&qspi, &status, 100) != HAL_OK)
        return -1;
    return (status & 2) ? 0 : -1;
}

int bsp_qspi_init(void)
{
    GPIO_InitTypeDef g = {0};
    RCC_PeriphCLKInitTypeDef clk = {0};
    uint8_t id[3], status3;
    ready = 0;
    bsp_qspi_jedec_id = 0;
    bsp_qspi_status3 = 0;
    bsp_qspi_init_status = -100;
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_QSPI_CLK_ENABLE();
    __HAL_RCC_QSPI_FORCE_RESET();
    __HAL_RCC_QSPI_RELEASE_RESET();
    clk.PeriphClockSelection = RCC_PERIPHCLK_QSPI;
    clk.QspiClockSelection = RCC_QSPICLKSOURCE_D1HCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK)
        return init_result(-1);
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    /* PF10 CLK is AF9; AF10 on PF10 selects SAI4_D3 and supplies no QSPI clock. */
    g.Alternate = GPIO_AF9_QUADSPI;
    g.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOF, &g);
    g.Alternate = GPIO_AF10_QUADSPI;
    g.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    HAL_GPIO_Init(GPIOF, &g);
    g.Pin = GPIO_PIN_6;
    HAL_GPIO_Init(GPIOG, &g);
    memset(&qspi, 0, sizeof(qspi));
    qspi.Instance = QUADSPI;
    qspi.Init.ClockPrescaler = 7;
    qspi.Init.FifoThreshold = 4;
    qspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
    qspi.Init.FlashSize = 24;
    qspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_4_CYCLE;
    qspi.Init.ClockMode = QSPI_CLOCK_MODE_0;
    qspi.Init.FlashID = QSPI_FLASH_ID_1;
    qspi.Init.DualFlash = QSPI_DUALFLASH_DISABLE;
    if (HAL_QSPI_Init(&qspi) != HAL_OK)
        return init_result(-2);
    if (command(0x66, 0xFFFFFFFFUL, 0, 0) || command(0x99, 0xFFFFFFFFUL, 0, 0))
        return init_result(-3);
    HAL_Delay(2);
    if (command(0x9F, 0xFFFFFFFFUL, 3, 0) || HAL_QSPI_Receive(&qspi, id, 100) != HAL_OK)
        return init_result(-4);
    bsp_qspi_jedec_id = ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];
    if (id[0] != 0xEF || id[2] != 0x19)
        return init_result(-5); /* W25Q256 */
    if (command(0x15, 0xFFFFFFFFUL, 1, 0) || HAL_QSPI_Receive(&qspi, &status3, 100) != HAL_OK)
        return init_result(-6);
    bsp_qspi_status3 = status3; /* Diagnostic only: never rewrite ADP/QE/protection bits. */
    ready = 1;
    return init_result(0);
}

int bsp_qspi_read(uint32_t a, void *data, size_t n)
{
    if (!ready || a >= 0x1000000UL || n > 0x1000000UL - a)
        return -1;
    if (!n)
        return 0;
    if (wait_ready(5000) || command(0x0C, a, (uint32_t)n, 8))
        return -1;
    return HAL_QSPI_Receive(&qspi, data, 5000) == HAL_OK ? 0 : -1;
}

/* 按 256 字节页边界拆分写入；Flash 写前需由上层安排擦除，当前仅使用芯片低 16 MiB。 */
int bsp_qspi_write(uint32_t a, const void *data, size_t n)
{
    const uint8_t *p = data;
    if (!ready || a >= 0x1000000UL || n > 0x1000000UL - a)
        return -1;
    while (n)
    {
        size_t m = 256U - (a & 255U);
        if (m > n)
            m = n;
        if (write_enable() || command(0x12, a, (uint32_t)m, 0) ||
            HAL_QSPI_Transmit(&qspi, (uint8_t *)p, 1000) != HAL_OK || wait_ready(1000))
            return -1;
        a += m;
        p += m;
        n -= m;
        bsp_storage_progress();
    }
    return 0;
}

int bsp_qspi_erase(uint32_t a, size_t n)
{
    if (!ready || (a & 4095U) || (n & 4095U) || a >= 0x1000000UL || n > 0x1000000UL - a)
        return -1;
    while (n)
    {
        uint32_t block = (!(a & 65535U) && n >= 65536U) ? 65536U : 4096U;
        if (write_enable() || command(block == 65536U ? 0xDC : 0x21, a, 0, 0) || wait_ready(5000))
            return -1;
        a += block;
        n -= block;
        bsp_storage_progress();
    }
    return 0;
}
