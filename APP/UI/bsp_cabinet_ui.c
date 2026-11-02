/*
 * 文件用途：当前机柜 LVGL 界面：创建页面、处理按钮和输入，读取业务快照并刷新显示。
 * 编译归属：CM7 应用。
 */
/* Handwritten LVGL 9 UI. Only the GUI task calls LVGL after initialization. */
#include "bsp_cabinet_ui.h"
#include "bsp_cabinet.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef CABINET_UI_HOST
#include "FreeRTOS.h"
#include "task.h"
#endif
LV_FONT_DECLARE(cabinet_font_24);
static lv_obj_t *pages[5], *nav[5], *clock_label, *mode_label, *values[4], *status_label,
    *alarm_label;
static lv_obj_t *history_label, *config_fields[5], *sim_switch, *settings_result, *keyboard,
    *rtc_field;
static lv_obj_t *charts[2], *upgrade_status, *dialog;
static lv_chart_series_t *series[2];
static CabinetView view;
static unsigned page;
static uint32_t chart_tick;
static const char *const number_keys[] = {"1",
                                          "2",
                                          "3",
                                          "4",
                                          "5",
                                          "6",
                                          "7",
                                          "8",
                                          "9",
                                          "0",
                                          "\n",
                                          "-",
                                          ".",
                                          ":",
                                          " ",
                                          LV_SYMBOL_LEFT,
                                          LV_SYMBOL_RIGHT,
                                          LV_SYMBOL_BACKSPACE,
                                          LV_SYMBOL_OK,
                                          LV_SYMBOL_CLOSE,
                                          ""};
static const lv_buttonmatrix_ctrl_t number_controls[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                                         1, 1, 1, 2, 1, 1, 2, 2, 2};
static const uint32_t bg = 0x0C1423, panel = 0x172339, accent = 0x4BD4AD;

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int width)
{
    lv_obj_t *o = lv_label_create(parent);
    lv_label_set_text(o, text);
    lv_obj_set_pos(o, x, y);
    if (width > 0)
        lv_obj_set_width(o, width);
    lv_obj_set_style_text_color(o, lv_color_hex(0xEAF1FA), 0);
    return o;
}

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(panel), 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 18, 0);
    lv_obj_set_style_radius(o, 14, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *button(lv_obj_t *parent, const char *text, int x, int y, int w, lv_event_cb_t cb,
                        void *data)
{
    lv_obj_t *o = lv_button_create(parent), *l;
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, 54);
    lv_obj_set_style_bg_color(o, lv_color_hex(0x2B425E), 0);
    lv_obj_set_style_shadow_width(o, 0, 0);
    l = label(o, text, 0, 0, 0);
    lv_obj_center(l);
    if (cb)
        lv_obj_add_event_cb(o, cb, LV_EVENT_CLICKED, data);
    return o;
}

static void navigate(lv_event_t *e)
{
    unsigned i;
    page = (unsigned)(uintptr_t)lv_event_get_user_data(e);
    for (i = 0; i < 5; ++i)
    {
        if (i == page)
            lv_obj_remove_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(nav[i], lv_color_hex(i == page ? 0x23745F : 0x172339), 0);
    }
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(pages[3], 686);
    lv_obj_scroll_to_y(pages[3], 0, LV_ANIM_OFF);
    lv_obj_remove_flag(pages[3], LV_OBJ_FLAG_SCROLLABLE);
}

/* 按钮只调用公开业务命令接口；Event 的创建和入队由 bsp_cabinet_command 完成。 */
static void command(lv_event_t *e)
{
    (void)bsp_cabinet_command((unsigned)(uintptr_t)lv_event_get_user_data(e), 0);
}

static void simulate(lv_event_t *e)
{
    unsigned preset = (unsigned)(uintptr_t)lv_event_get_user_data(e);
    (void)bsp_cabinet_simulation(preset == 1 ? 3800 : 2500, preset == 1 ? 8500 : 5000, preset == 2,
                                 preset == 3);
}

