/*
 * 文件用途：MIPI 屏 BSP：复位、面板命令、背光控制和基础绘图；与 DSI/LTDC 配置配合。
 * 编译归属：CM7 应用。
 */
#include "bsp_mipi_lcd.h"

static void MIPI_LCD_Reset(void);

/**
 * @brief  初始化 ILI9881C10 LCD 面板
 * @note   按初始化命令表配置面板，并执行 Sleep Out 和 Display On。
 * @retval HAL_OK：面板初始化成功
 *         HAL_ERROR：任意 DSI 命令发送失败
 */
HAL_StatusTypeDef BSP_ILI9881C10_Init(void)
{
    /*
		发送 Page 3 配置
		发送 Page 4 配置
		发送 Page 1 配置
		切换回 Page 0
		打开 TE
		发送 Sleep Out：0x11
		等待 120ms
		发送 Display On：0x29
		等待 120ms
	*/
    uint16_t i;
    uint8_t data[3] = {0x98U, 0x81U, 0x00U};

    for (i = 0U; i < ili9881c10_init_cmd_count; i++)
    {
        HAL_StatusTypeDef status =
            MIPI_LCD_SendCommand(ili9881c10_init_cmd_tbl[i].cmd, ili9881c10_init_cmd_tbl[i].data,
                                 ili9881c10_init_cmd_tbl[i].data_bytes);

        if (status != HAL_OK)
        {
            return HAL_ERROR;
        }
    }

    if (MIPI_LCD_SendCommand(0xFFU, data, 3U) != HAL_OK)
    {
        return HAL_ERROR;
    }

    data[0] = 0x00U;

    if (MIPI_LCD_SendCommand(0x35U, data, 1U) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (MIPI_LCD_SendCommand(0x11U, NULL, 0U) != HAL_OK)
    {
        return HAL_ERROR;
    }

    HAL_Delay(120U);

    if (MIPI_LCD_SendCommand(0x29U, NULL, 0U) != HAL_OK)
    {
        return HAL_ERROR;
    }

    HAL_Delay(120U);

    return HAL_OK;
}

/**
 * @brief  初始化 MIPI LCD 硬件
 * @note   依次完成电源控制、复位、DSI 启动、面板初始化和背光开启。
 * @retval HAL_OK：LCD 初始化成功
 *         HAL_ERROR：DSI 或面板初始化失败
 */
HAL_StatusTypeDef MIPI_LCD_Init(void)
{
    MIPI_LCD_SetBacklight(0U);

    HAL_GPIO_WritePin(LCD_PWREN_GPIO_Port, LCD_PWREN_Pin, GPIO_PIN_SET);
    HAL_Delay(100U);

    MIPI_LCD_Reset();

    HAL_Delay(50);
    /* Clear any LTDC startup flags before enabling the DSI video stream. */
    __HAL_LTDC_CLEAR_FLAG(&hltdc, LTDC_FLAG_FU | LTDC_FLAG_TE);
    if (HAL_DSI_Start(&hdsi) != HAL_OK)
    {
        return HAL_ERROR;
    }
    HAL_Delay(200);

    if (BSP_ILI9881C10_Init() != HAL_OK)
    {
        return HAL_ERROR;
    }

    HAL_Delay(50U);
    MIPI_LCD_SetBacklight(1U);

    return HAL_OK;
}

/**
 * @brief  复位 ILI9881C10 LCD 面板
 * @note   按照面板要求输出复位时序，并在复位释放后等待面板稳定。
 * @retval 无
 */
static void MIPI_LCD_Reset(void)
{
    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(20U);

    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(50U);

    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(200U);
}

/**
 * @brief  通过 DSI 向 LCD 发送 DCS 命令
 * @param  command LCD 控制命令字节
 * @param  data    命令参数数据指针，无参数时可传入 NULL
 * @param  length  参数数据长度，单位为字节
 * @retval HAL_OK：命令发送成功
 *         HAL_ERROR：命令发送失败
 */
HAL_StatusTypeDef MIPI_LCD_SendCommand(uint16_t command, const uint8_t *data, uint16_t length)
{
    uint8_t dummy = 0U;
    HAL_StatusTypeDef status;

    if (data == NULL)
    {
        data = &dummy;
    }

    if (length == 0U)
    {
        status = HAL_DSI_ShortWrite(&hdsi, 0U, DSI_DCS_SHORT_PKT_WRITE_P0, command, 0U);
    }
    else if (length == 1U)
    {
        status = HAL_DSI_ShortWrite(&hdsi, 0U, DSI_DCS_SHORT_PKT_WRITE_P1, command, data[0]);
    }
    else
    {
        status = HAL_DSI_LongWrite(&hdsi, 0U, DSI_DCS_LONG_PKT_WRITE, length, command, data);
    }

    return status;
}

/**
 * @brief  使用指定颜色清空 LCD 帧缓冲区
 * @param  color RGB565 格式的填充颜色
 * @note   该函数只写入 SDRAM 帧缓冲区，不直接发送 DSI 命令。
 * @retval 无
 */
void MIPI_LCD_Clear(uint16_t color)
{
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    uint32_t dma2d_color;

    /* HAL_DMA2D_Start expects the R2M color as 0x00RRGGBB, while the
       application color constants are RGB565 values. Expand first. */
    red = (uint32_t)((color >> 11U) & 0x1FU);
    red = (red << 3U) | (red >> 2U);
    green = (uint32_t)((color >> 5U) & 0x3FU);
    green = (green << 2U) | (green >> 4U);
    blue = (uint32_t)(color & 0x1FU);
    blue = (blue << 3U) | (blue >> 2U);
    dma2d_color = (red << 16U) | (green << 8U) | blue;

    hdma2d.Init.Mode = DMA2D_R2M;
    hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = 0U;

    if (HAL_DMA2D_Init(&hdma2d) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_DMA2D_Start(&hdma2d, dma2d_color, LCD_FRAMEBUFFER, LCD_WIDTH, LCD_HEIGHT) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_DMA2D_PollForTransfer(&hdma2d, 1000U) != HAL_OK)
    {
        Error_Handler();
    }

    __DSB();
}

/**
 * @brief  设置 LCD 背光状态
 * @param  enable 1：打开背光；0：关闭背光
 * @retval 无
 */
void MIPI_LCD_SetBacklight(uint8_t enable)
{
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief  使用 DMA2D 填充屏幕上的一个矩形区域
 * @param  x      屏幕物理坐标 X，左上角为 0
 * @param  y      屏幕物理坐标 Y，左上角为 0
 * @param  width  矩形宽度
 * @param  height 矩形高度
 * @param  color  RGB565 格式的颜色值
 * @note   屏幕与帧缓冲区相差 90 度旋转，函数内部自动转换坐标
 * @retval 无
 */
void MIPI_LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    uint32_t dma2d_color;
    uint32_t address;
    uint16_t framebuffer_x;
    uint16_t framebuffer_y;
    uint16_t framebuffer_width;
    uint16_t framebuffer_height;

    if (x >= LCD_PHYSICAL_WIDTH || y >= LCD_PHYSICAL_HEIGHT)
    {
        return;
    }

    if (((uint32_t)x + width) > LCD_PHYSICAL_WIDTH)
    {
        width = LCD_PHYSICAL_WIDTH - x;
    }

    if (((uint32_t)y + height) > LCD_PHYSICAL_HEIGHT)
    {
        height = LCD_PHYSICAL_HEIGHT - y;
    }

    /* 屏幕物理坐标旋转到帧缓冲区坐标，旋转后宽高互换。 */
    framebuffer_x = y;
    framebuffer_y = (uint16_t)(LCD_HEIGHT - x - width);
    framebuffer_width = height;
    framebuffer_height = width;

    red = (uint32_t)((color >> 11U) & 0x1FU);
    red = (red << 3U) | (red >> 2U);

    green = (uint32_t)((color >> 5U) & 0x3FU);
    green = (green << 2U) | (green >> 4U);

    blue = (uint32_t)(color & 0x1FU);
    blue = (blue << 3U) | (blue >> 2U);

    dma2d_color = (red << 16U) | (green << 8U) | blue;

    hdma2d.Init.Mode = DMA2D_R2M;
    hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = LCD_WIDTH - framebuffer_width;

    if (HAL_DMA2D_Init(&hdma2d) != HAL_OK)
    {
        Error_Handler();
    }

    address =
        LCD_FRAMEBUFFER + ((uint32_t)framebuffer_y * LCD_WIDTH + framebuffer_x) * LCD_PIXEL_BYTES;

    if (HAL_DMA2D_Start(&hdma2d, dma2d_color, address, framebuffer_width, framebuffer_height) !=
        HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_DMA2D_PollForTransfer(&hdma2d, 1000U) != HAL_OK)
    {
        Error_Handler();
    }

    __DSB();
}

/**
  * @brief  测试屏幕四个物理角落的颜色位置
  * @note   FillRect 已在内部处理帧缓冲区旋转，因此这里直接使用屏幕坐标
  * @retval 无
  */
void LCD_Test_Corners(void)
{
    const uint16_t marker_size = 100U;

    MIPI_LCD_Clear(LCD_COLOR_BLACK);

    /* 屏幕左上角显示红色。 */
    MIPI_LCD_FillRect(0U, 0U, marker_size, marker_size, LCD_COLOR_RED);

    /* 屏幕右上角显示绿色。 */
    MIPI_LCD_FillRect(LCD_PHYSICAL_WIDTH - marker_size, 0U, marker_size, marker_size,
                      LCD_COLOR_GREEN);

    /* 屏幕左下角显示蓝色。 */
    MIPI_LCD_FillRect(0U, LCD_PHYSICAL_HEIGHT - marker_size, marker_size, marker_size,
                      LCD_COLOR_BLUE);

    /* 屏幕右下角显示白色。 */
    MIPI_LCD_FillRect(LCD_PHYSICAL_WIDTH - marker_size, LCD_PHYSICAL_HEIGHT - marker_size,
                      marker_size, marker_size, LCD_COLOR_WHITE);
}
