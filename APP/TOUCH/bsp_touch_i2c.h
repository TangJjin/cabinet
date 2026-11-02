/*
 * 文件用途：接口与类型声明：触摸 I2C 访问：封装触摸控制器寄存器的总线读写。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef BSP_TOUCH_I2C_H
#define BSP_TOUCH_I2C_H

#include "bsp_system.h"

HAL_StatusTypeDef Touch_I2C_Read8(uint16_t device_address, uint8_t register_address, uint8_t *data,
                                  uint16_t length);

HAL_StatusTypeDef Touch_I2C_Write8(uint16_t device_address, uint8_t register_address,
                                   const uint8_t *data, uint16_t length);

HAL_StatusTypeDef Touch_I2C_Read16(uint16_t device_address, uint16_t register_address,
                                   uint8_t *data, uint16_t length);

HAL_StatusTypeDef Touch_I2C_Write16(uint16_t device_address, uint16_t register_address,
                                    const uint8_t *data, uint16_t length);

#endif
