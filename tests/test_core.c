#include "cabinet.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_alarms(void)
{
    CabinetConfig c;
    CabinetAlarm a = {0};
    CabinetSample s = {0};
    cabinet_config_default(&c);
    assert(cabinet_config_valid(&c));
    c.temp_clear = c.temp_high;
    assert(!cabinet_config_valid(&c));
    cabinet_config_default(&c);
    s.online = 1;
    s.simulated = 1;
    s.temperature = 3500;
    s.humidity = 8000;
    cabinet_alarm_update(&a, &c, &s, 1, 100);
    assert(a.active == 0);
    cabinet_alarm_update(&a, &c, &s, 1, 5099);
    assert(a.active == 0);
    cabinet_alarm_update(&a, &c, &s, 1, 5100);
    assert(a.active == 3 && cabinet_alarm_audible(&a));
    cabinet_alarm_mute(&a);
    assert(!cabinet_alarm_audible(&a));
    cabinet_alarm_update(&a, &c, &s, 1, 600000);
    assert(!cabinet_alarm_audible(&a));
    s.water = 1;
    cabinet_alarm_update(&a, &c, &s, 1, 600001);
    assert(cabinet_alarm_audible(&a));
    cabinet_alarm_mute(&a);
    s.water = 0;
    cabinet_alarm_update(&a, &c, &s, 1, 600002);
    assert(a.muted == 3);
    s.water = 1;
    cabinet_alarm_update(&a, &c, &s, 1, 600003);
    assert(cabinet_alarm_audible(&a));
    s.temperature = 3400;
    s.humidity = 7600;
    cabinet_alarm_update(&a, &c, &s, 1, 600004);
    assert((a.active & 3) == 3);
    s.temperature = 3300;
    s.humidity = 7500;
    cabinet_alarm_update(&a, &c, &s, 1, 600005);
    assert((a.active & 3) == 0);
    /* A communication fault must never clear a previously active physical alarm. */
    cabinet_alarm_update(&a, &c, &s, 0, 600006);
    assert((a.active & (CAB_ALARM_IPC | CAB_ALARM_WATER)) == 20);
    s.water = 0;
    s.online = 0;
    s.failures = 3;
    cabinet_alarm_update(&a, &c, &s, 1, 600007);
    assert(a.active == CAB_ALARM_SENSOR);
    memset(&a, 0, sizeof(a));
    s.online = 1;
    s.failures = 0;
    s.temperature = 3600;
    s.humidity = 7000;
    cabinet_alarm_update(&a, &c, &s, 1, 0xFFFFF000U);
    cabinet_alarm_update(&a, &c, &s, 1, 0x388U);
    assert(a.active == CAB_ALARM_TEMP); /* Tick wraps, 5000 ms. */
    memset(&a, 0, sizeof(a));
    cabinet_alarm_update(&a, &c, &s, 1, 0);
    s.failures = 1;
    cabinet_alarm_update(&a, &c, &s, 1, 5000);
    assert(a.active == 0);
    s.failures = 0;
    cabinet_alarm_update(&a, &c, &s, 1, 5001);
    assert(a.active == 0);
    cabinet_alarm_update(&a, &c, &s, 1, 10001);
    assert(a.active == CAB_ALARM_TEMP);
    s.online = 0;
    s.failures = 3;
    s.temperature = 0;
    cabinet_alarm_update(&a, &c, &s, 1, 10002);
    assert(a.active == (CAB_ALARM_TEMP | CAB_ALARM_SENSOR));
    memset(&a, 0, sizeof(a));
    s.failures = 2;
    cabinet_alarm_update(&a, &c, &s, 1, 10003);
    assert(a.active == 0);
}

static void test_modbus(void)
{
    uint8_t frame[9] = {1, 3, 4, 0xFE, 0x0C, 0x13, 0x88, 0, 0}, req[8];
    uint16_t crc;
    int16_t t;
    uint16_t h;
    assert(cabinet_modbus_crc((const uint8_t *)"123456789", 9) == 0x4B37);
    cabinet_rtu_request(1, req);
    assert(req[6] == 0xC4 && req[7] == 0x0B);
    crc = cabinet_modbus_crc(frame, 7);
    frame[7] = (uint8_t)crc;
    frame[8] = (uint8_t)(crc >> 8);
    assert(!cabinet_rtu_decode(1, frame, 9, &t, &h) && t == -500 && h == 5000);
    assert(cabinet_rtu_decode(2, frame, 9, &t, &h));
    assert(cabinet_rtu_decode(1, frame, 8, &t, &h));
    frame[3] ^= 1;
    assert(cabinet_rtu_decode(1, frame, 9, &t, &h));
}

int main(void)
{
    assert(sizeof(CabinetPackage) == 64 && sizeof(CabinetImageInfo) == 32);
    assert(cabinet_crc32("123456789", 9) == 0xCBF43926);
    test_alarms();
    test_modbus();
    puts("PASS: CRC, alarm hysteresis/mute/retrigger/stale/wrap, RTU request/response validation");
    return 0;
}
