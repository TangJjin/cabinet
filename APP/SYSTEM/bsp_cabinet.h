/*
 * 文件用途：接口与类型声明：CM7 业务中心：维护 model，运行控制、存储、监督任务，并提供界面使用的业务接口。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef BSP_CABINET_H
#define BSP_CABINET_H
#include "cabinet.h"

enum
{
    CAB_HEART_CONTROL,
    CAB_HEART_UI,
    CAB_HEART_STORAGE,
    CAB_HEART_COUNT
};

enum
{
    CAB_CMD_MUTE = 1,
    CAB_CMD_RING = 2,
    CAB_CMD_UPDATE = 3,
    CAB_CMD_CLOCK = 4
};

typedef struct
{
    CabinetControl control; //设置参数和模拟控制，发送给 CM4
    CabinetSample sample;   //CM4 上报的温湿度、水浸、采集状态
    CabinetAlarm alarm;     //当前报警和消音状态
    uint32_t uptime_ms, lost_logs, saved_revision;
    uint8_t ipc_online, valid, storage_ok;
    uint8_t nor_ok, update_busy, boot_state, boot_result;
    int update_result;
    char clock[24];
    char events[10][112];
} CabinetView;

void bsp_cabinet_init(void);
void bsp_cabinet_start(void);
void bsp_cabinet_snapshot(CabinetView *view);
int bsp_cabinet_config(const CabinetConfig *config);
int bsp_cabinet_simulation(int16_t temperature, uint16_t humidity, int water, int offline);
int bsp_cabinet_command(unsigned command, const char *text);
void bsp_cabinet_heartbeat(unsigned task);
#endif
