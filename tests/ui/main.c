/* Runs the real LVGL screens in memory, exercises callbacks, saves RGB PPM frames. */
#include "lvgl.h"
#include "bsp_cabinet.h"
#include "bsp_cabinet_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static CabinetView model;
static uint32_t tick;
static uint16_t pixels[1280 * 800], draw[1280 * 60];
static unsigned commands;

static uint32_t get_tick(void)
{
    return tick;
}

void bsp_cabinet_snapshot(CabinetView *v)
{
    *v = model;
    v->uptime_ms = tick;
}

int bsp_cabinet_config(const CabinetConfig *c)
{
    if (!cabinet_config_valid(c))
        return -1;
    model.control.config = *c;
    return 0;
}

int bsp_cabinet_simulation(int16_t t, uint16_t h, int w, int off)
{
    model.sample.temperature = t;
    model.sample.humidity = h;
    model.sample.water = (uint8_t)w;
    model.sample.online = !off;
    return 0;
}

int bsp_cabinet_command(unsigned command, const char *text)
{
    (void)text;
    commands |= 1U << command;
    if (command == CAB_CMD_MUTE)
        cabinet_alarm_mute(&model.alarm);
    if (command == CAB_CMD_RING)
        model.alarm.muted = 0;
    return 0;
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{
    int x, y;
    uint16_t *p = (uint16_t *)data;
    assert(area->x1 >= 0 && area->x2 < 1280 && area->y1 >= 0 && area->y2 < 800);
    for (y = area->y1; y <= area->y2; ++y)
        for (x = area->x1; x <= area->x2; ++x)
            pixels[y * 1280 + x] = *p++;
    lv_display_flush_ready(display);
}

static void advance(unsigned ms)
{
    unsigned i;
    for (i = 0; i < ms; i += 5)
    {
        tick += 5;
        lv_timer_handler();
    }
}

static void screenshot(const char *name)
{
    FILE *f;
    int i;
    char path[100];
    unsigned char rgb[3];
    lv_obj_invalidate(lv_screen_active());
    advance(100);
    snprintf(path, sizeof(path), "%s.ppm", name);
    f = fopen(path, "wb");
    assert(f);
    fprintf(f, "P6\n1280 800\n255\n");
    for (i = 0; i < 1280 * 800; ++i)
    {
        uint16_t p = pixels[i];
        rgb[0] = (uint8_t)((p >> 11) * 255 / 31);
        rgb[1] = (uint8_t)(((p >> 5) & 63) * 255 / 63);
        rgb[2] = (uint8_t)((p & 31) * 255 / 31);
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static lv_obj_t *find_button(lv_obj_t *parent, const char *text)
{
    unsigned i;
    if (lv_obj_check_type(parent, &lv_button_class))
    {
        lv_obj_t *l = lv_obj_get_child(parent, 0);
        if (l && lv_obj_check_type(l, &lv_label_class) && !strcmp(lv_label_get_text(l), text))
            return parent;
    }
    for (i = 0; i < lv_obj_get_child_count(parent); ++i)
    {
        lv_obj_t *found = find_button(lv_obj_get_child(parent, i), text);
        if (found)
            return found;
    }
    return 0;
}

static void click(const char *text)
{
    lv_obj_t *o = find_button(lv_screen_active(), text);
    if (!o)
        o = find_button(lv_layer_top(), text);
    assert(o);
    lv_obj_send_event(o, LV_EVENT_CLICKED, 0);
    advance(100);
}

static lv_obj_t *find_field(lv_obj_t *parent, const char *text)
{
    unsigned i;
    if (lv_obj_check_type(parent, &lv_textarea_class) &&
        !strcmp(lv_textarea_get_text(parent), text))
        return parent;
    for (i = 0; i < lv_obj_get_child_count(parent); ++i)
    {
        lv_obj_t *found = find_field(lv_obj_get_child(parent, i), text);
        if (found)
            return found;
    }
    return 0;
}

static void verify_font(void)
{
    extern const lv_font_t cabinet_font_24;
    static const char *words =
        "机柜环境监护运行总览实时趋势告警记录系统设置离线升级温度湿度水浸正常模拟模式消音重新响铃";
    uint32_t offset = 0;
    const unsigned char *p = (const unsigned char *)words;
    while (p[offset])
    {
        lv_font_glyph_dsc_t dsc;
        uint32_t cp = p[offset++];
        if (cp >= 0xE0)
        {
            cp = (cp & 15) << 12;
            cp |= (p[offset++] & 63) << 6;
            cp |= p[offset++] & 63;
        }
        assert(lv_font_get_glyph_dsc(&cabinet_font_24, &dsc, cp, 0));
    }
}

int main(void)
{
    lv_display_t *display;
    unsigned i;
    cabinet_config_default(&model.control.config);
    model.sample.temperature = 2568;
    model.sample.humidity = 5342;
    model.sample.online = 1;
    model.sample.simulated = 1;
    model.sample.tasks_ok = 3;
    model.ipc_online = model.valid = model.storage_ok = model.nor_ok = 1;
    model.saved_revision = 1;
    strcpy(model.clock, "2026-10-09 15:08:32");
    strcpy(model.events[0], "2026-10-09 15:08:30,12000,BOOT,0,0,2568,5342,0,1,1,0\n");
    lv_init();
    lv_tick_set_cb(get_tick);
    display = lv_display_create(1280, 800);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, flush);
    lv_display_set_buffers(display, draw, 0, sizeof(draw), LV_DISPLAY_RENDER_MODE_PARTIAL);
    bsp_ui_init();
    verify_font();
    screenshot("overview");
    click("温湿度超限");
    assert(model.sample.temperature == 3800);
    model.alarm.active = 3;
    click("消音");
    assert(model.alarm.muted == 3);
    click("重新响铃");
    assert(model.alarm.muted == 0);
    for (i = 0; i < 120; ++i)
    {
        model.sample.temperature = (int16_t)(2300 + i * 8);
        model.sample.humidity = (uint16_t)(4500 + i * 17);
        tick += 1000;
        bsp_ui_refresh();
    }
    click("实时趋势");
    screenshot("trends");
    click("告警记录");
    screenshot("alarms");
    click("系统设置");
    screenshot("settings");
    click("应用并保存");
    assert(model.control.config.temp_high == 3500);
    {
        lv_area_t area;
        lv_obj_t *keys = lv_obj_get_child(lv_layer_top(), 0);
        lv_obj_t *field = find_field(lv_screen_active(), "2026-10-09 12:00:00");
        assert(field);
        lv_obj_send_event(field, LV_EVENT_FOCUSED, 0);
        advance(100);
        lv_obj_get_coords(field, &area);
        assert(area.y1 >= 94 && area.y2 < 490);
        assert(keys && lv_obj_check_type(keys, &lv_keyboard_class) && lv_obj_is_visible(keys));
        lv_obj_get_coords(keys, &area);
        assert(area.x1 == 220 && area.x2 == 1259 && area.y1 == 490 && area.y2 == 799);
        screenshot("keyboard");
        assert(pixels[600 * 1280 + 600] != pixels[600 * 1280 + 215]);
        lv_obj_send_event(keys, LV_EVENT_READY, 0);
        advance(50);
        assert(!lv_obj_is_visible(keys));
        lv_obj_send_event(field, LV_EVENT_CLICKED, 0);
        advance(50);
        assert(lv_obj_is_visible(keys));
        lv_obj_send_event(keys, LV_EVENT_CANCEL, 0);
        advance(50);
        assert(!lv_obj_is_visible(keys));
    }
    click("离线升级");
    screenshot("upgrade");
    click("检查并安装 UPDATE.BIN");
    screenshot("confirmation");
    click("取消");
    assert(!(commands & (1U << CAB_CMD_UPDATE)));
    click("检查并安装 UPDATE.BIN");
    click("确认升级");
    assert(commands & (1U << CAB_CMD_UPDATE));
    puts(
        "PASS: real LVGL pages rendered; Chinese glyphs, navigation, presets, mute/ring, settings, update confirmation");
    lv_deinit();
    return 0;
}
