/*
 * 文件用途：C 运行库适配：关闭半主机依赖，处理断言/退出，并按目标提供最小打印占位。
 * 编译归属：CM7 应用、CM4 应用、CM7 Bootloader、CM4 启动门。
 */
#include "bsp_board.h"
#if defined(__ARMCC_VERSION)
__asm(".global __use_no_semihosting");

void _sys_exit(int code)
{
    (void)code;
    bsp_fatal();
}

void _ttywrch(int ch)
{
    (void)ch;
}

void __aeabi_assert(const char *expression, const char *file, int line)
{
    (void)expression;
    (void)file;
    (void)line;
    bsp_fatal();
}
#if !defined(CORE_CM7) || defined(CABINET_BOOTLOADER)
int fputc(int ch, FILE *stream)
{
    (void)stream;
    return ch;
}
#endif
#endif
