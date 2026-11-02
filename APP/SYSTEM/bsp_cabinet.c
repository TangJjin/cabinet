/*
 * 文件用途：CM7 业务中心：维护 model，运行控制、存储、监督任务，并提供界面使用的业务接口。
 * 编译归属：CM7 应用。
 */
#include "bsp_cabinet.h"
#include "bsp_board.h"
#include "bsp_ipc.h"
#include "bsp_qspi.h"
#include "bsp_rtc.h"
#include "bsp_sd.h"
#include "bsp_update.h"
#include "bsp_cabinet_ui.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "ff.h"

/* CM7 内部队列事件：kind 可表示日志事件或界面命令；xQueueSend 会复制整份 Event。 */
typedef struct
{
    unsigned kind;
    uint32_t tick, active, cleared;
    CabinetSample sample;
    uint8_t ipc, valid;
    char text[24];
} Event;

enum
{
    EVENT_ALARM = 10,
    EVENT_SAMPLE,
    EVENT_START,
    EVENT_CONFIG,
    EVENT_CONFIRM
};

/* CM7 业务状态。多个本核任务访问时用短临界区取得快照或更新；这个临界区不锁住 CM4。 */
static CabinetView model;
static QueueHandle_t events;
static volatile uint32_t hearts[CAB_HEART_COUNT];
static volatile int confirm_requested;
static uint32_t received_at;
static int received;
static FATFS filesystem;

void bsp_cabinet_heartbeat(unsigned task)
{
    if (task < CAB_HEART_COUNT)
        hearts[task] = HAL_GetTick();
}

void bsp_storage_progress(void)
{
    bsp_cabinet_heartbeat(CAB_HEART_STORAGE);
    /* Before the scheduler starts, initialization is the only activity. */
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
        bsp_watchdog_feed();
}

/* 复制一致的业务快照，界面后续使用自己的副本，避免长时间占用临界区。 */
void bsp_cabinet_snapshot(CabinetView *v)
{
    taskENTER_CRITICAL();
    *v = model;
    v->uptime_ms = HAL_GetTick();
    taskEXIT_CRITICAL();
}

/* 补齐事件发生时的采样快照后非阻塞入队；存储任务稍后消费，队列满则累计丢失日志。 */
static int enqueue(Event *e)
{
    e->tick = HAL_GetTick();
    taskENTER_CRITICAL();
    e->sample = model.sample;
    e->ipc = model.ipc_online;
    e->valid = model.valid;
    taskEXIT_CRITICAL();
    if (xQueueSend(events, e, 0) == pdPASS)
        return 0;
    taskENTER_CRITICAL();
    ++model.lost_logs;
    taskEXIT_CRITICAL();
    return -1;
}

int bsp_cabinet_config(const CabinetConfig *c)
{
    if (!cabinet_config_valid(c))
        return -1;
    taskENTER_CRITICAL();
    {
        uint32_t revision = model.control.config.revision + 1;
        model.control.config = *c;
        model.control.config.revision = revision;
    }
    taskEXIT_CRITICAL();
    return 0;
}

int bsp_cabinet_simulation(int16_t t, uint16_t h, int water, int offline)
{
    if (t < -4000 || t > 8000 || h > 10000 || water < 0 || water > 1 || offline < 0 || offline > 1)
        return -1;
    taskENTER_CRITICAL();
    model.control.sim_temperature = t;
    model.control.sim_humidity = h;
    model.control.sim_water = (uint8_t)water;
    model.control.sim_offline = (uint8_t)offline;
    taskEXIT_CRITICAL();
    return 0;
}

/* 界面调用的命令入口：本函数创建 Event 并入队；调用者不需要访问私有 Event 类型。 */
int bsp_cabinet_command(unsigned command, const char *text)
{
    Event e = {0};
    e.kind = command;
    if (command == CAB_CMD_MUTE || command == CAB_CMD_RING)
    {
        taskENTER_CRITICAL();
        if (command == CAB_CMD_MUTE)
            cabinet_alarm_mute(&model.alarm);
        else
            model.alarm.muted = 0;
        taskEXIT_CRITICAL();
    }
    else if (command == CAB_CMD_UPDATE)
    {
        taskENTER_CRITICAL();
        if (model.update_busy || !model.storage_ok || !model.nor_ok)
        {
            taskEXIT_CRITICAL();
            return -1;
        }
        model.update_busy = 1;
        model.update_result = 0;
        taskEXIT_CRITICAL();
    }
    else if (command == CAB_CMD_CLOCK)
    {
        if (!text || strlen(text) != 19)
            return -1;
        memcpy(e.text, text, 20);
    }
    else
        return -1;
    if (enqueue(&e))
    {
        if (command == CAB_CMD_UPDATE)
            model.update_busy = 0;
        return -1;
    }
    return 0;
}

