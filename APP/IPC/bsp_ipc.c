/*
 * 文件用途：双核消息接口：登记业务回调、建立 RPMsg 端点、发送消息，并在任务中轮询接收。
 * 编译归属：CM7 应用、CM4 应用。
 */
#include "bsp_ipc.h"
#include "openamp.h"
#include <string.h>
/* 每个核各有一份端点和回调变量；共用源文件不会让普通变量自动成为共享内存。 */
static struct rpmsg_endpoint endpoint;
static CabinetIpcReceive on_receive;
static int created;
#define CHANNEL "cabinet.monitor.v1"

/* 通信层接收：先检查长度并复制到本核栈，再校验项目消息头；业务回调必须在返回前复制需要保留的数据。 */
static int receive(struct rpmsg_endpoint *e, void *data, size_t size, uint32_t source,
                   void *private_data)
{
    CabinetMessage message;
    (void)e;
    (void)source;
    (void)private_data;
    if (size != sizeof(message))
        return 0;
    memcpy(&message, data, sizeof(message));
    if (message.magic != 0x4341424DUL || message.protocol != CABINET_PROTOCOL ||
        message.size != sizeof(message))
        return 0;
    if (on_receive)
        on_receive(&message);
    return 0;
}

static void unbind(struct rpmsg_endpoint *e)
{
    (void)e;
    created = 0;
}
#ifdef CORE_CM7
/* CM7 处理 CM4 的同名服务公告，在本核建立端点，并记下公告中的对端地址。 */
static void bind(struct rpmsg_device *device, const char *name, uint32_t destination)
{
    (void)device;
    if (created || strcmp(name, CHANNEL))
        return;
    if (!OPENAMP_create_endpoint(&endpoint, CHANNEL, destination, receive, unbind))
        created = 1;
}
#endif
/* 保存本核业务接收函数，再按编译目标选择 MASTER/REMOTE；此处登记回调，不直接执行业务接收。 */
int bsp_ipc_init(CabinetIpcReceive callback)
{
    on_receive = callback;
    created = 0;
#ifdef CORE_CM7 //根据对应核选择对应角色，CM7负责提供、管理共享缓冲区，CM4负责使用共享缓冲区
    return MX_OPENAMP_Init(RPMSG_MASTER, bind);
#else
    if (MX_OPENAMP_Init(RPMSG_REMOTE, 0))
        return -1;
    if (OPENAMP_create_endpoint(&endpoint, CHANNEL, RPMSG_ADDR_ANY, receive, unbind))
        return -1;
    created = 1;
    return 0;
#endif
}

/* 由本核任务反复调用；接收回调在调用 poll 的任务上下文执行。 */
void bsp_ipc_poll(void)
{
    OPENAMP_check_for_message();
}

/* 端点创建成功后，还需要本端和对端地址均已确定，才能发送普通业务消息。 */
int bsp_ipc_ready(void)
{
    return created && is_rpmsg_ept_ready(&endpoint);
}

/* 复制发送整个 CabinetMessage；trysend 无缓冲区时立即失败，成功不代表对端业务已经执行完。 */
int bsp_ipc_send(const CabinetMessage *message)
{
    if (!bsp_ipc_ready())
        return -1;
    return rpmsg_trysend(&endpoint, (void *)message, sizeof(*message)) < 0 ? -1 : 0;
}
