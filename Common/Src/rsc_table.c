/*
 * 文件用途：共享资源表：由 CM7 写入两条 VRing 的地址、容量和通知编号，CM4 读取同一份布局。
 * 编译归属：CM7 应用、CM4 应用。
 */
/* Resource layout follows ST's STM32CubeH7 OpenAMP_PingPong example. */
#include "rsc_table.h"
#include <stddef.h>
#include <string.h>
#include "stm32h7xx_hal.h"

/* 仅 MASTER 清零并填写共享资源表；REMOTE 保留内容，避免覆盖 CM7 已经建立的队列。 */
void resource_table_init(int role, void **table, int *length)
{
    struct shared_resource_table *r = (struct shared_resource_table *)0x38000000UL;
    if (role == RPMSG_MASTER)
    {
        memset(r, 0, sizeof(*r));
        r->version = 1;
        r->num = 1;
        r->offset[0] = offsetof(struct shared_resource_table, vdev);
        r->vdev.type = RSC_VDEV;
        r->vdev.id = 7;
        r->vdev.dfeatures = 1;
        r->vdev.num_of_vrings = 2;
        r->vring0.da = VRING_TX_ADDRESS;
        r->vring0.align = VRING_ALIGNMENT;
        r->vring0.num = VRING_NUM_BUFFS;
        r->vring0.notifyid = VRING0_ID;
        r->vring1.da = VRING_RX_ADDRESS;
        r->vring1.align = VRING_ALIGNMENT;
        r->vring1.num = VRING_NUM_BUFFS;
        r->vring1.notifyid = VRING1_ID;
        __DMB();
    }
    *table = r;
    *length = sizeof(*r);
}
