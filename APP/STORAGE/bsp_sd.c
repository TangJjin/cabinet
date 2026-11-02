/*
 * 文件用途：TF 卡块设备：通过 SDMMC1 初始化卡、读写 512 字节逻辑扇区并等待卡完成操作。
 * 编译归属：CM7 应用、CM7 Bootloader。
 */
#include "bsp_board.h"
#include "bsp_qspi.h"
#include "bsp_sd.h"
static SD_HandleTypeDef sd;
static int ready;
static uint32_t sectors;

/* 先以单线初始化卡，再切 SDMMC1 四线；这里只识别块设备，FAT 文件系统挂载由 storage_task 完成。 */
int bsp_sd_init(void)
{
    GPIO_InitTypeDef g = {0};
    RCC_PeriphCLKInitTypeDef c = {0};
    HAL_SD_CardInfoTypeDef info;
    ready = 0;
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_SDMMC1_CLK_ENABLE();
    __HAL_RCC_SDMMC1_FORCE_RESET();
    __HAL_RCC_SDMMC1_RELEASE_RESET();
    c.PeriphClockSelection = RCC_PERIPHCLK_SDMMC;
    c.SdmmcClockSelection = RCC_SDMMCCLKSOURCE_PLL;
    if (HAL_RCCEx_PeriphCLKConfig(&c) != HAL_OK)
        return -1;
    g.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF12_SDIO1;
    HAL_GPIO_Init(GPIOC, &g);
    g.Pin = GPIO_PIN_2;
    HAL_GPIO_Init(GPIOD, &g);
    memset(&sd, 0, sizeof(sd));
    sd.Instance = SDMMC1;
    sd.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
    sd.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    sd.Init.BusWide = SDMMC_BUS_WIDE_1B;
    sd.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    sd.Init.ClockDiv = 3; /* PLL1Q 120 MHz / (2*3) = 20 MHz. */
    if (HAL_SD_Init(&sd) != HAL_OK ||
        HAL_SD_ConfigWideBusOperation(&sd, SDMMC_BUS_WIDE_4B) != HAL_OK ||
        HAL_SD_GetCardInfo(&sd, &info) != HAL_OK)
        return -1;
    sectors = info.LogBlockNbr;
    ready = 1;
    return 0;
}

int bsp_sd_sync(void)
{
    uint32_t start = HAL_GetTick();
    while (ready && HAL_SD_GetCardState(&sd) != HAL_SD_CARD_TRANSFER)
    {
        bsp_storage_progress();
        if ((uint32_t)(HAL_GetTick() - start) >= 3000)
        {
            ready = 0;
            return -1;
        }
    }
    return ready ? 0 : -1;
}

/* sector/count 的单位是 512 字节逻辑扇区；这层处理原始数据块，文件名和目录由 FatFs 解释。 */
int bsp_sd_read(uint8_t *data, uint32_t sector, uint32_t count)
{
    if (!ready || !count || sector >= sectors || count > sectors - sector)
        return -1;
    if (HAL_SD_ReadBlocks(&sd, data, sector, count, 2000) != HAL_OK || bsp_sd_sync())
    {
        ready = 0;
        return -1;
    }
    bsp_storage_progress();
    return 0;
}

int bsp_sd_write(const uint8_t *data, uint32_t sector, uint32_t count)
{
    if (!ready || !count || sector >= sectors || count > sectors - sector)
        return -1;
    if (HAL_SD_WriteBlocks(&sd, (uint8_t *)data, sector, count, 2000) != HAL_OK || bsp_sd_sync())
    {
        ready = 0;
        return -1;
    }
    bsp_storage_progress();
    return 0;
}

int bsp_sd_ready(void)
{
    return ready;
}

void bsp_sd_forget(void)
{
    ready = 0;
}

uint32_t bsp_sd_sectors(void)
{
    return sectors;
}
