/*
 * 文件用途：FatFs 适配：把文件系统的磁盘操作转接到 bsp_sd.c，并向文件系统提供 RTC 时间。
 * 编译归属：CM7 应用、CM7 Bootloader。
 */
#include "ff.h"
#include "diskio.h"
#include "bsp_sd.h"
#include "bsp_rtc.h"

DSTATUS disk_initialize(BYTE drive)
{
    return drive || bsp_sd_init() ? STA_NOINIT : 0;
}

DSTATUS disk_status(BYTE drive)
{
    return drive || !bsp_sd_ready() ? STA_NOINIT : 0;
}

DRESULT disk_read(BYTE drive, BYTE *buffer, LBA_t sector, UINT count)
{
    return drive ? RES_PARERR : (bsp_sd_read(buffer, (uint32_t)sector, count) ? RES_ERROR : RES_OK);
}

DRESULT disk_write(BYTE drive, const BYTE *buffer, LBA_t sector, UINT count)
{
    return drive ? RES_PARERR
                 : (bsp_sd_write(buffer, (uint32_t)sector, count) ? RES_ERROR : RES_OK);
}

/* 向 FatFs 提供同步、扇区总数和 512 字节扇区大小等块设备信息。 */
DRESULT disk_ioctl(BYTE drive, BYTE cmd, void *buffer)
{
    if (drive)
        return RES_PARERR;
    if (!bsp_sd_ready())
        return RES_NOTRDY;
    switch (cmd)
    {
        case CTRL_SYNC:
            return bsp_sd_sync() ? RES_ERROR : RES_OK;
        case GET_SECTOR_COUNT:
            *(LBA_t *)buffer = bsp_sd_sectors();
            return RES_OK;
        case GET_SECTOR_SIZE:
            *(WORD *)buffer = 512;
            return RES_OK;
        case GET_BLOCK_SIZE:
            *(DWORD *)buffer = 1;
            return RES_OK;
        default:
            return RES_PARERR;
    }
}

DWORD get_fattime(void)
{
    return bsp_rtc_fattime();
}
