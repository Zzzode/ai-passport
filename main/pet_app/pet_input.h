// pet_app/pet_input.h —— 按键→界面动作的纯映射（三键交互宪法，designs 10 §1）。
// 与 BSP/ESP-IDF 无关，可在主机上单测；BSP 事件由 pet_app 转换成本文件的枚举。
#pragma once

#include <stdbool.h>

typedef enum {
    PET_BTN_UP = 0,
    PET_BTN_DOWN,
    PET_BTN_OK,
} pet_btn_t;

typedef enum {
    PET_EV_PRESS = 0,   // 按下瞬间
    PET_EV_CLICK,       // 单击
    PET_EV_DOUBLE,      // 双击
    PET_EV_LONG,        // 长按
} pet_ev_t;

typedef enum {
    PET_UI_ACT_NONE = 0,
    PET_UI_ACT_PREV,      // 光标左移 / 列表上移
    PET_UI_ACT_NEXT,      // 光标右移 / 列表下移
    PET_UI_ACT_CONFIRM,   // 短按 OK：进入 / 确认
    PET_UI_ACT_BACK,      // 长按 OK：返回 / 取消（弹层内）
    PET_UI_ACT_LIGHTS,    // 长按 OK：主屏快捷关灯/开灯
    PET_UI_ACT_MENU,      // 双击 OK：状态页（主屏与弹层通用）
} pet_ui_action_t;

// modal_open=true 表示当前有弹层/二级界面打开，长按 OK 解释为返回。
// 短按（PRESS）不产生动作：避免误触与重复触发。
pet_ui_action_t pet_input_map(pet_btn_t btn, pet_ev_t ev, bool modal_open);
