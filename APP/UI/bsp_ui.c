/*
 * 文件用途：保留的旧姿态仪表界面，当前四个 Keil 目标均未编译；机柜界面使用 bsp_cabinet_ui.c。
 * 编译归属：保留代码，当前四目标未编译。
 */
#include "bsp_ui.h"

uint64_t left_HAL_Tick = 0;
uint64_t left_HAL_Tick_old = 0;
int16_t left_high = 0;                             //左侧速度数值
int16_t right_high = 0;                            //右侧高度数值
int16_t set_yaw_deg = 0;                           //下侧偏航数值
int16_t set_pitch_deg = 0;                         //左侧俯仰数值
int16_t Horizon_angle = 0;                         //地平线容器旋转角度
uint16_t Horizon_center_x = 270;                   //地平线容器中心x值
uint16_t Horizon_center_y = 400;                   //地平线容器中心y值
static lv_point_precise_t horizon_left_points[2];  //地平线左线两端点
static lv_point_precise_t horizon_right_points[2]; //地平线右线两端点

/**
 * @brief 屏幕坐标转换成线控件自己的局部坐标
 * @param line 要更新的 LVGL 线控件
 * @param points 相对于 line 控件左上角的局部坐标
 * @param x1,y1,x2,y2 line 两个端点相对容器的坐标
 */
static void Horizon_UpdateLine(lv_obj_t *line, lv_point_precise_t points[2], int32_t x1, int32_t y1,
                               int32_t x2, int32_t y2)
{
    //让线段周围多留出 4 个像素
    const int32_t margin = 4;

    int32_t min_x = (x1 < x2) ? x1 : x2; //找出左边界
    int32_t min_y = (y1 < y2) ? y1 : y2; //找出上边界
    int32_t max_x = (x1 > x2) ? x1 : x2; //找出右边界
    int32_t max_y = (y1 > y2) ? y1 : y2; //找出下边界

    //整体线控件相对容器左上角坐标值
    int32_t obj_x = min_x - margin;
    int32_t obj_y = min_y - margin;

    //获取到线相对于线控件的坐标
    points[0].x = x1 - obj_x;
    points[0].y = y1 - obj_y;
    points[1].x = x2 - obj_x;
    points[1].y = y2 - obj_y;

    //移动整体线控件的位置
    lv_obj_set_pos(line, obj_x, obj_y);
    //计算线控件大小
    lv_obj_set_size(line, max_x - min_x + margin * 2 + 1, max_y - min_y + margin * 2 + 1);

    //请在这个 line 控件内部，绘制两个点之间的线段
    lv_line_set_points(line, points, 2);
}

void Horizon_Init(lv_obj_t *parent)
{
    //让 LVGL 立即计算并更新 parent 当前的实际尺寸和布局
    lv_obj_update_layout(parent);

    Horizon_center_x = lv_obj_get_content_width(parent) / 2;
    Horizon_center_y = lv_obj_get_content_height(parent) / 2;
}

/**
 * @brief 设置地平线
 * @param pitch_deg 俯仰角
 * @param roll_deg 滚转角
 */
