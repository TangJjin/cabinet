/*
 * 文件用途：Modbus 编解码：RTU 请求组包、CRC16 和固定温湿度响应解析。
 * 编译归属：CM7 应用、CM4 应用。
 */
#include "cabinet.h"
#include <string.h>

static uint16_t be16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] << 8 | p[1]);
}

/* Modbus RTU CRC16：初值 0xFFFF，反向多项式 0xA001；报文尾部先放 CRC 低字节。 */
uint16_t cabinet_modbus_crc(const uint8_t *p, size_t n)
{
    uint16_t crc = 0xFFFF;
    while (n--)
    {
        unsigned i;
        crc ^= *p++;
        for (i = 0; i < 8; ++i)
            crc = (uint16_t)((crc >> 1) ^ ((crc & 1) ? 0xA001 : 0));
    }
    return crc;
}

/* 固定传感器请求：功能码 03，从偏移 0 连读两个寄存器；地址/寄存器布局需要匹配实物说明书。 */
size_t cabinet_rtu_request(uint8_t address, uint8_t request[8])
{
    uint16_t crc;
    request[0] = address;
    request[1] = 3;
    request[2] = 0;
    request[3] = 0;
    request[4] = 0;
    request[5] = 2;
    crc = cabinet_modbus_crc(request, 6);
    request[6] = (uint8_t)crc;
    request[7] = (uint8_t)(crc >> 8);
    return 8;
}

/* 本项目固定九字节温湿度响应：校验地址、功能、长度、CRC 和范围后，才输出 0.01 单位数值。 */
int cabinet_rtu_decode(uint8_t address, const uint8_t *f, size_t n, int16_t *temperature,
                       uint16_t *humidity)
{
    uint16_t crc;
    int16_t t;
    uint16_t h;
    if (n != 9 || f[0] != address || f[1] != 3 || f[2] != 4)
        return -1;
    crc = cabinet_modbus_crc(f, 7);
    if (f[7] != (uint8_t)crc || f[8] != (uint8_t)(crc >> 8))
        return -2;
    t = (int16_t)be16(f + 3);
    h = be16(f + 5);
    if (t < -4000 || t > 8000 || h > 10000)
        return -3;
    *temperature = t;
    *humidity = h;
    return 0;
}
