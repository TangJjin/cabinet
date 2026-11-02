/*
 * 文件用途：触摸屏 BSP：初始化触摸控制器、读取触点并转换坐标，供 LVGL 输入适配使用。
 * 编译归属：CM7 应用。
 */
#include "bsp_touch.h"

#define GT9XXX_PID_REG 0x8140U
#define GT9XXX_CTRL_REG 0x8040U
#define GT9XXX_GSTID_REG 0x814EU
#define GT9XXX_TP1_REG 0x8150U

#define FT5206_MODE_REG 0x00U
#define FT5206_STATUS_REG 0x02U
#define FT5206_VERSION_REG 0xA1U
#define FT5206_TP1_REG 0x03U

static uint8_t touch_controller = TOUCH_CONTROLLER_NONE;

/**
 * @brief  复位触摸控制器
 * @retval 无
 */
static void Touch_Reset(void)
{
    HAL_GPIO_WritePin(CTP_RST_GPIO_Port, CTP_RST_Pin, GPIO_PIN_RESET);

    HAL_Delay(20U);

    HAL_GPIO_WritePin(CTP_RST_GPIO_Port, CTP_RST_Pin, GPIO_PIN_SET);

    HAL_Delay(100U);
}

/**
 * @brief  尝试初始化 GT9xxx
 * @retval HAL_OK：初始化成功
 *         HAL_ERROR：初始化失败
 */
