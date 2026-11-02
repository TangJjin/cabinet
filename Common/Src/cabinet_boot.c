/*
 * 文件用途：双核升级状态机：启动记录双副本、镜像校验、配套备份/安装、试运行确认和回滚。
 * 编译归属：CM7 应用、CM7 Bootloader。
 */
#include "cabinet_boot.h"
#include <string.h>
#include <stddef.h>
#define RECORD_MAGIC 0x43414252UL
#define RECORD_COMMIT 0xA55AC33CUL

static int read_at(const CabinetBootOps *o, int s, uint32_t a, void *p, size_t n)
{
    return o->read(o->context, s, a, p, n);
}

static int erase_at(const CabinetBootOps *o, int s, uint32_t a, size_t n)
{
    return o->erase(o->context, s, a, n);
}

static int write_at(const CabinetBootOps *o, int s, uint32_t a, const void *p, size_t n)
{
    return o->write(o->context, s, a, p, n);
}

static void progress(const CabinetBootOps *o)
{
    if (o->progress)
        o->progress(o->context);
}

static int record_valid(const CabinetBootRecord *r)
{
    return r->magic == RECORD_MAGIC && r->commit == RECORD_COMMIT && r->state <= CAB_BOOT_RESTORE &&
           r->attempts <= 3 && r->has_backup <= 1 && cabinet_package_valid(&r->installed) &&
           r->crc == cabinet_crc32(r, offsetof(CabinetBootRecord, crc));
}

int cabinet_boot_load(const CabinetBootOps *o, CabinetBootRecord *r)
{
    CabinetBootRecord a, b;
    int va, vb;
    if (read_at(o, 0, CAB_BOOT_META_A, &a, sizeof(a)) ||
        read_at(o, 0, CAB_BOOT_META_B, &b, sizeof(b)))
        return -1;
    va = record_valid(&a);
    vb = record_valid(&b);
    if (!va && !vb)
        return 1;
    *r = va && (!vb || (int32_t)(a.sequence - b.sequence) > 0) ? a : b;
    return 0;
}

/* 只擦写新的记录副本，最后写提交字；启动时用校验和序号选择最近一份完整记录。 */
int cabinet_boot_save(const CabinetBootOps *o, CabinetBootRecord *r)
{
    CabinetBootRecord old, check;
    uint32_t address;
    int found = cabinet_boot_load(o, &old);
    if (found < 0)
        return -1;
    r->sequence = found == 0 ? old.sequence + 1U : 1U;
    r->magic = RECORD_MAGIC;
    r->commit = RECORD_COMMIT;
    r->crc = cabinet_crc32(r, offsetof(CabinetBootRecord, crc));
    address = (r->sequence & 1U) ? CAB_BOOT_META_A : CAB_BOOT_META_B;
    /* Never erase the only committed copy. Commit word is programmed last. */
    if (erase_at(o, 0, address, 4096) ||
        write_at(o, 0, address, r, offsetof(CabinetBootRecord, commit)) ||
        write_at(o, 0, address + offsetof(CabinetBootRecord, commit), &r->commit, 4) ||
        read_at(o, 0, address, &check, sizeof(check)) || !record_valid(&check) ||
        memcmp(r, &check, sizeof(check)))
        return -1;
    return 0;
}

static int crc_at(const CabinetBootOps *o, int space, uint32_t address, uint32_t n,
                  uint32_t *result)
{
    uint8_t buf[1024];
    uint32_t crc = 0xFFFFFFFFUL;
    while (n)
    {
        uint32_t m = n > sizeof(buf) ? sizeof(buf) : n;
        if (read_at(o, space, address, buf, m))
            return -1;
        crc = cabinet_crc32_update(crc, buf, m);
        address += m;
        n -= m;
        progress(o);
    }
    *result = ~crc;
    return 0;
}

static int image_check(const CabinetBootOps *o, int space, uint32_t source, uint32_t target,
                       uint32_t size, uint32_t crc, uint32_t version, uint32_t core)
{
    uint32_t v[2], actual;
    CabinetImageInfo info;
    if (read_at(o, space, source, v, sizeof(v)) ||
        !cabinet_vector_valid(core, target, size, v[0], v[1]) ||
        read_at(o, space, source + CABINET_INFO_OFFSET, &info, sizeof(info)))
        return -1;
    if (info.magic != CABINET_INFO_MAGIC || info.board != CABINET_BOARD_ID ||
        info.version != version || info.core != core || info.protocol != CABINET_PROTOCOL)
        return -1;
    if (crc_at(o, space, source, size, &actual) || actual != crc)
        return -1;
    return 0;
}

