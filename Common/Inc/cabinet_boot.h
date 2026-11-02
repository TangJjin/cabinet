/*
 * 文件用途：接口与类型声明：双核升级状态机：启动记录双副本、镜像校验、配套备份/安装、试运行确认和回滚。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef CABINET_BOOT_H
#define CABINET_BOOT_H
#include "cabinet.h"
#define CAB_BOOT_META_A 0x00000000UL
#define CAB_BOOT_META_B 0x00001000UL
#define CAB_CONFIG_A 0x00002000UL
#define CAB_CONFIG_B 0x00003000UL
#define CAB_STAGE 0x00100000UL
#define CAB_BACKUP 0x00300000UL
#define CAB_SLOT_SIZE 0x00200000UL
#define CAB_SPACE_NOR 0
#define CAB_SPACE_FLASH 1

enum
{
    CAB_BOOT_STABLE,
    CAB_BOOT_REQUEST,
    CAB_BOOT_INSTALL,
    CAB_BOOT_TRIAL,
    CAB_BOOT_RESTORE
};

enum
{
    CAB_BOOT_NEW = 0,
    CAB_BOOT_CONFIRMED = 1,
    CAB_BOOT_ROLLED_BACK = 2,
    CAB_BOOT_REJECTED = 3
};

typedef struct
{
    uint32_t magic, sequence, state, attempts, result, has_backup;
    CabinetPackage installed;
    uint32_t crc, commit;
} CabinetBootRecord;

typedef struct
{
    int (*read)(void *, int, uint32_t, void *, size_t);
    int (*erase)(void *, int, uint32_t, size_t);
    int (*write)(void *, int, uint32_t, const void *, size_t);
    void (*progress)(void *);
    void *context;
} CabinetBootOps;

int cabinet_boot_load(const CabinetBootOps *ops, CabinetBootRecord *record);
int cabinet_boot_save(const CabinetBootOps *ops, CabinetBootRecord *record);
int cabinet_boot_package_check(const CabinetBootOps *ops, uint32_t slot, CabinetPackage *header);
int cabinet_boot_request(const CabinetBootOps *ops);
int cabinet_boot_confirm(const CabinetBootOps *ops, uint32_t version);
/* 0 = boot a validated pair; nonzero = stay in recovery. Never jumps itself. */
int cabinet_boot_run(const CabinetBootOps *ops);
#endif
