/*
 * 文件用途：RTC 时间服务：LSE 初始化、文本时间校验与设置、日志时间和 FatFs 时间戳。
 * 编译归属：CM7 应用、CM7 Bootloader。
 */
#include "bsp_board.h"
#include "bsp_rtc.h"
static RTC_HandleTypeDef rtc;
static int ready;
#define RTC_SET_MAGIC 0x43414254U

int bsp_rtc_init(void)
{
    RCC_OscInitTypeDef o = {0};
    RCC_PeriphCLKInitTypeDef c = {0};
    HAL_PWR_EnableBkUpAccess();
    o.OscillatorType = RCC_OSCILLATORTYPE_LSE;
    o.LSEState = RCC_LSE_ON;
    o.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&o) != HAL_OK)
        return -1;
    c.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    c.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    if (HAL_RCCEx_PeriphCLKConfig(&c) != HAL_OK)
        return -1;
    __HAL_RCC_RTC_ENABLE();
    memset(&rtc, 0, sizeof(rtc));
    rtc.Instance = RTC;
    rtc.Init.HourFormat = RTC_HOURFORMAT_24;
    rtc.Init.AsynchPrediv = 127;
    rtc.Init.SynchPrediv = 255;
    rtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    rtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    rtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
    ready = HAL_RTC_Init(&rtc) == HAL_OK;
    return ready ? 0 : -1;
}

/* 只接受 2000..2099 的有效日期时间；写入备份标记后，日志才把 RTC 当成已经设置。 */
int bsp_rtc_set_text(const char *text)
{
    unsigned y, m, d, hh, mm, ss;
    char tail;
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    RTC_TimeTypeDef t = {0};
    RTC_DateTypeDef date = {0};
    if (!ready || sscanf(text, "%u-%u-%u %u:%u:%u%c", &y, &m, &d, &hh, &mm, &ss, &tail) != 6)
        return -1;
    if (y < 2000 || y > 2099 || m < 1 || m > 12 || d < 1 ||
        d > (unsigned)(days[m - 1] + (m == 2 && y % 4 == 0)) || hh > 23 || mm > 59 || ss > 59)
        return -1;
    t.Hours = hh;
    t.Minutes = mm;
    t.Seconds = ss;
    t.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    t.StoreOperation = RTC_STOREOPERATION_RESET;
    date.Year = y - 2000;
    date.Month = m;
    date.Date = d;
    {
        static const unsigned offset[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        unsigned year = y - (m < 3),
                 weekday = (year + year / 4 - year / 100 + year / 400 + offset[m - 1] + d) % 7;
        date.WeekDay = weekday ? weekday : 7;
    }
    if (HAL_RTC_SetTime(&rtc, &t, RTC_FORMAT_BIN) != HAL_OK ||
        HAL_RTC_SetDate(&rtc, &date, RTC_FORMAT_BIN) != HAL_OK)
        return -1;
    HAL_RTCEx_BKUPWrite(&rtc, RTC_BKP_DR0, RTC_SET_MAGIC);
    return 0;
}

static int read_clock(RTC_TimeTypeDef *t, RTC_DateTypeDef *d)
{
    if (!ready || HAL_RTCEx_BKUPRead(&rtc, RTC_BKP_DR0) != RTC_SET_MAGIC)
        return -1;
    if (HAL_RTC_GetTime(&rtc, t, RTC_FORMAT_BIN) != HAL_OK ||
        HAL_RTC_GetDate(&rtc, d, RTC_FORMAT_BIN) != HAL_OK)
        return -1;
    return 0;
}

int bsp_rtc_text(char *text, size_t n)
{
    RTC_TimeTypeDef t;
    RTC_DateTypeDef d;
    if (read_clock(&t, &d))
    {
        snprintf(text, n, "UNSET");
        return -1;
    }
    snprintf(text, n, "%04u-%02u-%02u %02u:%02u:%02u", 2000U + d.Year, d.Month, d.Date, t.Hours,
             t.Minutes, t.Seconds);
    return 0;
}

uint32_t bsp_rtc_fattime(void)
{
    RTC_TimeTypeDef t;
    RTC_DateTypeDef d;
    if (read_clock(&t, &d))
        return (46UL << 25) | (1UL << 21) | (1UL << 16); /* 2026-01-01; CSV says UNSET. */
    return ((uint32_t)(d.Year + 20U) << 25) | ((uint32_t)d.Month << 21) | ((uint32_t)d.Date << 16) |
           ((uint32_t)t.Hours << 11) | ((uint32_t)t.Minutes << 5) | (t.Seconds / 2U);
}
