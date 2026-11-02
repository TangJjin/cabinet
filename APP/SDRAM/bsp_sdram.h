/*
 * 文件用途：接口与类型声明：外部 SDRAM BSP：执行芯片上电命令序列并配置刷新，提供基础内存读写接口。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef BSP_SDRAM__
#define BSP_SDRAM__

#include "bsp_system.h"

#define SDRAM_BASE_ADDR 0xD0000000UL
#define SDRAM_MODEREG_BURST_LENGTH_1 0x0000U
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL 0x0000U
#define SDRAM_MODEREG_CAS_LATENCY_2 0x0020U
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD 0x0000U
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE 0x0200U

#define SDRAM_SIZE_BYTES (32UL * 1024UL * 1024UL)

uint8_t sdram_init(SDRAM_HandleTypeDef *hsdram);

void sdram_write(uint8_t *buffer, uint32_t addr, uint32_t n);
void sdram_read(uint8_t *buffer, uint32_t addr, uint32_t n);

#endif
