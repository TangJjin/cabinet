/*
 * 文件用途：串口环形缓冲区：管理读写位置、可读数据量及剩余空间。
 * 编译归属：CM7 应用。
 */
#include "ringbuffer.h"

#include <stddef.h>
#include <string.h>
#include "cmsis_compiler.h"

/**
 * @brief 初始化环形缓冲区。
 * @param rb 环形缓冲区对象。
 */
void ringbuffer_init(ringbuffer_t *rb)
{
    rb->write_index = 0U;
    rb->read_index = 0U;
    memset(rb->buffer, 0, sizeof(rb->buffer));
}

/**
 * @brief 判断环形缓冲区是否已满。
 */
uint8_t ringbuffer_is_full(const ringbuffer_t *rb)
{
    uint32_t next_write = (rb->write_index + 1U) % RINGBUFFER_SIZE;

    return (next_write == rb->read_index) ? 1U : 0U;
}

/**
 * @brief 判断环形缓冲区是否为空。
 */
uint8_t ringbuffer_is_empty(const ringbuffer_t *rb)
{
    return (rb->write_index == rb->read_index) ? 1U : 0U;
}

/**
 * @brief 获取当前可读取的字节数。
 */
uint32_t ringbuffer_available(const ringbuffer_t *rb)
{
    uint32_t write_index = rb->write_index;
    uint32_t read_index = rb->read_index;

    if (write_index >= read_index)
    {
        return write_index - read_index;
    }

    return RINGBUFFER_SIZE - read_index + write_index;
}

/**
 * @brief 获取当前还能写入的字节数。
 */
uint32_t ringbuffer_free_space(const ringbuffer_t *rb)
{
    return (RINGBUFFER_SIZE - 1U) - ringbuffer_available(rb);
}

/**
 * @brief 向环形缓冲区写入一组数据。
 * @note 本函数由 UART 接收回调调用，是单一写入者。
 * @retval 0 写入成功；-1 参数错误或剩余空间不足。
 */
int8_t ringbuffer_write(ringbuffer_t *rb, const uint8_t *data, uint32_t length)
{
    uint32_t write_index;

    if ((rb == NULL) || (data == NULL) || (length > ringbuffer_free_space(rb)))
    {
        return -1;
    }

    write_index = rb->write_index;

    for (uint32_t i = 0U; i < length; i++)
    {
        rb->buffer[write_index] = data[i];
        write_index = (write_index + 1U) % RINGBUFFER_SIZE;
    }

    /* 先写数据，再发布新的写指针。 */
    __DMB();
    rb->write_index = write_index;

    return 0;
}

/**
 * @brief 从环形缓冲区读取一组数据。
 * @note 本函数由主循环调用，是单一读取者。
 * @retval 0 读取成功；-1 参数错误或可读数据不足。
 */
int8_t ringbuffer_read(ringbuffer_t *rb, uint8_t *data, uint32_t length)
{
    uint32_t read_index;

    if ((rb == NULL) || (data == NULL) || (length > ringbuffer_available(rb)))
    {
        return -1;
    }

    read_index = rb->read_index;

    for (uint32_t i = 0U; i < length; i++)
    {
        data[i] = rb->buffer[read_index];
        read_index = (read_index + 1U) % RINGBUFFER_SIZE;
    }

    /* 数据读取完成后，再发布新的读指针。 */
    __DMB();
    rb->read_index = read_index;

    return 0;
}
