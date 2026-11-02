/*
 * 文件用途：触摸 I2C 访问：封装触摸控制器寄存器的总线读写。
 * 编译归属：CM7 应用。
 */
#include "bsp_touch_i2c.h"

/**
 * @brief  使用硬件 I2C 读取 8 位寄存器地址
 * @param  device_address 已左移一位的设备地址
 * @param  register_address 8 位寄存器地址
 * @param  data 接收缓冲区
 * @param  length 读取长度
 * @retval HAL 状态
 */
HAL_StatusTypeDef Touch_I2C_Read8(uint16_t device_address, uint8_t register_address, uint8_t *data,
                                  uint16_t length)
{
    return HAL_I2C_Mem_Read(&hi2c2, device_address, register_address, I2C_MEMADD_SIZE_8BIT, data,
                            length, 100U);
}

/**
 * @brief  使用硬件 I2C 写入 8 位寄存器地址
 * @param  device_address 已左移一位的设备地址
 * @param  register_address 8 位寄存器地址
 * @param  data 发送缓冲区
 * @param  length 写入长度
 * @retval HAL 状态
 */
HAL_StatusTypeDef Touch_I2C_Write8(uint16_t device_address, uint8_t register_address,
                                   const uint8_t *data, uint16_t length)
{
    return HAL_I2C_Mem_Write(&hi2c2, device_address, register_address, I2C_MEMADD_SIZE_8BIT,
                             (uint8_t *)data, length, 100U);
}

/**
 * @brief  使用硬件 I2C 读取 16 位寄存器地址
 * @param  device_address 已左移一位的设备地址
 * @param  register_address 16 位寄存器地址
 * @param  data 接收缓冲区
 * @param  length 读取长度
 * @retval HAL 状态
 */
HAL_StatusTypeDef Touch_I2C_Read16(uint16_t device_address, uint16_t register_address,
                                   uint8_t *data, uint16_t length)
{
    return HAL_I2C_Mem_Read(&hi2c2, device_address, register_address, I2C_MEMADD_SIZE_16BIT, data,
                            length, 100U);
}

/**
 * @brief  使用硬件 I2C 写入 16 位寄存器地址
 * @param  device_address 已左移一位的设备地址
 * @param  register_address 16 位寄存器地址
 * @param  data 发送缓冲区
 * @param  length 写入长度
 * @retval HAL 状态
 */
HAL_StatusTypeDef Touch_I2C_Write16(uint16_t device_address, uint16_t register_address,
                                    const uint8_t *data, uint16_t length)
{
    return HAL_I2C_Mem_Write(&hi2c2, device_address, register_address, I2C_MEMADD_SIZE_16BIT,
                             (uint8_t *)data, length, 100U);
}
