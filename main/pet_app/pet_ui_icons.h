// PV1 UI 图标包装层：生成表 pet_ui_icons_data（ARGB8888 位图）→ LVGL 静态图片描述符。
#pragma once

#include <stdbool.h>

#include "lvgl.h"
#include "pet_dock.h"

#ifdef __cplusplus
extern "C" {
#endif

// 状态行/顶栏 12px 小图标。
typedef enum {
    PET_UI_SMALL_BOWL = 0,
    PET_UI_SMALL_HEART_GREEN,
    PET_UI_SMALL_HEART_YELLOW,
    PET_UI_SMALL_HEART_EMPTY,
    PET_UI_SMALL_HEART_PINK,
    PET_UI_SMALL_SMILE,
    PET_UI_SMALL_SUN,
    PET_UI_SMALL_MOON,
    PET_UI_SMALL_COUNT,
} pet_ui_small_t;

// 房间贴花。
typedef enum {
    PET_UI_DECO_SUN = 0,
    PET_UI_DECO_PLANT,
    PET_UI_DECO_MOON,
    PET_UI_DECO_COUNT,
} pet_ui_deco_t;

// 坞图标：selected=true 返回 25x25 加粗版，否则 20x20 普通版。
const lv_image_dsc_t *pet_ui_dock_dsc(pet_icon_t icon, bool selected);
const lv_image_dsc_t *pet_ui_small_dsc(pet_ui_small_t id);
const lv_image_dsc_t *pet_ui_deco_dsc(pet_ui_deco_t id);

#ifdef __cplusplus
}
#endif
