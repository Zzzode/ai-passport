// pet_app/pet_ui.h —— PasPet 原创 LVGL 界面（三键 + 240x320，designs 10）。
// 不复用官方 demo 菜单/页面/ui_pixel 外壳；角色全部由 LVGL 基元程序化绘制，
// 无图片资源；文案纯英文（Montserrat 14/20），规避中文字形问题。
#pragma once

#include "pet_input.h"

// 在持有 bsp_lvgl_lock 时调用：创建唯一主屏（房间 + 状态栏 + 图标坞 + 弹层容器）。
void pet_ui_init(void);

// 输入任务语境调用：内部自行获取/释放 LVGL 锁，回调不得阻塞。
void pet_ui_button(pet_btn_t btn, pet_ev_t ev);
