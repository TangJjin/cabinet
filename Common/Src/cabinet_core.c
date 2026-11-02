/*
 * 文件用途：通用业务算法：CRC32、默认配置和校验、报警迟滞/延时/消音、升级包及向量表基本检查。
 * 编译归属：CM7 应用、CM4 应用、CM7 Bootloader、CM4 启动门。
 */
#include "cabinet.h"
#include <string.h>

uint32_t cabinet_crc32_update(uint32_t crc, const void *data, size_t size)
{
    const uint8_t *p = (const uint8_t *)data;
    while (size--)
    {
        unsigned i;
        crc ^= *p++;
        for (i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1U)));
    }
    return crc;
}

uint32_t cabinet_crc32(const void *data, size_t size)
{
    return ~cabinet_crc32_update(0xFFFFFFFFUL, data, size);
}

void cabinet_config_default(CabinetConfig *c)
{
    memset(c, 0, sizeof(*c));
    c->temp_high = 3500;
    c->temp_clear = 3300;
    c->humidity_high = 8000;
    c->humidity_clear = 7500;
    c->sensor_address = 1;
    c->simulated = 1;
    c->revision = 1;
}

int cabinet_config_valid(const CabinetConfig *c)
{
    return c->temp_high >= -3900 && c->temp_high <= 8000 && c->temp_clear >= -4000 &&
           c->temp_clear < c->temp_high && c->humidity_high > 0 && c->humidity_high <= 10000 &&
           c->humidity_clear < c->humidity_high && c->sensor_address >= 1 &&
           c->sensor_address <= 247 && c->simulated <= 1;
}

/* 报警采用持续超限延时与独立解除阈值，抑制临界值抖动；时间差用无符号减法处理计数回绕。 */
static void threshold(CabinetAlarm *a, uint32_t bit, int32_t value, int32_t high, int32_t clear,
                      uint32_t now, uint32_t *since, uint8_t *pending)
{
    if (a->active & bit)
    {
        if (value <= clear)
            a->active &= ~bit;
        *pending = 0;
    }
    else if (value >= high)
    {
        if (!*pending)
        {
            *pending = 1;
            *since = now;
        }
        if ((uint32_t)(now - *since) >= 5000U)
        {
            a->active |= bit;
            *pending = 0;
        }
    }
    else
        *pending = 0;
}

void cabinet_alarm_update(CabinetAlarm *a, const CabinetConfig *c, const CabinetSample *s,
                          int ipc_online, uint32_t now)
{
    if (!ipc_online)
        a->active |= CAB_ALARM_IPC;
    else
        a->active &= ~CAB_ALARM_IPC;
    if (ipc_online)
    {
        if (!s->online && s->failures >= 3)
            a->active |= CAB_ALARM_SENSOR;
        else
            a->active &= ~CAB_ALARM_SENSOR;
        if (s->water)
            a->active |= CAB_ALARM_WATER;
        else
            a->active &= ~CAB_ALARM_WATER;
    }
    if (ipc_online && s->online && !s->failures && s->simulated == c->simulated &&
        s->uptime_ms - s->last_good_ms <= 2000U)
    {
        threshold(a, CAB_ALARM_TEMP, s->temperature, c->temp_high, c->temp_clear, now,
                  &a->temp_since, &a->temp_pending);
        threshold(a, CAB_ALARM_HUMIDITY, s->humidity, c->humidity_high, c->humidity_clear, now,
                  &a->humidity_since, &a->humidity_pending);
    }
    else
    {
        a->temp_pending = 0;
        a->humidity_pending = 0;
    }
    /* A cleared event loses its acknowledgement; its next occurrence rings again. */
    a->muted &= a->active;
}

void cabinet_alarm_mute(CabinetAlarm *a)
{
    a->muted = a->active;
}

int cabinet_alarm_audible(const CabinetAlarm *a)
{
    return (a->active & ~a->muted) != 0;
}

int cabinet_package_valid(const CabinetPackage *p)
{
    return sizeof(*p) == 64U && p->magic == CABINET_PACKAGE_MAGIC &&
           p->format == CABINET_PACKAGE_FORMAT && p->board == CABINET_BOARD_ID &&
           p->cm7_address == CABINET_CM7_BASE && p->cm4_address == CABINET_CM4_BASE &&
           p->cm7_size >= CABINET_INFO_OFFSET + sizeof(CabinetImageInfo) &&
           p->cm7_size <= CABINET_APP_CAPACITY &&
           p->cm4_size >= CABINET_INFO_OFFSET + sizeof(CabinetImageInfo) &&
           p->cm4_size <= CABINET_APP_CAPACITY && p->payload_size == p->cm7_size + p->cm4_size &&
           p->flags == 0 && p->reserved[0] == 0 && p->reserved[1] == 0 && p->reserved[2] == 0 &&
           p->header_crc == cabinet_crc32(p, 60);
}

int cabinet_vector_valid(uint32_t core, uint32_t address, uint32_t size, uint32_t stack,
                         uint32_t reset)
{
    int ram;
    if (core == 7)
        ram = stack > 0x24000000UL && stack <= 0x24080000UL;
    else if (core == 4)
        ram = stack > 0x30000000UL && stack <= 0x30040000UL;
    else
        return 0;
    return ram && !(stack & 7U) && (reset & 1U) && (reset & ~1UL) >= address &&
           (reset & ~1UL) < address + size;
}