static HAL_StatusTypeDef Touch_Init_GT9XXX(void)
{
    uint8_t id[4];
    uint8_t control_value;

    if (HAL_I2C_IsDeviceReady(&hi2c2, TOUCH_GT9XXX_ADDRESS, 3U, 100U) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (Touch_I2C_Read16(TOUCH_GT9XXX_ADDRESS, GT9XXX_PID_REG, id, 4U) != HAL_OK)
    {
        return HAL_ERROR;
    }

    id[3] = '\0';

    printf("GT9XXX ID: %s\r\n", id);

    if ((id[0] == 0xFFU) || (id[1] == 0xFFU) || (id[2] == 0xFFU))
    {
        return HAL_ERROR;
    }

    /* 清除 GT9xxx 的触摸状态。 */
    control_value = 0x02U;
    Touch_I2C_Write16(TOUCH_GT9XXX_ADDRESS, GT9XXX_CTRL_REG, &control_value, 1U);

    HAL_Delay(10U);

    control_value = 0x00U;
    Touch_I2C_Write16(TOUCH_GT9XXX_ADDRESS, GT9XXX_CTRL_REG, &control_value, 1U);

    return HAL_OK;
}

/**
 * @brief  尝试初始化 FT5206
 * @retval HAL_OK：初始化成功
 *         HAL_ERROR：初始化失败
 */
static HAL_StatusTypeDef Touch_Init_FT5206(void)
{
    uint8_t version[2];
    uint8_t value;

    if (HAL_I2C_IsDeviceReady(&hi2c2, TOUCH_FT5206_ADDRESS, 3U, 100U) != HAL_OK)
    {
        return HAL_ERROR;
    }

    value = 0x00U;

    Touch_I2C_Write8(TOUCH_FT5206_ADDRESS, FT5206_MODE_REG, &value, 1U);

    if (Touch_I2C_Read8(TOUCH_FT5206_ADDRESS, FT5206_VERSION_REG, version, 2U) != HAL_OK)
    {
        return HAL_ERROR;
    }

    printf("FT5206 version: %02X%02X\r\n", version[0], version[1]);

    return HAL_OK;
}

/**
 * @brief  初始化触摸屏
 * @retval HAL_OK：初始化成功
 *         HAL_ERROR：初始化失败
 */
HAL_StatusTypeDef Touch_Init(void)
{
    touch_controller = TOUCH_CONTROLLER_NONE;

    Touch_Reset();

    if (Touch_Init_GT9XXX() == HAL_OK)
    {
        touch_controller = TOUCH_CONTROLLER_GT9XXX;
        printf("Touch controller: GT9XXX\r\n");
        return HAL_OK;
    }

    Touch_Reset();

    if (Touch_Init_FT5206() == HAL_OK)
    {
        touch_controller = TOUCH_CONTROLLER_FT5206;
        printf("Touch controller: FT5206\r\n");
        return HAL_OK;
    }

    printf("Touch controller not found\r\n");
    return HAL_ERROR;
}

/**
 * @brief  读取 GT9xxx 触摸数据
 * @param  touch 触摸数据结构体
 * @retval 1：检测到触摸
 *         0：没有触摸或读取失败
 */
static uint8_t Touch_Read_GT9XXX(Touch_DataTypeDef *touch)
{
    uint8_t coordinate[4];
    uint8_t status;
    uint8_t clear_status = 0U;
    uint8_t points;
    uint16_t raw_x;
    uint16_t raw_y;

    if (Touch_I2C_Read16(TOUCH_GT9XXX_ADDRESS, GT9XXX_GSTID_REG, &status, 1U) != HAL_OK)
    {
        touch->pressed = 0U;
        touch->points = 0U;
        return 0U;
    }

    /* bit7 为数据就绪标志，低 4 位为当前触点数量。 */
    if ((status & 0x80U) == 0U)
    {
        /* 没有新坐标不代表已经抬手，保持上一帧的按压状态。 */
        return touch->pressed;
    }

    points = status & 0x0FU;

    /* 读取完成后必须清除状态，否则 GT9271 不会更新下一帧数据。 */
    Touch_I2C_Write16(TOUCH_GT9XXX_ADDRESS, GT9XXX_GSTID_REG, &clear_status, 1U);

    if (points == 0U)
    {
        touch->pressed = 0U;
        touch->points = 0U;
        return 0U;
    }

    if (Touch_I2C_Read16(TOUCH_GT9XXX_ADDRESS, GT9XXX_TP1_REG, coordinate, sizeof(coordinate)) !=
        HAL_OK)
    {
        touch->pressed = 0U;
        touch->points = 0U;
        return 0U;
    }

    raw_x = (uint16_t)coordinate[0] | ((uint16_t)coordinate[1] << 8U);

    raw_y = (uint16_t)coordinate[2] | ((uint16_t)coordinate[3] << 8U);

    /* GT9271 原始坐标相对屏幕旋转 90 度，且水平轴方向相反。 */
    if (raw_x >= LCD_PHYSICAL_HEIGHT)
    {
        raw_x = LCD_PHYSICAL_HEIGHT - 1U;
    }

    if (raw_y >= LCD_PHYSICAL_WIDTH)
    {
        raw_y = LCD_PHYSICAL_WIDTH - 1U;
    }

    touch->x = (uint16_t)(LCD_PHYSICAL_WIDTH - 1U - raw_y);
    touch->y = raw_x;

    touch->points = points;
    touch->pressed = 1U;

    return 1U;
}

/**
 * @brief 读取 FT5206 触摸数据
 * @param touch 触摸数据结构体
 * @retval 1：检测到触摸
 *         0：没有触摸或读取失败
 */
static uint8_t Touch_Read_FT5206(Touch_DataTypeDef *touch)
{
    uint8_t data[5];
    uint8_t points;

    if (Touch_I2C_Read8(TOUCH_FT5206_ADDRESS, FT5206_STATUS_REG, data, sizeof(data)) != HAL_OK)
    {
        touch->pressed = 0U;
        touch->points = 0U;
        return 0U;
    }

    points = data[0] & 0x0FU;

    if (points == 0U)
    {
        touch->pressed = 0U;
        touch->points = 0U;
        return 0U;
    }

    touch->x = (uint16_t)(((uint16_t)data[1] & 0x0FU) << 8U) | data[2];

    touch->y = (uint16_t)(((uint16_t)data[3] & 0x0FU) << 8U) | data[4];

    if (touch->x >= LCD_PHYSICAL_WIDTH)
    {
        touch->x = LCD_PHYSICAL_WIDTH - 1U;
    }

    if (touch->y >= LCD_PHYSICAL_HEIGHT)
    {
        touch->y = LCD_PHYSICAL_HEIGHT - 1U;
    }

    touch->points = points;
    touch->pressed = 1U;

    return 1U;
}

/**
 * @brief  读取当前触摸状态
 * @param  touch 触摸数据结构体
 * @retval 1：当前有触摸
 *         0：当前没有触摸
 */
uint8_t Touch_Read(Touch_DataTypeDef *touch)
{
    if (touch == NULL)
    {
        return 0U;
    }

    if (touch_controller == TOUCH_CONTROLLER_GT9XXX)
    {
        return Touch_Read_GT9XXX(touch);
    }

    if (touch_controller == TOUCH_CONTROLLER_FT5206)
    {
        return Touch_Read_FT5206(touch);
    }

    touch->pressed = 0U;
    touch->points = 0U;
    return 0U;
}

/**
 * @brief  获取当前触摸芯片类型
 * @retval 触摸芯片类型
 */
uint8_t Touch_GetController(void)
{
    return touch_controller;
}
