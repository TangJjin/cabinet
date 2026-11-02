/*
 * 文件用途：接口与类型声明：当前机柜 LVGL 界面：创建页面、处理按钮和输入，读取业务快照并刷新显示。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
#ifndef BSP_CABINET_UI_H
#define BSP_CABINET_UI_H
void bsp_ui_init(void);
void bsp_ui_refresh(void);
void bsp_ui_task(void *argument);
#endif
