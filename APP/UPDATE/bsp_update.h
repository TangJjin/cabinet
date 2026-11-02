/*
 * 文件用途：接口与类型声明：存储与升级适配：TF 包写入 QSPI、配置双副本保存，以及 Bootloader 的内部 Flash 擦写接口。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef BSP_UPDATE_H
#define BSP_UPDATE_H
#include "cabinet_boot.h"
extern const CabinetBootOps cabinet_boot_ops;
int bsp_update_from_card(const char *path);
int bsp_config_load(CabinetConfig *config);
int bsp_config_save(const CabinetConfig *config);
#endif
