/*
 * 文件用途：接口与类型声明：双核消息、配置、采样和固件包格式，以及通用业务接口。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef CABINET_H
#define CABINET_H

#include <stdint.h>
#include <stddef.h>

#define CABINET_BOARD_ID 0x07470101UL
#define CABINET_FW_VERSION 0x00010000UL
#define CABINET_PROTOCOL 1U
#define CABINET_CM7_BASE 0x08020000UL
#define CABINET_CM4_BASE 0x08120000UL
#define CABINET_APP_CAPACITY 0x000E0000UL
#define CABINET_INFO_OFFSET 0x400U
#define CABINET_INFO_MAGIC 0x43414249UL
#define CABINET_PACKAGE_MAGIC 0x43414250UL
#define CABINET_PACKAGE_FORMAT 1U
#define CABINET_GATE_ADDRESS 0x3800FC00UL
#define CABINET_GATE_MAGIC 0x47415445UL
#define CABINET_GATE_HSEM 15U

enum
{
    CAB_ALARM_TEMP = 1U,
    CAB_ALARM_HUMIDITY = 2U,
    CAB_ALARM_WATER = 4U,
    CAB_ALARM_SENSOR = 8U,
    CAB_ALARM_IPC = 16U
};

/* All temperatures/humidity values use 0.01 units. No floating point on the wire. */
/* CabinetConfig：可保存配置，revision 递增用于通知存储任务保存。 */
typedef struct
{
    int16_t temp_high, temp_clear;
    uint16_t humidity_high, humidity_clear;
    uint16_t sensor_address;
    uint16_t simulated;
    uint32_t revision;
} CabinetConfig;

/* CabinetSample：CM4 发布的采样快照，含新序号、采集时间及任务健康状态。 */
typedef struct
{
    uint32_t sequence, uptime_ms, last_good_ms, firmware_version;
    int16_t temperature;
    uint16_t humidity;
    uint8_t water, online, simulated, failures;
    uint32_t tasks_ok;
} CabinetSample;

/* CabinetControl：CM7 发给 CM4 的配置和模拟输入。 */
typedef struct
{
    CabinetConfig config;
    int16_t sim_temperature;
    uint16_t sim_humidity;
    uint8_t sim_water, sim_offline;
    uint16_t reserved;
} CabinetControl;

/* CabinetMessage：双核业务消息；kind=1 使用 sample，kind=2 使用 control。 */
typedef struct
{
    uint32_t magic, protocol, kind, size;

    union
    {
        CabinetSample sample;
        CabinetControl control;
    } payload;
} CabinetMessage;

/* CabinetAlarm：活动/消音位图，以及超限延时计时状态。 */
typedef struct
{
    uint32_t active, muted, temp_since, humidity_since;
    uint8_t temp_pending, humidity_pending;
} CabinetAlarm;

typedef struct
{
    uint32_t magic, board, version, core;
    uint32_t protocol, reserved[3];
} CabinetImageInfo;

/* Exact 64-byte little-endian package header. CRC covers first 60 bytes. */
typedef struct
{
    uint32_t magic, format, board, version;
    uint32_t cm7_address, cm7_size, cm7_crc;
    uint32_t cm4_address, cm4_size, cm4_crc;
    uint32_t payload_size, flags, reserved[3], header_crc;
} CabinetPackage;

typedef struct
{
    volatile uint32_t magic, address, inverse, reserved;
} CabinetGate;

uint32_t cabinet_crc32_update(uint32_t crc, const void *data, size_t size);
uint32_t cabinet_crc32(const void *data, size_t size);
uint16_t cabinet_modbus_crc(const uint8_t *data, size_t size);
void cabinet_config_default(CabinetConfig *config);
int cabinet_config_valid(const CabinetConfig *config);
void cabinet_alarm_update(CabinetAlarm *alarm, const CabinetConfig *config,
                          const CabinetSample *sample, int ipc_online, uint32_t now);
void cabinet_alarm_mute(CabinetAlarm *alarm);
int cabinet_alarm_audible(const CabinetAlarm *alarm);
int cabinet_package_valid(const CabinetPackage *header);
int cabinet_vector_valid(uint32_t core, uint32_t address, uint32_t size, uint32_t stack,
                         uint32_t reset);
size_t cabinet_rtu_request(uint8_t address, uint8_t request[8]);
int cabinet_rtu_decode(uint8_t address, const uint8_t *frame, size_t length, int16_t *temperature,
                       uint16_t *humidity);

#endif
