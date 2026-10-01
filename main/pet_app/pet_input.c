#include "pet_input.h"

pet_ui_action_t pet_input_map(pet_btn_t btn, pet_ev_t ev, bool modal_open)
{
    // 长按 OK：弹层内=返回；主屏=快捷关灯/开灯。
    if (btn == PET_BTN_OK && ev == PET_EV_LONG) {
        return modal_open ? PET_UI_ACT_BACK : PET_UI_ACT_LIGHTS;
    }
    // 双击 OK：任何界面都可查看状态页。
    if (btn == PET_BTN_OK && ev == PET_EV_DOUBLE) {
        return PET_UI_ACT_MENU;
    }
    // 只有单击产生移动/确认；PRESS 不触发，避免长按过程中先误触发一次。
    if (ev != PET_EV_CLICK) {
        return PET_UI_ACT_NONE;
    }
    switch (btn) {
    case PET_BTN_UP:
        return PET_UI_ACT_PREV;
    case PET_BTN_DOWN:
        return PET_UI_ACT_NEXT;
    case PET_BTN_OK:
        return PET_UI_ACT_CONFIRM;
    }
    return PET_UI_ACT_NONE;
}