static void input_focus(lv_event_t *e)
{
    lv_keyboard_set_textarea(keyboard, lv_event_get_target(e));
    lv_obj_set_height(pages[3], 380);
    lv_obj_add_flag(pages[3], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_scroll_to_view(lv_event_get_target(e), LV_ANIM_OFF);
    lv_obj_remove_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(keyboard);
}

static void keyboard_done(lv_event_t *e)
{
    (void)e;
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(pages[3], 686);
    lv_obj_scroll_to_y(pages[3], 0, LV_ANIM_OFF);
    lv_obj_remove_flag(pages[3], LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *field(lv_obj_t *parent, const char *title, int x, int y, int w)
{
    lv_obj_t *o;
    label(parent, title, x, y, w);
    o = lv_textarea_create(parent);
    lv_obj_set_pos(o, x, y + 35);
    lv_obj_set_size(o, w, 52);
    lv_textarea_set_one_line(o, true);
    lv_textarea_set_max_length(o, 23);
    lv_textarea_set_accepted_chars(o, "0123456789-.: ");
    lv_obj_set_style_bg_color(o, lv_color_hex(0x24374F), 0);
    lv_obj_set_style_text_color(o, lv_color_hex(0xFFFFFF), 0);
    lv_obj_add_event_cb(o, input_focus, LV_EVENT_FOCUSED, 0);
    lv_obj_add_event_cb(o, input_focus, LV_EVENT_CLICKED, 0);
    return o;
}

static void fixed(char *out, size_t n, int v)
{
    snprintf(out, n, "%s%d.%02d", v < 0 ? "-" : "", abs(v) / 100, abs(v) % 100);
}

static int parse_fixed(const char *text, int *value)
{
    const char *p = text;
    int sign = 1, whole = 0, fraction = 0, digits = 0;
    if (*p == '-')
    {
        sign = -1;
        ++p;
    }
    if (*p < '0' || *p > '9')
        return -1;
    while (*p >= '0' && *p <= '9')
    {
        whole = whole * 10 + *p++ - '0';
        if (whole > 10000)
            return -1;
    }
    if (*p == '.')
    {
        ++p;
        while (*p >= '0' && *p <= '9')
        {
            if (++digits > 2)
                return -1;
            fraction = fraction * 10 + *p++ - '0';
        }
    }
    if (*p)
        return -1;
    if (digits == 1)
        fraction *= 10;
    *value = sign * (whole * 100 + fraction);
    return 0;
}

/* 从输入框解析、校验配置后交给业务层；界面不直接操作 TF 或 QSPI。 */
static void save_settings(lv_event_t *e)
{
    CabinetConfig c;
    int v[4];
    unsigned i;
    char *end;
    long address;
    (void)e;
    bsp_cabinet_snapshot(&view);
    c = view.control.config;
    for (i = 0; i < 4; ++i)
        if (parse_fixed(lv_textarea_get_text(config_fields[i]), &v[i]))
            goto invalid;
    address = strtol(lv_textarea_get_text(config_fields[4]), &end, 10);
    if (*end || address < 1 || address > 247 || v[0] < -3900 || v[0] > 8000 || v[1] < -4000 ||
        v[1] > 8000 || v[2] < 1 || v[2] > 10000 || v[3] < 0 || v[3] > 10000)
        goto invalid;
    c.temp_high = (int16_t)v[0];
    c.temp_clear = (int16_t)v[1];
    c.humidity_high = (uint16_t)v[2];
    c.humidity_clear = (uint16_t)v[3];
    c.sensor_address = (uint16_t)address;
    c.simulated = lv_obj_has_state(sim_switch, LV_STATE_CHECKED) ? 1 : 0;
    if (bsp_cabinet_config(&c))
        goto invalid;
    lv_label_set_text(settings_result, "设置已生效，等待保存到板载 Flash");
    keyboard_done(0);
    return;
invalid:
    lv_label_set_text(settings_result, "参数无效：解除阈值必须小于报警阈值");
    keyboard_done(0);
}

static void set_clock(lv_event_t *e)
{
    (void)e;
    lv_label_set_text(settings_result,
                      bsp_cabinet_command(CAB_CMD_CLOCK, lv_textarea_get_text(rtc_field))
                          ? "时间格式错误"
                          : "时间设置已提交，请查看顶部时钟");
    keyboard_done(0);
}

static void dismiss(lv_event_t *e)
{
    (void)e;
    if (dialog)
    {
        lv_obj_delete(dialog);
        dialog = 0;
    }
}

/* 本地确认升级后投递命令，耗时的读卡和写 Flash 由存储任务执行。 */
static void accept_update(lv_event_t *e)
{
    (void)e;
    dismiss(0);
    if (bsp_cabinet_command(CAB_CMD_UPDATE, 0))
        lv_label_set_text(upgrade_status, "无法升级：请检查 TF 卡和板载 Flash");
}

static void ask_update(lv_event_t *e)
{
    lv_obj_t *card;
    (void)e;
    if (dialog)
        return;
    dialog = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(dialog);
    lv_obj_set_size(dialog, 1280, 800);
    lv_obj_remove_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(dialog, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(dialog, LV_OPA_50, 0);
    card = box(dialog, 300, 230, 680, 310);
    label(card, "确认安装 TF 卡中的双核固件？", 18, 12, 610);
    label(card, "校验成功后设备将重启。\n请保持供电，等待升级完成。", 18, 70, 610);
    button(card, "取消", 80, 200, 180, dismiss, 0);
    button(card, "确认升级", 350, 200, 180, accept_update, 0);
}

static void load_fields(void)
{
    unsigned i;
    int v[4];
    char text[32];
    bsp_cabinet_snapshot(&view);
    v[0] = view.control.config.temp_high;
    v[1] = view.control.config.temp_clear;
    v[2] = view.control.config.humidity_high;
    v[3] = view.control.config.humidity_clear;
    for (i = 0; i < 4; ++i)
    {
        fixed(text, sizeof(text), v[i]);
        lv_textarea_set_text(config_fields[i], text);
    }
    snprintf(text, sizeof(text), "%u", view.control.config.sensor_address);
    lv_textarea_set_text(config_fields[4], text);
    if (view.control.config.simulated)
        lv_obj_add_state(sim_switch, LV_STATE_CHECKED);
}

/* 创建五个页面及事件回调；在调度器启动前完成，运行期由唯一 UI 任务访问 LVGL。 */
void bsp_ui_init(void)
{
    static const char *names[] = {"运行总览", "实时趋势", "告警记录", "系统设置", "离线升级"};
    lv_obj_t *root = lv_screen_active(), *o;
    unsigned i;
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root, lv_color_hex(bg), 0);
    lv_obj_set_style_text_font(root, &cabinet_font_24, 0);
    lv_obj_set_style_text_font(lv_layer_top(), &cabinet_font_24, 0);
    label(root, "机柜环境监护", 24, 24, 270);
    clock_label = label(root, "UNSET", 810, 25, 445);
    mode_label = label(root, "", 300, 25, 500);
    for (i = 0; i < 5; ++i)
    {
        nav[i] = button(root, names[i], 18, 110 + (int)i * 88, 184, navigate, (void *)(uintptr_t)i);
        pages[i] = box(root, 220, 94, 1040, 686);
        lv_obj_set_style_pad_all(pages[i], 0, 0);
        lv_obj_set_style_bg_opa(pages[i], LV_OPA_TRANSP, 0);
        if (i)
            lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_bg_color(nav[0], lv_color_hex(0x23745F), 0);
    label(root, "STM32H747\nCM7 + CM4\nOpenAMP", 28, 610, 180);
    {
        static const char *titles[] = {"温度 / C", "湿度 / %RH", "水浸状态", "当前告警"};
        for (i = 0; i < 4; ++i)
        {
            o = box(pages[0], (int)(i % 2) * 520, (int)(i / 2) * 160, 500, 140);
            label(o, titles[i], 0, 0, 450);
            values[i] = label(o, "--", 0, 52, 450);
            if (i < 2)
                lv_obj_set_style_text_font(values[i], &lv_font_montserrat_40, 0);
            lv_obj_set_style_text_color(values[i], lv_color_hex(accent), 0);
        }
    }
    status_label = label(pages[0], "等待双核通信", 12, 337, 1020);
    button(pages[0], "消音", 12, 452, 180, command, (void *)CAB_CMD_MUTE);
    button(pages[0], "重新响铃", 210, 452, 180, command, (void *)CAB_CMD_RING);
    label(pages[0], "模拟场景（仅在模拟模式生效）", 12, 534, 1000);
    button(pages[0], "恢复正常", 12, 581, 220, simulate, (void *)0);
    button(pages[0], "温湿度超限", 256, 581, 220, simulate, (void *)1);
    button(pages[0], "水浸触发", 500, 581, 220, simulate, (void *)2);
    button(pages[0], "传感器离线", 744, 581, 260, simulate, (void *)3);
    label(pages[1], "最近 120 秒 · 每秒一个点 · 无效数据留空", 10, 0, 1000);
    for (i = 0; i < 2; ++i)
    {
        label(pages[1], i ? "湿度 0~100 %RH" : "温度 -40~80 C", 10, 47 + (int)i * 306, 900);
        charts[i] = lv_chart_create(pages[1]);
        lv_obj_set_pos(charts[i], 12, 94 + (int)i * 306);
        lv_obj_set_size(charts[i], 996, 230);
        lv_obj_set_style_bg_color(charts[i], lv_color_hex(panel), 0);
        lv_obj_set_style_border_width(charts[i], 0, 0);
        lv_chart_set_type(charts[i], LV_CHART_TYPE_LINE);
        lv_chart_set_point_count(charts[i], 120);
        lv_chart_set_range(charts[i], LV_CHART_AXIS_PRIMARY_Y, i ? 0 : -4000, i ? 10000 : 8000);
        series[i] = lv_chart_add_series(charts[i], lv_color_hex(i ? 0x64B5F6 : accent),
                                        LV_CHART_AXIS_PRIMARY_Y);
        lv_chart_set_all_value(charts[i], series[i], LV_CHART_POINT_NONE);
        lv_obj_set_style_size(charts[i], 0, 0, LV_PART_INDICATOR);
    }
    alarm_label = label(pages[2], "", 12, 8, 1000);
    button(pages[2], "消音", 12, 142, 190, command, (void *)CAB_CMD_MUTE);
    button(pages[2], "重新响铃", 220, 142, 210, command, (void *)CAB_CMD_RING);
    label(pages[2], "最近事件 / TF 卡保存完整 CSV 日志", 12, 218, 1000);
    history_label = label(pages[2], "", 12, 266, 1000);
    lv_obj_set_height(history_label, 394);
    lv_obj_set_style_text_font(history_label, &lv_font_montserrat_18, 0);
    config_fields[0] = field(pages[3], "温度报警 / C", 12, 0, 290);
    config_fields[1] = field(pages[3], "温度解除 / C", 342, 0, 290);
    config_fields[2] = field(pages[3], "湿度报警 / %RH", 12, 116, 290);
    config_fields[3] = field(pages[3], "湿度解除 / %RH", 342, 116, 290);
    config_fields[4] = field(pages[3], "传感器地址 1~247", 672, 0, 330);
    label(pages[3], "模拟模式", 672, 123, 210);
    sim_switch = lv_switch_create(pages[3]);
    lv_obj_set_pos(sim_switch, 862, 118);
    lv_obj_set_size(sim_switch, 112, 54);
    label(pages[3], "超限持续 5 秒报警；降低至解除值恢复", 12, 237, 1000);
    button(pages[3], "应用并保存", 12, 289, 260, save_settings, 0);
    settings_result = label(pages[3], "", 12, 362, 1000);
    rtc_field = field(pages[3], "本地时间 YYYY-MM-DD HH:MM:SS", 12, 418, 632);
    lv_textarea_set_text(rtc_field, "2026-10-09 12:00:00");
    button(pages[3], "设置时间", 686, 453, 280, set_clock, 0);
    label(pages[4], "TF 卡离线升级", 12, 12, 1000);
    label(
        pages[4],
        "1. 使用打包工具生成配套 CM7 / CM4 升级包。\n\n2. 将 UPDATE.BIN 复制到 FAT32 卡的根目录。\n\n3. 插入 TF 卡，确认后开始校验和安装。\n\n新固件连续健康运行后自动确认；\n连续 3 次启动失败将恢复上一套固件。",
        12, 75, 1000);
    upgrade_status = label(pages[4], "", 12, 445, 1000);
    button(pages[4], "检查并安装 UPDATE.BIN", 12, 570, 600, ask_update, 0);
    keyboard = lv_keyboard_create(lv_layer_top());
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_NUMBER, number_keys, number_controls);
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_NUMBER);
    /* Keyboard defaults to bottom-center alignment; use screen coordinates here. */
    lv_obj_set_align(keyboard, LV_ALIGN_TOP_LEFT);
    lv_obj_set_size(keyboard, 1040, 310);
    lv_obj_set_pos(keyboard, 220, 490);
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(panel), 0);
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x24374F), LV_PART_ITEMS);
    lv_obj_set_style_text_color(keyboard, lv_color_hex(0xEAF1FA), LV_PART_ITEMS);
    lv_obj_set_style_text_font(keyboard, &cabinet_font_24, LV_PART_ITEMS);
    lv_obj_add_event_cb(keyboard, keyboard_done, LV_EVENT_READY, 0);
    lv_obj_add_event_cb(keyboard, keyboard_done, LV_EVENT_CANCEL, 0);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    load_fields();
    bsp_ui_refresh();
}

