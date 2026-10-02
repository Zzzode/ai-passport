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

// PV2 卡片缩略图（28x28）：FOOD 直喂 + 签到/空态 + 商店物品 +
// PV2 batch2 状态/声音仪表图标。
typedef enum {
    PET_UI_CARD_BOTTLE = 0,
    PET_UI_CARD_MEAL,
    PET_UI_CARD_SNACK,
    PET_UI_CARD_GIFT,
    PET_UI_CARD_EMPTY,
    PET_UI_CARD_RICEBALL,
    PET_UI_CARD_BISCUIT,
    PET_UI_CARD_BENTO,
    PET_UI_CARD_PUDDING,
    PET_UI_CARD_CAKE,
    PET_UI_CARD_CREPE,
    PET_UI_CARD_WATERMELON,
    PET_UI_CARD_BALL,
    PET_UI_CARD_MUSICBOX,
    PET_UI_CARD_BUBBLES,
    // ---- PV2 batch2：STATUS / SOUND ----
    PET_UI_CARD_PETFACE,
    PET_UI_CARD_SCALE,
    PET_UI_CARD_BOND,
    PET_UI_CARD_WALLET,
    PET_UI_CARD_FOODBOWL,
    PET_UI_CARD_FUNFACE,
    PET_UI_CARD_HEALTH,
    PET_UI_CARD_BOLT,
    PET_UI_CARD_BANDAID,
    PET_UI_CARD_MEDAL,
    PET_UI_CARD_SNUB,
    PET_UI_CARD_STAR,
    PET_UI_CARD_SPEAKER,
    PET_UI_CARD_SPEAKER_OFF,
    PET_UI_CARD_CRESCENT,
    // ---- PV2 batch3：GAMES 选择器（G1–G6） ----
    PET_UI_CARD_HILO,
    PET_UI_CARD_RHYTHM,
    PET_UI_CARD_CATCH,
    PET_UI_CARD_MEMORY,
    PET_UI_CARD_WALK,
    PET_UI_CARD_PREFER,
    PET_UI_CARD_COUNT,
} pet_ui_card_t;

// 坞图标：selected=true 返回 25x25 加粗版，否则 20x20 普通版。
const lv_image_dsc_t *pet_ui_dock_dsc(pet_icon_t icon, bool selected);
const lv_image_dsc_t *pet_ui_small_dsc(pet_ui_small_t id);
const lv_image_dsc_t *pet_ui_deco_dsc(pet_ui_deco_t id);
const lv_image_dsc_t *pet_ui_card_dsc(pet_ui_card_t id);

#ifdef __cplusplus
}
#endif
