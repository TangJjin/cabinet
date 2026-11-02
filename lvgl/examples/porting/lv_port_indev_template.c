#include "lv_port_indev_template.h"
#include "bsp_system.h"
/* Preserve the established touch coordinate conversion in Touch_Read(). */
static void touchpad_read(lv_indev_t *device,lv_indev_data_t *data)
{
    static int32_t last_x,last_y;
    Touch_DataTypeDef touch={0};(void)device;
    if(Touch_Read(&touch)!=0U){last_x=touch.x;last_y=touch.y;data->state=LV_INDEV_STATE_PRESSED;}
    else data->state=LV_INDEV_STATE_RELEASED;
    data->point.x=last_x;data->point.y=last_y;
}
void lv_port_indev_init(void)
{
    lv_indev_t *touchpad=lv_indev_create();
    lv_indev_set_type(touchpad,LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touchpad,touchpad_read);
}