void Horizon_SetAttitude(float pitch_deg, float roll_deg)
{
    /********************* 平移操作 *********************/

    //对容器进行上下移动模拟俯仰角
    set_pitch_deg = (130.0f / -90.0f) * pitch_deg + 400.0f;
    if (pitch_deg > 180) //180对应-180;220对应-140
    {
        pitch_deg = (-40.0f / -40.0f) * (pitch_deg - 180.0f) - 180.0f;
        set_pitch_deg = (130.0f / -90.0f) * pitch_deg + 400.0f;
    }
    else if (pitch_deg < -180) //-180对应180;-220对应140
    {
        pitch_deg = (-40.0f / -40.0f) * (pitch_deg + 180.0f) + 180.0f;
        set_pitch_deg = (130.0f / -90.0f) * pitch_deg + 400.0f;
    }

    //把地平线的上下浮动限制在±90°内
    if ((pitch_deg >= 90 && pitch_deg <= 180) || (pitch_deg < -180 && pitch_deg >= -360))
    {
        set_pitch_deg = (130.0f / -90.0f) * 90 + 400.0f;
    }
    else if ((pitch_deg <= -90 && pitch_deg >= -180) || (pitch_deg >= 180 && pitch_deg <= 360))
    {
        set_pitch_deg = (130.0f / -90.0f) * -90 + 400.0f;
    }

    Horizon_center_y = set_pitch_deg; //400对应0;270对应90

    /********************* 旋转操作 *********************/

    float half_length = 170.0f; //中心到地平线最外端的距离
    float half_gap = 50.0f;     //中心到地平线最内端的距离

    //零角度端点
    horizon_left_points[0].x = Horizon_center_x - half_length;
    horizon_left_points[0].y = Horizon_center_y;

    horizon_left_points[1].x = Horizon_center_x - half_gap;
    horizon_left_points[1].y = Horizon_center_y;

    horizon_right_points[0].x = Horizon_center_x + half_gap;
    horizon_right_points[0].y = Horizon_center_y;

    horizon_right_points[1].x = Horizon_center_x + half_length;
    horizon_right_points[1].y = Horizon_center_y;

    //旋转操作
    float angle = -roll_deg * 3.1415926f / 180.0f;
    float cos_value = cosf(angle);
    float sin_value = sinf(angle);

    //原地旋转后端点
    horizon_left_points[0].x = Horizon_center_x - half_length * cos_value;
    horizon_left_points[0].y = Horizon_center_y - half_length * sin_value;

    horizon_left_points[1].x = Horizon_center_x - half_gap * cos_value;
    horizon_left_points[1].y = Horizon_center_y - half_gap * sin_value;

    horizon_right_points[0].x = Horizon_center_x + half_gap * cos_value;
    horizon_right_points[0].y = Horizon_center_y + half_gap * sin_value;

    horizon_right_points[1].x = Horizon_center_x + half_length * cos_value;
    horizon_right_points[1].y = Horizon_center_y + half_length * sin_value;

    Horizon_UpdateLine(guider_ui.screen_line_6, horizon_left_points,
                       (int32_t)horizon_left_points[0].x, (int32_t)horizon_left_points[0].y,
                       (int32_t)horizon_left_points[1].x, (int32_t)horizon_left_points[1].y);

    Horizon_UpdateLine(guider_ui.screen_line_7, horizon_right_points,
                       (int32_t)horizon_right_points[0].x, (int32_t)horizon_right_points[0].y,
                       (int32_t)horizon_right_points[1].x, (int32_t)horizon_right_points[1].y);
}

/**
 * @brief 设置左侧俯仰表数值
 * @param pitch_deg 偏航角
 */
void left_SetAttitude(float pitch_deg)
{
    int16_t set_pitch_deg_in = 0;

    char text[16];
    snprintf(text, sizeof(text), "%.1f°", pitch_deg);
    //设置label数据函数
    lv_label_set_text(guider_ui.screen_label_5, text);

    //set_pitch_deg_in、pitch_deg函数对应关系
    set_pitch_deg_in = (-90.0f / 180.0f) * pitch_deg + 90.0f;
    if (pitch_deg > 180) //180对应-180;220对应-140
    {
        pitch_deg = (-40.0f / -40.0f) * (pitch_deg - 180.0f) - 180.0f;
        set_pitch_deg_in = (-90.0f / 180.0f) * pitch_deg + 90.0f;
    }
    else if (pitch_deg < -180) //-180对应180;-220对应140
    {
        pitch_deg = (-40.0f / -40.0f) * (pitch_deg + 180.0f) + 180.0f;
        set_pitch_deg_in = (-90.0f / 180.0f) * pitch_deg + 90.0f;
    }

    //设置meter控件偏移量
    lv_scale_set_rotation(guider_ui.screen_meter_3, set_pitch_deg_in); //90对应0;180对应-180
}