/* 先复制 model 快照，再更新控件；数据无效时显示 --，保留历史值不代表当前测量有效。 */
void bsp_ui_refresh(void)
{
    char text[1400], a[24], b[24];
    unsigned i;
    uint32_t mask;
    static const char *boot_states[] = {"稳定运行", "等待安装", "正在安装", "新固件试运行",
                                        "恢复旧版本"};
    static const char *boot_results[] = {"出厂固件", "新固件已确认", "已恢复旧版本",
                                         "升级包被拒绝"};
    const char *update_text;
    bsp_cabinet_snapshot(&view);
    mask = view.alarm.active;
    lv_label_set_text(clock_label, view.clock);
    lv_label_set_text(mode_label, view.control.config.simulated ? "模拟模式 · 不代表现场数据"
                                                                : "现场采集 · RS485 Modbus");
    if (view.valid)
    {
        fixed(a, sizeof(a), view.sample.temperature);
        fixed(b, sizeof(b), view.sample.humidity);
    }
    else
    {
        strcpy(a, "--");
        strcpy(b, "--");
    }
    lv_label_set_text(values[0], a);
    lv_label_set_text(values[1], b);
    lv_label_set_text(values[2], !view.ipc_online    ? "状态未知"
                                 : view.sample.water ? "检测到水浸"
                                                     : "正常");
    lv_label_set_text(values[3], mask ? "存在报警或故障" : "运行正常");
    lv_obj_set_style_text_color(values[3], lv_color_hex(mask ? 0xFFB36B : accent), 0);
    snprintf(
        text, sizeof(text),
        "双核通信：%s    传感器：%s    数据：%s\nTF 卡：%s    未保存日志：%lu\n参数保存：%s    消音状态：%s",
        view.ipc_online ? "正常" : "中断", view.sample.online ? "在线" : "离线",
        view.valid ? "有效" : "无效或等待更新", view.storage_ok ? "就绪" : "未就绪",
        (unsigned long)view.lost_logs,
        view.saved_revision == view.control.config.revision ? "已保存" : "等待保存",
        view.alarm.muted ? "已确认当前告警" : "未消音");
    lv_label_set_text(status_label, text);
    snprintf(text, sizeof(text), "当前：%s%s%s%s%s%s\n新告警及恢复后再次触发会重新响铃。",
             mask ? "" : "无告警", mask & CAB_ALARM_TEMP ? "温度超限  " : "",
             mask & CAB_ALARM_HUMIDITY ? "湿度超限  " : "", mask & CAB_ALARM_WATER ? "水浸  " : "",
             mask & CAB_ALARM_SENSOR ? "传感器离线  " : "",
             mask & CAB_ALARM_IPC ? "双核通信中断" : "");
    lv_label_set_text(alarm_label, text);
    text[0] = 0;
    for (i = 0; i < 10; ++i)
    {
        strncat(text, view.events[i], sizeof(text) - strlen(text) - 1);
    }
    lv_label_set_text(history_label, text);
    if (view.uptime_ms - chart_tick >= 1000)
    {
        chart_tick = view.uptime_ms;
        lv_chart_set_next_value(charts[0], series[0],
                                view.valid ? view.sample.temperature : LV_CHART_POINT_NONE);
        lv_chart_set_next_value(charts[1], series[1],
                                view.valid ? view.sample.humidity : LV_CHART_POINT_NONE);
    }
    update_text = view.update_busy           ? "正在校验升级包"
                  : view.update_result == -3 ? "未找到 UPDATE.BIN"
                  : view.update_result == -2 ? "当前固件尚未确认，请稍后再试"
                  : view.update_result < 0   ? "升级包校验失败，请检查文件"
                  : view.update_result == 1  ? "升级准备完成，即将重启"
                                             : "等待操作";
    snprintf(text, sizeof(text), "版本 %u.%u.%u    %s    %s\n%s    TF 卡：%s",
             (unsigned)(CABINET_FW_VERSION >> 16), (unsigned)(CABINET_FW_VERSION >> 8) & 255,
             (unsigned)CABINET_FW_VERSION & 255,
             boot_states[view.boot_state <= 4 ? view.boot_state : 0],
             boot_results[view.boot_result <= 3 ? view.boot_result : 0], update_text,
             view.storage_ok ? "就绪" : "未就绪");
    lv_label_set_text(upgrade_status, text);
}
#ifndef CABINET_UI_HOST
/* UI 任务每约 5 ms 调用 LVGL 处理器，文字约 250 ms 刷新；本工程采用单任务管理 LVGL。 */
void bsp_ui_task(void *argument)
{
    uint32_t last = 0;
    (void)argument;
    for (;;)
    {
        uint32_t now = lv_tick_get();
        if (now - last >= 250)
        {
            last = now;
            bsp_ui_refresh();
        }
        lv_timer_handler();
        bsp_cabinet_heartbeat(CAB_HEART_UI);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
#endif
