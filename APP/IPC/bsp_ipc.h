/*
 * 文件用途：接口与类型声明：双核消息接口：登记业务回调、建立 RPMsg 端点、发送消息，并在任务中轮询接收。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef BSP_IPC_H
#define BSP_IPC_H
#include "cabinet.h"
typedef void (*CabinetIpcReceive)(const CabinetMessage *message);
int bsp_ipc_init(CabinetIpcReceive receive);
void bsp_ipc_poll(void);
int bsp_ipc_send(const CabinetMessage *message);
int bsp_ipc_ready(void);
#endif