void top_SetAttitude(float roll_deg)
{
    int16_t roll_deg_deg_in = 0;

    char text[16];
    snprintf(text, sizeof(text), "%.1f°", roll_deg);
    //设置label数据函数
    lv_label_set_text(guider_ui.screen_label_3, text);

    //set_pitch_deg_in、pitch_deg函数对应关系
    roll_deg_deg_in = (-90.0f / 180.0f) * roll_deg + 180.0f;
    if (roll_deg > 180) //180对应-180;220对应-140
    {
        roll_deg = (-40.0f / -40.0f) * (roll_deg - 180.0f) - 180.0f;
        roll_deg_deg_in = (-90.0f / 180.0f) * roll_deg + 180.0f;
    }
    else if (roll_deg < -180) //-180对应180;-220对应140
    {
        roll_deg = (-40.0f / -40.0f) * (roll_deg + 180.0f) + 180.0f;
        roll_deg_deg_in = (-90.0f / 180.0f) * roll_deg + 180.0f;
    }

    //设置meter控件偏移量
    lv_scale_set_rotation(guider_ui.screen_meter_2, roll_deg_deg_in); //180对应0;270对应-180
}

/**
 * @brief 设置下侧航向表数值
 * @param yaw_deg 偏航角
 */
void down_SetAttitude(float yaw_deg)
{
    //right_high、altitude函数对应关系
    set_yaw_deg = (-400.0f / 180.0f) * yaw_deg - 200.0f;
    if (yaw_deg > 180) //180对应-180;220对应-140
    {
        yaw_deg = (-40.0f / -40.0f) * (yaw_deg - 180.0f) - 180.0f;
        set_yaw_deg = (-400.0f / 180.0f) * yaw_deg - 200.0f;
    }
    else if (yaw_deg < -180) //-180对应180;-220对应140
    {
        yaw_deg = (-40.0f / -40.0f) * (yaw_deg + 180.0f) + 180.0f;
        set_yaw_deg = (-400.0f / 180.0f) * yaw_deg - 200.0f;
    }

    char text[16];
    snprintf(text, sizeof(text), "%.1f°", yaw_deg);

    //设置label数据函数
    lv_label_set_text(guider_ui.screen_label_4, text);

    //设置控件x值函数
    lv_obj_set_x(guider_ui.screen_scale_3, set_yaw_deg); //-200对应0;200对应-180
}

/**
 * @brief 设置左侧速度表数值
 * @param speed 速度值
 */
void left_SetNum(float speed)
{
    //left_high、speed函数对应关系
    left_high = (150.0f / 20.0f) * speed - 440.0f;
    if (speed >= 80)
        left_high = (150.0f / 20.0f) * 80 - 440.0f;

    char text[16];
    snprintf(text, sizeof(text), "%.1f", speed);

    //设置label数据函数
    lv_label_set_text(guider_ui.screen_label_1, text);

    //设置控件xy值函数
    //lv_obj_set_pos(obj, x, y);

    //设置控件y值函数
    lv_obj_set_y(guider_ui.screen_scale_1, left_high); //-440对应0;-290对应20
}

/**
 * @brief 设置右侧高度表数值
 * @param altitude 高度值
 */
void right_SetNum(float altitude)
{
    //right_high、altitude函数对应关系
    right_high = (-496.0f / -150.0f) * altitude - 340.0f;
    if (altitude >= 150)
        right_high = (-496.0f / -150.0f) * 150 - 340.0f;
    else if (altitude <= -150)
        right_high = (-496.0f / -150.0f) * (-150) - 340.0f;

    char text[16];
    snprintf(text, sizeof(text), "%.1f", altitude);

    //设置label数据函数
    lv_label_set_text(guider_ui.screen_label_2, text);

    //设置控件y值函数
    lv_obj_set_y(guider_ui.screen_scale_2, right_high); //-340对应0;156对应150
}
