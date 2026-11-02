/*
 * 文件用途：接口与类型声明：CM7 调试串口：USART1 的打印重定向、DMA 接收及环形缓冲接入。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef __BSP_UART__
#define __BSP_UART__

#include "bsp_system.h"

void uart_init(void);
void uart_proc(void);

extern volatile uint32_t uart1_dropped_bytes;

#endif
