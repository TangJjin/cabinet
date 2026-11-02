/*
 * 文件用途：接口与类型声明：串口环形缓冲区：管理读写位置、可读数据量及剩余空间。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>

/* 实际可存储 RINGBUFFER_SIZE - 1 个字节。 */
#define RINGBUFFER_SIZE 512U

typedef struct
{
    volatile uint32_t write_index;
    volatile uint32_t read_index;
    uint8_t buffer[RINGBUFFER_SIZE];
} ringbuffer_t;

void ringbuffer_init(ringbuffer_t *rb);
uint8_t ringbuffer_is_full(const ringbuffer_t *rb);
uint8_t ringbuffer_is_empty(const ringbuffer_t *rb);
uint32_t ringbuffer_available(const ringbuffer_t *rb);
uint32_t ringbuffer_free_space(const ringbuffer_t *rb);
int8_t ringbuffer_write(ringbuffer_t *rb, const uint8_t *data, uint32_t length);
int8_t ringbuffer_read(ringbuffer_t *rb, uint8_t *data, uint32_t length);

extern ringbuffer_t usart_rb1;

#endif
