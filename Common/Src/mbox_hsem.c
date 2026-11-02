/*
 * 文件用途：核间门铃：HSEM 0/1 负责通知，ISR 置 pending，任务轮询时才推进 OpenAMP 接收。
 * 编译归属：CM7 应用、CM4 应用。
 */
/* Based on ST's HSEM mailbox. Boot rendezvous uses a separate semaphore (15). */
#include "openamp/open_amp.h"
#include "stm32h7xx_hal.h"
#include "openamp_conf.h"
#include "mbox_hsem.h"
static volatile uint32_t pending = 1;

/* 对端释放约定的硬件信号量后进入此中断回调；只置检查标志，并重新使能后续通知。 */
void HAL_HSEM_FreeCallback(uint32_t mask)
{
#ifdef CORE_CM7
    if (mask & (1UL << HSEM_ID_1))
    {
        pending = 1;
        HAL_HSEM_ActivateNotification(1UL << HSEM_ID_1);
    }
#else
    if (mask & (1UL << HSEM_ID_0))
    {
        pending = 1;
        HAL_HSEM_ActivateNotification(1UL << HSEM_ID_0);
    }
#endif
}

/* 约定 CM7 发通知用 HSEM 0、CM4 发通知用 HSEM 1；接收核监听对端所释放的编号。 */
int MAILBOX_Init(void)
{
    __HAL_RCC_HSEM_CLK_ENABLE();
    pending = 1;
#ifdef CORE_CM7
    HAL_NVIC_SetPriority(HSEM1_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(HSEM1_IRQn);
    HAL_HSEM_ActivateNotification(1UL << HSEM_ID_1);
#else
    HAL_NVIC_SetPriority(HSEM2_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(HSEM2_IRQn);
    HAL_HSEM_ActivateNotification(1UL << HSEM_ID_0);
#endif
    return 0;
}

/* pending 仅表示需要检查，不是消息数量；CM7 检查接收 VRing 0，CM4 检查接收 VRing 1。 */
int MAILBOX_Poll(struct virtio_device *vdev)
{
    /* Clear before processing so an IRQ during the callback is not lost. */
    if (pending)
    {
        pending = 0;
        __DMB();
#ifdef CORE_CM7
        rproc_virtio_notified(vdev, VRING0_ID);
#else
        rproc_virtio_notified(vdev, VRING1_ID);
#endif
        return 0;
    }
    return -EAGAIN;
}

/* 消息已写入共享 SRAM/VRing 后，先取得再释放信号量，借释放事件通知对端；缓冲区归还由 VRing 管理。 */
int MAILBOX_Notify(void *private_data, uint32_t id)
{
    uint32_t sem;
    (void)private_data;
    (void)id;
    __DMB();
#ifdef CORE_CM7
    sem = HSEM_ID_0;
#else
    sem = HSEM_ID_1;
#endif
    if (HAL_HSEM_FastTake(sem) != HAL_OK)
        return -EAGAIN;
    HAL_HSEM_Release(sem, 0);
    return 0;
}
