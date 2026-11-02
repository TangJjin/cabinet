/*
 * 文件用途：接口与类型声明：TF 卡块设备：通过 SDMMC1 初始化卡、读写 512 字节逻辑扇区并等待卡完成操作。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef BSP_SD_H
#define BSP_SD_H
#include <stdint.h>
int bsp_sd_init(void);
int bsp_sd_read(uint8_t *data, uint32_t sector, uint32_t count);
int bsp_sd_write(const uint8_t *data, uint32_t sector, uint32_t count);
int bsp_sd_sync(void);
int bsp_sd_ready(void);
uint32_t bsp_sd_sectors(void);
void bsp_sd_forget(void);
#endif
