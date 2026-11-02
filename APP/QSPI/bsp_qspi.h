/*
 * 文件用途：接口与类型声明：外部 W25Q256 Flash 驱动：初始化、读取、按页写入及扇区擦除，供配置和固件暂存使用。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef BSP_QSPI_H
#define BSP_QSPI_H
#include <stdint.h>
#include <stddef.h>
/* Watch: 0=ready; -1=clock, -2=HAL, -3=reset, -4=ID read, -5=ID, -6=SR3 read. */
extern volatile uint32_t bsp_qspi_jedec_id;
extern volatile int32_t bsp_qspi_init_status;
extern volatile uint32_t bsp_qspi_status3;
int bsp_qspi_init(void);
int bsp_qspi_read(uint32_t address, void *data, size_t length);
int bsp_qspi_write(uint32_t address, const void *data, size_t length);
int bsp_qspi_erase(uint32_t address, size_t length);
/* Hook used to keep the storage task heartbeat alive during bounded long operations. */
void bsp_storage_progress(void);
#endif
