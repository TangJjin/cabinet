/*
 * 文件用途：外部 SDRAM BSP：执行芯片上电命令序列并配置刷新，提供基础内存读写接口。
 * 编译归属：CM7 应用。
 */
#include "bsp_sdram.h"

/**
 * @brief  初始化外部 SDRAM 芯片
 * @param  hsdram SDRAM 句柄，传入 &hsdram2
 * @retval HAL_OK：初始化成功
 *         HAL_ERROR：命令发送或刷新配置失败
 */
uint8_t sdram_init(SDRAM_HandleTypeDef *hsdram)
{
    FMC_SDRAM_CommandTypeDef command = {0}; /* 初始化参数为0 */
    /*
		FMC_SDRAM_CommandTypeDef参数内容：

		CommandMode              要发送的命令类型
		CommandTarget            发给 Bank1 还是 Bank2
		AutoRefreshNumber        自动刷新次数
		ModeRegisterDefinition   模式寄存器的值
	*/

    uint32_t mode_register;

    /* 所有初始化命令都发送到板载 SDRAM 所在的 FMC Bank 2。 */
    command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK2;
    command.AutoRefreshNumber = 1U;
    command.ModeRegisterDefinition = 0U;

    /* 1. 开启 SDRAM 时钟。 */
    command.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
    if (HAL_SDRAM_SendCommand(hsdram, &command, 0x1000U) != HAL_OK)
    {
        return 1;
    }

    /* 芯片上电后要求等待至少 200 us，这里等待 1 ms。 */
    HAL_Delay(1U);

    /* 2. 对所有内部 Bank 执行预充电。 */
    command.CommandMode = FMC_SDRAM_CMD_PALL;
    if (HAL_SDRAM_SendCommand(hsdram, &command, 0x1000U) != HAL_OK)
    {
        return 1;
    }

    /* 3. 执行 8 次自动刷新。 */
    command.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
    command.AutoRefreshNumber = 8U;
    if (HAL_SDRAM_SendCommand(hsdram, &command, 0x1000U) != HAL_OK)
    {
        return 1;
    }

    /* 4. 配置 SDRAM 模式寄存器。 */
    mode_register = SDRAM_MODEREG_BURST_LENGTH_1 | SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL |
                    SDRAM_MODEREG_CAS_LATENCY_2 | SDRAM_MODEREG_OPERATING_MODE_STANDARD |
                    SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;

    command.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
    command.AutoRefreshNumber = 1U;
    command.ModeRegisterDefinition = mode_register;
    if (HAL_SDRAM_SendCommand(hsdram, &command, 0x1000U) != HAL_OK)
    {
        return 1;
    }

    /*
	 * SDRAM 时钟：100 MHz
	 * 64 ms 内刷新 4096 行
	 * 64000 × 100 / 4096 - 20 ≈ 1542
	 */
    if (HAL_SDRAM_ProgramRefreshRate(hsdram, 1699U) != HAL_OK)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief       从SDRAM指定地址(SDRAM_BASE_ADDR + addr)连续写入多个数据
 * @param       buffer    : 字节指针
 * @param       addr      : 写入的地址
 * @param       n         : 字节长度
 * @retval      无
*/
void sdram_write(uint8_t *buffer, uint32_t addr, uint32_t n)
{
    volatile uint8_t *sdram_addr = (volatile uint8_t *)SDRAM_BASE_ADDR + addr;

    for (uint32_t i = 0U; i < n; i++)
    {
        *sdram_addr++ = *buffer++;
        __DSB();
    }
}

/**
 * @brief       从SDRAM指定地址(SDRAM_BASE_ADDR + addr)连续读取多个数据
 * @param       buffer    : 字节指针
 * @param       addr      : 读取的地址
 * @param       n         : 字节长度
 * @retval      无
*/
void sdram_read(uint8_t *buffer, uint32_t addr, uint32_t n)
{
    volatile const uint8_t *sdram_addr = (volatile const uint8_t *)SDRAM_BASE_ADDR + addr;

    for (uint32_t i = 0U; i < n; i++)
    {
        *buffer++ = *sdram_addr++;
        __DSB();
    }
}