/* CM7 业务接收：只接受配套版本的 kind=1 采样，用新序号更新时间，供通信超时判断。 */
static void receive(const CabinetMessage *m)
{
    if (m->kind != 1 || m->payload.sample.firmware_version != CABINET_FW_VERSION)
        return;
    taskENTER_CRITICAL();
    if (!received || m->payload.sample.sequence != model.sample.sequence)
    {
        model.sample = m->payload.sample;
        received_at = HAL_GetTick();
        received = 1;
    }
    taskEXIT_CRITICAL();
}

/* 控制任务：接收 CM4 样本、判断报警并驱动输出；周期性发送 kind=2 参数，有事件时投递日志。 */
static void control_task(void *arg)
{
    uint32_t last_send = 0, last_log = 0;
    (void)arg;
    for (;;)
    {
        uint32_t now = HAL_GetTick(), old;
        Event e = {0};
        CabinetMessage m = {0};
        int audible;
        bsp_ipc_poll(); //检查并处理 OpenAMP 消息，收到采样信息后会调用receive()函数更新时间用来判断是否在线
        taskENTER_CRITICAL();
        model.ipc_online =
            (uint8_t)(received && now - received_at < 1000); //判断双核通信是否在线CM7内部计算
        model.valid =
            (uint8_t)(model.ipc_online && model.sample.online &&
                      !model.sample.failures && //确认数据正在更新、采集状态正常以及没有故障
                      model.control.config.simulated ==
                          model.sample.simulated && //设置模式与样本模式一致
                      model.sample.uptime_ms - model.sample.last_good_ms <=
                          2000); //数据没有过期，CM4时间数据传到CM7进行计算
        old = model.alarm.active;
        cabinet_alarm_update(&model.alarm, &model.control.config, &model.sample, model.ipc_online,
                             now);
        e.kind = EVENT_ALARM;
        e.active = model.alarm.active & ~old;
        e.cleared = old & ~model.alarm.active;
        e.sample = model.sample; //分别判断是否有新报警以及是否有报警被清除
        old = model.alarm.active;
        audible = cabinet_alarm_audible(&model.alarm); //
        m.payload.control = model.control;
        taskEXIT_CRITICAL();
        bsp_board_alarm_outputs(old, audible, now); //根据报警位、响铃状态和时间设置灯、蜂鸣器 GPIO
        if (e.active || e.cleared)
            (void)enqueue(&e); //有报警新增或解除，就把事件放进日志队列
        if (now - last_send >= 100)
        { //每 100 ms 向 CM4 发送控制参数
            m.magic = 0x4341424D;
            m.protocol = CABINET_PROTOCOL;
            m.kind = 2;
            m.size = sizeof(m);
            if (!bsp_ipc_send(&m))
                last_send = now;
        }
        if (now - last_log >= 10000)
        {
            last_log = now;
            e.kind = EVENT_SAMPLE;
            (void)enqueue(&e);
        } //每 10 秒投递采样日志
        bsp_cabinet_heartbeat(CAB_HEART_CONTROL);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void append_history(const char *line)
{
    size_t n;
    taskENTER_CRITICAL();
    memmove(model.events[1], model.events[0], 9 * sizeof(model.events[0]));
    strncpy(model.events[0], line, sizeof(model.events[0]) - 2);
    model.events[0][sizeof(model.events[0]) - 2] = 0;
    n = strlen(model.events[0]);
    if (n && model.events[0][n - 1] != '\n')
    {
        model.events[0][n++] = '\n';
        model.events[0][n] = 0;
    }
    taskEXIT_CRITICAL();
}

/* 写入并关闭文件以同步数据；仅由存储任务调用，超过容量阈值时轮换旧日志。 */
static int write_line(const char *path, const char *line)
{
    FIL file;
    UINT count;
    FRESULT result;
    size_t n = strlen(line);
    result = f_open(&file, path, FA_OPEN_APPEND | FA_WRITE);
    if (result != FR_OK)
        return -1;
    /* Rotate at 4 MiB: bounded disk use, retain one previous file per stream. */
    if (f_size(&file) > 4UL * 1024 * 1024)
    {
        const char *old = !strcmp(path, "DATA.CSV") ? "DATA.OLD" : "EVENTS.OLD";
        f_close(&file);
        (void)f_unlink(old);
        if (f_rename(path, old) != FR_OK ||
            f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
            return -1;
    }
    result = f_write(&file, line, (UINT)n, &count);
    if (result != FR_OK || count != n)
    {
        f_close(&file);
        return -1;
    }
    /* f_close performs f_sync; return errors (full card/removal) to the UI. */
    return f_close(&file) == FR_OK ? 0 : -1;
}

static void log_event(const Event *e, int *mounted)
{
    char line[220], clock[24];
    const char *type = "EVENT";
    (void)bsp_rtc_text(clock, sizeof(clock));
    switch (e->kind)
    {
        case EVENT_ALARM:
            type = "ALARM";
            break;
        case EVENT_SAMPLE:
            type = "SAMPLE";
            break;
        case EVENT_START:
            type = "BOOT";
            break;
        case EVENT_CONFIG:
            type = "CONFIG";
            break;
        case EVENT_CONFIRM:
            type = "CONFIRM";
            break;
        case CAB_CMD_MUTE:
            type = "MUTE";
            break;
        case CAB_CMD_RING:
            type = "RING";
            break;
        case CAB_CMD_UPDATE:
            type = "UPDATE";
            break;
        case CAB_CMD_CLOCK:
            type = "CLOCK";
            break;
        default:
            break;
    }
    snprintf(line, sizeof(line), "%s,%lu,%s,%lu,%lu,%d,%u,%u,%u,%u,%u,%u,%u,%lu\r\n", clock,
             (unsigned long)e->tick, type, (unsigned long)e->active, (unsigned long)e->cleared,
             e->sample.temperature, e->sample.humidity, e->sample.water, e->sample.online,
             e->sample.simulated, e->sample.failures, e->ipc, e->valid,
             (unsigned long)CABINET_FW_VERSION);
    if (e->kind != EVENT_SAMPLE)
        append_history(line);
    if (*mounted && write_line(e->kind == EVENT_SAMPLE ? "DATA.CSV" : "EVENTS.CSV", line))
    {
        *mounted = 0;
        bsp_sd_forget();
        f_mount(0, "", 0);
    }
    if (!*mounted)
    {
        taskENTER_CRITICAL();
        ++model.lost_logs;
        taskEXIT_CRITICAL();
    }
}

/* 存储任务独占应用内的 FatFs 和可写 QSPI 操作；保存配置、消费事件、准备升级并确认试运行版本。 */
static void storage_task(void *arg)
{
    uint32_t mount_try = 0, config_try = 0, clock_tick = 0, seen_revision = 0, changed_at = 0;
    int mounted = 0;
    Event e = {0};
    (void)arg;
    e.kind = EVENT_START;
    e.active = RCC->RSR;
    e.cleared = model.boot_result;
    e.tick = HAL_GetTick();
    __HAL_RCC_CLEAR_RESET_FLAGS();
    (void)enqueue(&e);
    for (;;)
    {
        uint32_t now = HAL_GetTick();
        CabinetConfig c;
        bsp_cabinet_heartbeat(CAB_HEART_STORAGE);
        if (!mounted && (!mount_try || now - mount_try >= 5000))
        { //当前没有挂载成功，并且是首次尝试或距离上次尝试已过 5 秒，就尝试挂载
            mount_try = now;
            mounted = (f_mount(&filesystem, "", 1) == FR_OK); //表示使用默认磁盘；1为立即检查并挂载
        }
        taskENTER_CRITICAL();
        model.storage_ok = (uint8_t)mounted;
        c = model.control.config;
        taskEXIT_CRITICAL(); //保存参数配置
        if (c.revision != seen_revision)
        {
            seen_revision = c.revision;
            changed_at = now;
        } //查看修订次数判断是否需要更新
        if (model.nor_ok && c.revision != model.saved_revision && now - changed_at >= 1000 &&
            now - config_try >= 3000)
        { //严格判断是否要保存到flash
            config_try = now;
            if (!bsp_config_save(&c))
            { //保存配置，更新saved_revision
                taskENTER_CRITICAL();
                model.saved_revision = c.revision;
                taskEXIT_CRITICAL();
                memset(&e, 0, sizeof(e));
                e.kind = EVENT_CONFIG;
                e.tick = now;
                log_event(&e, &mounted);
            }
        }
        if (confirm_requested && model.nor_ok)
        {
            confirm_requested = 0;
            if (!cabinet_boot_confirm(&cabinet_boot_ops, CABINET_FW_VERSION))
            { //监督任务检查两核和工作任务持续健康 10 秒后，提出确认请求
                model.boot_state = CAB_BOOT_STABLE;
                model.boot_result = CAB_BOOT_CONFIRMED;
                memset(&e, 0, sizeof(e));
                e.kind = EVENT_CONFIRM;
                e.tick = now;
                log_event(&e, &mounted);
            }
        }
        if (now - clock_tick >= 500)
        { //界面显示时间文本
            char text[24];
            clock_tick = now;
            (void)bsp_rtc_text(text, sizeof(text));
            taskENTER_CRITICAL();
            memcpy(model.clock, text, sizeof(text));
            taskEXIT_CRITICAL();
        }
        if (xQueueReceive(events, &e, pdMS_TO_TICKS(20)) == pdPASS)
        { //从 events 队列取出一份事件，复制到存储任务自己的 e 中
            if (e.kind == CAB_CMD_CLOCK)
            {
                if (bsp_rtc_set_text(e.text))
                    append_history("RTC rejected: use YYYY-MM-DD HH:MM:SS (2000..2099)");
            }
            else if (e.kind == CAB_CMD_UPDATE)
            {
                int result;
                append_history("UPDATE: validating TF package and writing QSPI staging area...");
                result = mounted ? bsp_update_from_card("UPDATE.BIN") : -3;
                taskENTER_CRITICAL();
                model.update_result = result ? result : 1;
                model.update_busy = 0;
                taskEXIT_CRITICAL();
                e.active = (uint32_t)result;
                log_event(&e, &mounted);
                if (!result)
                {
                    vTaskDelay(pdMS_TO_TICKS(100));
                    NVIC_SystemReset();
                }
                append_history(
                    "UPDATE rejected. Check UPDATE.BIN, paired version, CRC and TF card.");
                continue;
            }
            log_event(&e, &mounted);
        }
    }
}

/* 监督任务只检查任务是否持续推进；传感器断线但采集任务能正常处理超时，仍可认为任务健康。 */
static void supervisor_task(void *arg)
{
    uint32_t start = HAL_GetTick(), healthy_since = 0;
    int healthy_pending = 0;
    (void)arg;
    for (;;)
    {
        uint32_t now = HAL_GetTick();
        unsigned i;
        int ok = 1;
        for (i = 0; i < CAB_HEART_COUNT; ++i)
            if (now - hearts[i] > 5000)
                ok = 0; //任意一个任务超过 5 秒没有报告心跳，本轮健康检查失败
        taskENTER_CRITICAL();
        if (!model.ipc_online || model.sample.tasks_ok != 3)
            ok = 0; //CM7 持续收到新的 CM4 消息;CM4 报告它的采集、水浸任务都在推进
        taskEXIT_CRITICAL();
        if (ok)
        {                        //检查通过后才会进行喂狗
            bsp_watchdog_feed(); //喂狗
            /* 连续健康计时被任何失败打断；试运行满 10 秒后，只提出请求，由存储任务落盘确认。 */
            if (!healthy_pending)
            {
                healthy_pending = 1;
                healthy_since = now;
            }
            if (now - healthy_since >= 10000 && model.boot_state == CAB_BOOT_TRIAL)
                confirm_requested = 1;
        }
        else
        {
            healthy_pending = 0; //连续健康统计时间标志
            if (now - start < 5000)
                bsp_watchdog_feed(); //宽限期，系统启动前五秒还是会喂狗
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

/**
  * @brief 初始化机柜状态、存储资源、日志队列和双核通信
 */
void bsp_cabinet_init(void)
{
    CabinetBootRecord record;
    unsigned i;
    memset(&model, 0, sizeof(model));
    cabinet_config_default(&model.control.config);
    model.control.sim_temperature = 2500;
    model.control.sim_humidity = 5000;
    strcpy(model.clock, "UNSET");
    model.nor_ok = (uint8_t)(bsp_qspi_init() == 0);
    if (model.nor_ok)
    {
        if (bsp_config_load(&model.control.config) == 0)
            model.saved_revision = model.control.config.revision;
        if (!cabinet_boot_load(&cabinet_boot_ops, &record))
        {
            model.boot_state = (uint8_t)record.state;
            model.boot_result = (uint8_t)record.result;
        }
    }
    (void)bsp_rtc_init();
    events = xQueueCreate(32, sizeof(Event));
    if (!events)
        bsp_fatal();
    for (i = 0; i < CAB_HEART_COUNT; ++i)
        hearts[i] = HAL_GetTick();
    if (bsp_ipc_init(receive))
        bsp_fatal();
}

/**
  * @brief Freertos任务创建函数
 */
/* xTaskCreate 的栈大小单位为 StackType_t（本平台 4 字节）；运行周期由各任务中的 delay 控制。 */
void bsp_cabinet_start(void)
{
    unsigned i;
    for (i = 0; i < CAB_HEART_COUNT; ++i)
        hearts[i] = HAL_GetTick();
    if (xTaskCreate(control_task, "control", 1024, 0, 3, 0) !=
            pdPASS || //每 20 ms 更新报警、输出和控制消息
        xTaskCreate(bsp_ui_task, "lvgl", 2048, 0, 2, 0) !=
            pdPASS || //每 5 ms 调用 LVGL，文字每 250 ms 更新
        xTaskCreate(storage_task, "storage", 2048, 0, 1, 0) !=
            pdPASS || //写日志、保存参数、处理升级请求
        xTaskCreate(supervisor_task, "watchdog", 384, 0, 4, 0) !=
            pdPASS //每 250 ms 检查任务进度，健康才喂狗
    )
        bsp_fatal();
}
