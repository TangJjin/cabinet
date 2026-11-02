/*
 * 文件用途：接口与类型声明：RTC 时间服务：LSE 初始化、文本时间校验与设置、日志时间和 FatFs 时间戳。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef BSP_RTC_H
#define BSP_RTC_H
#include <stdint.h>
#include <stddef.h>
int bsp_rtc_init(void);
int bsp_rtc_set_text(const char *text);
int bsp_rtc_text(char *text, size_t capacity);
uint32_t bsp_rtc_fattime(void);
#endif
