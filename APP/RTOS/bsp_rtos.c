/*
 * 文件用途：保留的旧演示任务，当前四个 Keil 目标均未编译；现用任务在 bsp_cabinet.c、bsp_sensor.c 等文件。
 * 编译归属：保留代码，当前四目标未编译。
 */
#include "bsp_rtos.h"

uint64_t HAL_Tick = 0;
uint64_t HAL_Tick_old = 0;
float speed = 0;
float altitude = 0;
float pitch_deg = 0;
float roll_deg = 0;
float yaw_deg = 0;

void freertos_test_task(void *argument)
{
    (void)argument;

    for (;;)
    {
        green_led(2);                   // 翻转绿灯
        vTaskDelay(pdMS_TO_TICKS(500)); // 500ms
    }
}

void lvgl_task(void *argument)
{
    (void)argument;

    static uint32_t max_us = 0;
    static uint32_t sum_us = 0;
    static uint32_t count = 0;
    static uint32_t last_report_tick = 0;

    for (;;)
    {
        HAL_Tick = HAL_GetTick();
        if (HAL_Tick >= 50 + HAL_Tick_old)
        {
            HAL_Tick_old = HAL_Tick;
            pitch_deg -= 1.0f;
            roll_deg -= 1.0f;
            yaw_deg -= 1.0f;
            speed += 1.0f;
            altitude -= 1.0f;
            if (yaw_deg >= 360.0f || yaw_deg <= -360.0f)
            {
                if (yaw_deg >= 360.0f)
                    yaw_deg -= 360.0f;
                else if (yaw_deg <= -360.0f)
                    yaw_deg += 360.0f;
            }
            if (pitch_deg >= 360.0f || pitch_deg <= -360.0f)
            {
                if (pitch_deg >= 360.0f)
                    pitch_deg -= 360.0f;
                else if (pitch_deg <= -360.0f)
                    pitch_deg += 360.0f;
            }
            if (roll_deg >= 360.0f || roll_deg <= -360.0f)
            {
                if (roll_deg >= 360.0f)
                    roll_deg -= 360.0f;
                else if (roll_deg <= -360.0f)
                    roll_deg += 360.0f;
            }
            Horizon_SetAttitude(pitch_deg, roll_deg);
            left_SetAttitude(pitch_deg);
            top_SetAttitude(roll_deg);
            down_SetAttitude(yaw_deg);
            left_SetNum(speed);
            right_SetNum(altitude);
        }

        uint32_t start_cycle = DWT->CYCCNT;

        lv_timer_handler();

        uint32_t elapsed_cycle = DWT->CYCCNT - start_cycle;
        uint32_t elapsed_us = elapsed_cycle / (SystemCoreClock / 1000000U);

        if (elapsed_us > max_us)
        {
            max_us = elapsed_us;
        }

        sum_us += elapsed_us;
        count++;

        if (HAL_GetTick() - last_report_tick >= 1000U)
        {
            last_report_tick = HAL_GetTick();

            printf("LVGL avg=%lu us, max=%lu us, count=%lu\r\n", count ? sum_us / count : 0, max_us,
                   count);
            printf("LVGL avg=%lu us, max=%lu us, count=%lu | "
                   "flush last=%lu us, max=%lu us, count=%lu, max_pixels=%lu\r\n",
                   count ? sum_us / count : 0, max_us, count, g_flush_last_us, g_flush_max_us,
                   g_flush_count, g_flush_max_pixels);

            g_flush_last_us = 0;
            g_flush_max_us = 0;
            g_flush_total_us = 0;
            g_flush_count = 0;
            g_flush_max_pixels = 0;

            sum_us = 0;
            max_us = 0;
            count = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