int cabinet_boot_package_check(const CabinetBootOps *o, uint32_t slot, CabinetPackage *h)
{
    if (slot != CAB_STAGE && slot != CAB_BACKUP)
        return -1;
    if (read_at(o, 0, slot, h, sizeof(*h)) || !cabinet_package_valid(h))
        return -1;
    if (image_check(o, 0, slot + 64, h->cm7_address, h->cm7_size, h->cm7_crc, h->version, 7) ||
        image_check(o, 0, slot + 64 + h->cm7_size, h->cm4_address, h->cm4_size, h->cm4_crc,
                    h->version, 4))
        return -1;
    return 0;
}

static int flash_check(const CabinetBootOps *o, const CabinetPackage *h)
{
    return image_check(o, 1, h->cm7_address, h->cm7_address, h->cm7_size, h->cm7_crc, h->version,
                       7) ||
           image_check(o, 1, h->cm4_address, h->cm4_address, h->cm4_size, h->cm4_crc, h->version,
                       4);
}

static int describe_factory(const CabinetBootOps *o, CabinetPackage *p)
{
    CabinetImageInfo m7, m4;
    if (read_at(o, 1, CABINET_CM7_BASE + CABINET_INFO_OFFSET, &m7, sizeof(m7)) ||
        read_at(o, 1, CABINET_CM4_BASE + CABINET_INFO_OFFSET, &m4, sizeof(m4)))
        return -1;
    if (m7.magic != CABINET_INFO_MAGIC || m4.magic != CABINET_INFO_MAGIC ||
        m7.version != m4.version)
        return -1;
    memset(p, 0, sizeof(*p));
    p->magic = CABINET_PACKAGE_MAGIC;
    p->format = CABINET_PACKAGE_FORMAT;
    p->board = CABINET_BOARD_ID;
    p->version = m7.version;
    p->cm7_address = CABINET_CM7_BASE;
    p->cm4_address = CABINET_CM4_BASE;
    p->cm7_size = p->cm4_size = CABINET_APP_CAPACITY;
    p->payload_size = 2U * CABINET_APP_CAPACITY;
    if (crc_at(o, 1, p->cm7_address, p->cm7_size, &p->cm7_crc) ||
        crc_at(o, 1, p->cm4_address, p->cm4_size, &p->cm4_crc))
        return -1;
    p->header_crc = cabinet_crc32(p, 60);
    return flash_check(o, p) ? -1 : 0;
}

/* 将当前 CM7/CM4 配对镜像一起备份到 QSPI，完整数据写好后才提交包头。 */
static int backup(const CabinetBootOps *o, const CabinetPackage *installed)
{
    uint8_t buf[1024];
    unsigned core;
    CabinetPackage h = *installed, check;
    if (flash_check(o, &h) || erase_at(o, 0, CAB_BACKUP, CAB_SLOT_SIZE))
        return -1;
    for (core = 0; core < 2; ++core)
    {
        uint32_t from = core ? h.cm4_address : h.cm7_address;
        uint32_t to = CAB_BACKUP + 64 + (core ? h.cm7_size : 0), n = core ? h.cm4_size : h.cm7_size;
        while (n)
        {
            uint32_t m = n > sizeof(buf) ? sizeof(buf) : n;
            if (read_at(o, 1, from, buf, m) || write_at(o, 0, to, buf, m))
                return -1;
            from += m;
            to += m;
            n -= m;
            progress(o);
        }
    }
    /* Header last: an interrupted backup can never appear complete. */
    if (write_at(o, 0, CAB_BACKUP, &h, sizeof(h)))
        return -1;
    return cabinet_boot_package_check(o, CAB_BACKUP, &check);
}

