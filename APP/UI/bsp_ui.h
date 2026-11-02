/*
 * 文件用途：接口与类型声明：保留的旧姿态仪表界面，当前四个 Keil 目标均未编译；机柜界面使用 bsp_cabinet_ui.c。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef BSP_UI_H
#define BSP_UI_H

#include "bsp_system.h"

void Horizon_Init(lv_obj_t *parent);

void Horizon_SetAttitude(float pitch_deg, float roll_deg); //地平线姿态设置
void left_SetAttitude(float pitch_deg);                    //仪表盘姿态设置（俯仰角）
void top_SetAttitude(float roll_deg);                      //仪表盘姿态设置（滚转角）
void down_SetAttitude(float yaw_deg);                      //刻度表姿态设置（偏航角）

void left_SetNum(float speed);     //速度表数值设置
void right_SetNum(float altitude); //高度表数值设置

#endif
