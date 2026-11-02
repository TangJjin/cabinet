/*
 * 文件用途：存储与升级适配：TF 包写入 QSPI、配置双副本保存，以及 Bootloader 的内部 Flash 擦写接口。
 * 编译归属：CM7 应用、CM7 Bootloader。
 */
#include "bsp_board.h"
#include "bsp_qspi.h"
#include "bsp_update.h"
#include "ff.h"
#include <stddef.h>

static int port_read(void *ctx, int space, uint32_t a, void *p, size_t n)
{
    (void)ctx;
    if (space == 0)
        return bsp_qspi_read(a, p, n);
    if (a < 0x08000000UL || a > 0x08200000UL || n > 0x08200000UL - a)
        return -1;
    memcpy(p, (const void *)a, n);
    return 0;
}
#ifdef CABINET_BOOTLOADER
static int flash_bounds(uint32_t a, size_t n)
{
    return (a >= CABINET_CM7_BASE && a <= 0x08100000UL && n <= 0x08100000UL - a) ||
           (a >= CABINET_CM4_BASE && a <= 0x08200000UL && n <= 0x08200000UL - a);
}
#endif
/* space=0 操作外部 QSPI；内部应用分区的擦写仅在 CABINET_BOOTLOADER 构建中开放。 */
static int port_erase(void *ctx, int space, uint32_t a, size_t n)
{
    (void)ctx;
    if (space == 0)
        return bsp_qspi_erase(a, n);
#ifdef CABINET_BOOTLOADER
    if (!flash_bounds(a, n) || (a & 0x1FFFFUL) || (n & 0x1FFFFUL))
        return -1;
    if (HAL_FLASH_Unlock() != HAL_OK)
        return -1;
    while (n)
    {
        FLASH_EraseInitTypeDef e = {0};
        uint32_t error = 0;
        e.TypeErase = FLASH_TYPEERASE_SECTORS;
        e.Banks = a < 0x08100000UL ? FLASH_BANK_1 : FLASH_BANK_2;
        e.Sector = (a & 0xFFFFFUL) / 0x20000UL;
        e.NbSectors = 1;
        e.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        bsp_storage_progress();
        if (HAL_FLASHEx_Erase(&e, &error) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return -1;
        }
        a += 0x20000UL;
        n -= 0x20000UL;
    }
    HAL_FLASH_Lock();
    return 0;
#else
    (void)a;
    (void)n;
    return -1; /* Running applications never erase internal firmware. */
#endif
}

static int port_write(void *ctx, int space, uint32_t a, const void *p, size_t n)
{
    (void)ctx;
    if (space == 0)
        return bsp_qspi_write(a, p, n);
#ifdef CABINET_BOOTLOADER
    if (!flash_bounds(a, n) || (a & 31U) || (n & 31U))
        return -1;
    if (HAL_FLASH_Unlock() != HAL_OK)
        return -1;
    while (n)
    {
        __attribute__((aligned(32))) uint32_t word[8];
        memcpy(word, p, 32);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, a, (uint32_t)word) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return -1;
        }
        a += 32;
        p = (const uint8_t *)p + 32;
        n -= 32;
        bsp_storage_progress();
    }
    HAL_FLASH_Lock();
    return 0;
#else
    (void)a;
    (void)p;
    (void)n;
    return -1;
#endif
}

static void port_progress(void *ctx)
{
    (void)ctx;
    bsp_storage_progress();
}

/* 通过函数指针把通用升级状态机连接到本板存储驱动，状态机本身不直接调用 HAL。 */
const CabinetBootOps cabinet_boot_ops = {port_read, port_erase, port_write, port_progress, 0};

/* 应用阶段仅把 TF 升级包完整写入 QSPI 并提交请求；重启后的 Bootloader 才安装内部双核固件。 */
int bsp_update_from_card(const char *path)
{
    FIL file;
    CabinetPackage h, check;
    CabinetBootRecord r;
    uint8_t buffer[1024];
    UINT got;
    uint32_t remaining, address;
    int result = -1;
    if (cabinet_boot_load(&cabinet_boot_ops, &r) || r.state != CAB_BOOT_STABLE)
        return -2;
    if (f_open(&file, path, FA_READ) != FR_OK)
        return -3;
    if (f_read(&file, &h, sizeof(h), &got) != FR_OK || got != sizeof(h) ||
        !cabinet_package_valid(&h) || f_size(&file) != (FSIZE_t)(64U + h.payload_size) ||
        h.version == r.installed.version)
        goto done;
    if (bsp_qspi_erase(CAB_STAGE, CAB_SLOT_SIZE))
        goto done;
    remaining = h.payload_size;
    address = CAB_STAGE + 64;
    while (remaining)
    {
        UINT n = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
        if (f_read(&file, buffer, n, &got) != FR_OK || got != n ||
            bsp_qspi_write(address, buffer, n))
            goto done;
        remaining -= n;
        address += n;
        bsp_storage_progress();
    }
    if (bsp_qspi_write(CAB_STAGE, &h, sizeof(h)) ||
        cabinet_boot_package_check(&cabinet_boot_ops, CAB_STAGE, &check))
        goto done;
    result = cabinet_boot_request(&cabinet_boot_ops);
done:
    (void)f_close(&file);
    return result;
}

typedef struct
{
    uint32_t magic, sequence;
    CabinetConfig config;
    uint32_t crc, commit;
} ConfigRecord;

static int config_valid(const ConfigRecord *r)
{
    return r->magic == 0x43414243UL && r->commit == 0xA55AC33CUL &&
           cabinet_config_valid(&r->config) &&
           r->crc == cabinet_crc32(r, offsetof(ConfigRecord, crc));
}

static int config_read(ConfigRecord *r)
{
    ConfigRecord a, b;
    int va, vb;
    if (bsp_qspi_read(CAB_CONFIG_A, &a, sizeof(a)) || bsp_qspi_read(CAB_CONFIG_B, &b, sizeof(b)))
        return -1;
    va = config_valid(&a);
    vb = config_valid(&b);
    if (!va && !vb)
        return 1;
    *r = va && (!vb || (int32_t)(a.sequence - b.sequence) > 0) ? a : b;
    return 0;
}

int bsp_config_load(CabinetConfig *c)
{
    ConfigRecord r;
    int status = config_read(&r);
    if (!status)
        *c = r.config;
    else
        cabinet_config_default(c);
    return status;
}

/* 配置采用 A/B 双副本：先写内容和 CRC，最后写 commit；掉电时仍保留上一份已提交记录。 */
int bsp_config_save(const CabinetConfig *c)
{
    ConfigRecord r, check;
    int status;
    uint32_t address;
    if (!cabinet_config_valid(c))
        return -1;
    memset(&r, 0, sizeof(r));
    status = config_read(&r);
    if (status < 0)
        return -1;
    r.sequence = status == 0 ? r.sequence + 1U : 1U;
    r.magic = 0x43414243UL;
    r.config = *c;
    r.crc = cabinet_crc32(&r, offsetof(ConfigRecord, crc));
    r.commit = 0xA55AC33CUL;
    address = (r.sequence & 1U) ? CAB_CONFIG_A : CAB_CONFIG_B;
    if (bsp_qspi_erase(address, 4096) ||
        bsp_qspi_write(address, &r, offsetof(ConfigRecord, commit)) ||
        bsp_qspi_write(address + offsetof(ConfigRecord, commit), &r.commit, 4) ||
        bsp_qspi_read(address, &check, sizeof(check)) || memcmp(&r, &check, sizeof(r)))
        return -1;
    return 0;
}
