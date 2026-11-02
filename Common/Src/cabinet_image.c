/*
 * 文件用途：应用镜像说明：在 CABINET_INFO 段写入板卡、固件、核类型和协议版本，供升级校验读取。
 * 编译归属：CM7 应用、CM4 应用。
 */
#include "cabinet.h"
#ifdef CORE_CM7
#define IMAGE_CORE 7U
#else
#define IMAGE_CORE 4U
#endif
/* scatter 将此结构放在各应用起点 +0x400；Bootloader 据此核对两个镜像的核类型和配套版本。 */
__attribute__((section("CABINET_INFO"), used)) const CabinetImageInfo cabinet_image_info = {
    CABINET_INFO_MAGIC, CABINET_BOARD_ID, CABINET_FW_VERSION,
    IMAGE_CORE,         CABINET_PROTOCOL, {0, 0, 0}};