/* 从已校验的 QSPI 包重新安装整对镜像；发生掉电后可从头重做，避免运行半套固件。 */
static int install(const CabinetBootOps *o, uint32_t slot, CabinetPackage *h)
{
    uint8_t buf[1024];
    unsigned core;
    if (cabinet_boot_package_check(o, slot, h))
        return -1;
    for (core = 0; core < 2; ++core)
    {
        uint32_t target = core ? h->cm4_address : h->cm7_address;
        uint32_t source = slot + 64 + (core ? h->cm7_size : 0),
                 n = core ? h->cm4_size : h->cm7_size;
        if (erase_at(o, 1, target, CABINET_APP_CAPACITY))
            return -1;
        while (n)
        {
            uint32_t m = n > sizeof(buf) ? sizeof(buf) : n, padded = (m + 31U) & ~31U;
            memset(buf, 0xFF, sizeof(buf));
            if (read_at(o, 0, source, buf, m) || write_at(o, 1, target, buf, padded))
                return -1;
            target += m;
            source += m;
            n -= m;
            progress(o);
        }
    }
    return flash_check(o, h) ? -1 : 0;
}

int cabinet_boot_request(const CabinetBootOps *o)
{
    CabinetBootRecord r;
    CabinetPackage h;
    if (cabinet_boot_load(o, &r) || r.state != CAB_BOOT_STABLE ||
        cabinet_boot_package_check(o, CAB_STAGE, &h) || h.version == r.installed.version)
        return -1;
    r.state = CAB_BOOT_REQUEST;
    r.attempts = 0;
    return cabinet_boot_save(o, &r);
}

int cabinet_boot_confirm(const CabinetBootOps *o, uint32_t version)
{
    CabinetBootRecord r;
    if (cabinet_boot_load(o, &r) || r.installed.version != version)
        return -1;
    if (r.state == CAB_BOOT_STABLE)
        return 0;
    if (r.state != CAB_BOOT_TRIAL)
        return -1;
    r.state = CAB_BOOT_STABLE;
    r.attempts = 0;
    r.result = CAB_BOOT_CONFIRMED;
    return cabinet_boot_save(o, &r);
}

/* 状态顺序：稳定 -> 请求 -> 安装 -> 试运行 -> 确认稳定；试运行连续三次未确认则在下次启动恢复旧版本。 */
int cabinet_boot_run(const CabinetBootOps *o)
{
    CabinetBootRecord r;
    CabinetPackage h;
    int found = cabinet_boot_load(o, &r);
    if (found < 0)
        return -1;
    if (found > 0)
    {
        memset(&r, 0, sizeof(r));
        if (describe_factory(o, &r.installed) || cabinet_boot_save(o, &r))
            return -1;
    }
    if (r.state == CAB_BOOT_REQUEST)
    {
        if (cabinet_boot_package_check(o, CAB_STAGE, &h))
        {
            r.state = CAB_BOOT_STABLE;
            r.result = CAB_BOOT_REJECTED;
            if (cabinet_boot_save(o, &r))
                return -1;
        }
        else
        {
            if (backup(o, &r.installed))
                return -1;
            r.has_backup = 1;
            r.state = CAB_BOOT_INSTALL;
            if (cabinet_boot_save(o, &r))
                return -1;
        }
    }
    if (r.state == CAB_BOOT_INSTALL)
    {
        /* Restart the entire pair after a power cut. Never inspect partially written flash. */
        if (install(o, CAB_STAGE, &h))
        {
            r.state = CAB_BOOT_RESTORE;
            if (cabinet_boot_save(o, &r))
                return -1;
        }
        else
        {
            r.installed = h;
            r.state = CAB_BOOT_TRIAL;
            r.attempts = 0;
            if (cabinet_boot_save(o, &r))
                return -1;
        }
    }
    if (r.state == CAB_BOOT_TRIAL)
    {
        if (r.attempts >= 3 || flash_check(o, &r.installed))
        {
            r.state = CAB_BOOT_RESTORE;
            if (cabinet_boot_save(o, &r))
                return -1;
        }
        else
        {
            ++r.attempts;
            return cabinet_boot_save(o, &r);
        }
    }
    if (r.state == CAB_BOOT_STABLE && flash_check(o, &r.installed))
    {
        if (!r.has_backup)
            return -1;
        r.state = CAB_BOOT_RESTORE;
        if (cabinet_boot_save(o, &r))
            return -1;
    }
    if (r.state == CAB_BOOT_RESTORE)
    {
        if (!r.has_backup || install(o, CAB_BACKUP, &h))
            return -1;
        r.installed = h;
        r.state = CAB_BOOT_STABLE;
        r.attempts = 0;
        r.result = CAB_BOOT_ROLLED_BACK;
        return cabinet_boot_save(o, &r);
    }
    return r.state == CAB_BOOT_STABLE ? 0 : -1;
}
