/*
 * 文件用途：CM7 调试串口：USART1 的打印重定向、DMA 接收及环形缓冲接入。
 * 编译归属：CM7 应用。
 */
#include "bsp_uart.h"

ringbuffer_t usart_rb1;
uint8_t usart1_read_buffer[128];
volatile uint32_t uart1_dropped_bytes;

/**
 * @brief 初始化 UART 软件缓冲区并启动 DMA 空闲接收。
 */
void uart_init(void)
{
    ringbuffer_init(&usart_rb1);
    memset(uart_rx_dma_buffer, 0, sizeof(uart_rx_dma_buffer));
    memset(usart1_read_buffer, 0, sizeof(usart1_read_buffer));
    uart1_dropped_bytes = 0U;
}

/**
 * @brief UART 空闲事件回调，将 DMA 收到的数据放入环形缓冲区。
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
    if (huart != &huart1)
    {
        return;
    }

    if (ringbuffer_write(&usart_rb1, uart_rx_dma_buffer, size) != 0)
    {
        uart1_dropped_bytes += size;
    }

    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_dma_buffer, sizeof(uart_rx_dma_buffer));
    __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
}

/**
 * @brief 从环形缓冲区取出数据并通过 USART1 输出。
 */
void uart_proc(void)
{
    uint32_t length = ringbuffer_available(&usart_rb1);

    if (length == 0U)
    {
        return;
    }

    if (length > (sizeof(usart1_read_buffer) - 1U))
    {
        length = sizeof(usart1_read_buffer) - 1U;
    }

    if (ringbuffer_read(&usart_rb1, usart1_read_buffer, length) != 0)
    {
        return;
    }

    //    usart1_read_buffer[length] = '\0';
    //    printf("uart data:");
    //    HAL_UART_Transmit(&huart1, usart1_read_buffer, length, 100U);
    //    printf("\r\n");
    printf("%s\r\n", usart1_read_buffer);
    memset(usart1_read_buffer, 0, sizeof(usart1_read_buffer));
}
