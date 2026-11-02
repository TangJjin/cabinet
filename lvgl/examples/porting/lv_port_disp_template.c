/**
 * @file lv_port_disp_templ.c
 *
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp_template.h"
#include <stdbool.h>
#include "bsp_system.h"

/*********************
 *      DEFINES
 *********************/
#define MY_DISP_HOR_RES    1280
#define MY_DISP_VER_RES    800

#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565)) /*will be 2 for RGB565 */

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

/**********************
 *  STATIC VARIABLES
 **********************/
volatile uint32_t g_flush_last_us = 0;
volatile uint32_t g_flush_max_us = 0;
volatile uint32_t g_flush_total_us = 0;
volatile uint32_t g_flush_count = 0;
volatile uint32_t g_flush_max_pixels = 0;
/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*------------------------------------
     * Create a display and set a flush_cb
     * -----------------------------------*/
    lv_display_t * disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    lv_display_set_flush_cb(disp, disp_flush);

    /* Example 1
     * One buffer for partial rendering*/
    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t buf_1_1[MY_DISP_HOR_RES * 60 * BYTE_PER_PIXEL];            /*A buffer for 10 rows*/
    lv_display_set_buffers(disp, buf_1_1, NULL, sizeof(buf_1_1), LV_DISPLAY_RENDER_MODE_PARTIAL);

//    /* Example 2
//     * Two buffers for partial rendering
//     * In flush_cb DMA or similar hardware should be used to update the display in the background.*/
//    LV_ATTRIBUTE_MEM_ALIGN
//    static uint8_t buf_2_1[MY_DISP_HOR_RES * 40 * BYTE_PER_PIXEL];

//    LV_ATTRIBUTE_MEM_ALIGN
//    static uint8_t buf_2_2[MY_DISP_HOR_RES * 40 * BYTE_PER_PIXEL];
//    lv_display_set_buffers(disp, buf_2_1, buf_2_2, sizeof(buf_2_1), LV_DISPLAY_RENDER_MODE_PARTIAL);

//    /* Example 3
//     * Two buffers screen sized buffer for double buffering.
//     * Both LV_DISPLAY_RENDER_MODE_DIRECT and LV_DISPLAY_RENDER_MODE_FULL works, see their comments*/
//    LV_ATTRIBUTE_MEM_ALIGN
//    static uint8_t buf_3_1[MY_DISP_HOR_RES * MY_DISP_VER_RES * BYTE_PER_PIXEL];

//    LV_ATTRIBUTE_MEM_ALIGN
//    static uint8_t buf_3_2[MY_DISP_HOR_RES * MY_DISP_VER_RES * BYTE_PER_PIXEL];
//    lv_display_set_buffers(disp, buf_3_1, buf_3_2, sizeof(buf_3_1), LV_DISPLAY_RENDER_MODE_DIRECT);

}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
    /*You code here*/
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/*Flush the content of the internal buffer the specific area on the display.
 *`px_map` contains the rendered image as raw pixel map and it should be copied to `area` on the display.
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_display_flush_ready()' has to be called when it's finished.*/
//static void disp_flush(lv_display_t * disp_drv, const lv_area_t * area, uint8_t * px_map)
//{
//    if(disp_flush_enabled) {
//        /*The most simple case (but also the slowest) to put all pixels to the screen one-by-one*/

//        int32_t x;
//        int32_t y;
//        for(y = area->y1; y <= area->y2; y++) {
//            for(x = area->x1; x <= area->x2; x++) {
//                /*Put a pixel to the display. For example:*/
//                /*put_px(x, y, *px_map)*/
//                px_map++;
//            }
//        }
//    }

//    /*IMPORTANT!!!
//     *Inform the graphics library that you are ready with the flushing*/
//    lv_display_flush_ready(disp_drv);
//}

static void disp_flush(lv_display_t *disp,
                       const lv_area_t *area,
                       uint8_t *px_map)
{
	uint32_t start_cycle = DWT->CYCCNT;
	
	uint32_t width =
        (uint32_t)(area->x2 - area->x1 + 1);

    uint32_t height =
        (uint32_t)(area->y2 - area->y1 + 1);

    uint32_t pixels = width * height;
	
	/********************************************/
	
    uint16_t *src = (uint16_t *)px_map;

    for(int32_t y = area->y1; y <= area->y2; y++) {
        for(int32_t x = area->x1; x <= area->x2; x++) {
            uint32_t fb_x = (uint32_t)y;
            uint32_t fb_y = LCD_HEIGHT - (uint32_t)x - 1U;

            LCD_FB[fb_y * LCD_WIDTH + fb_x] = *src++;
        }
    }

    lv_display_flush_ready(disp);
	
	/********************************************/
	
	uint32_t elapsed_cycle = DWT->CYCCNT - start_cycle;
    uint32_t elapsed_us =
        elapsed_cycle / (SystemCoreClock / 1000000U);

    g_flush_last_us = elapsed_us;
    g_flush_total_us += elapsed_us;
    g_flush_count++;

    if(elapsed_us > g_flush_max_us) {
        g_flush_max_us = elapsed_us;
    }

    if(pixels > g_flush_max_pixels) {
        g_flush_max_pixels = pixels;
    }
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
