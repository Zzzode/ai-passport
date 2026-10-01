// PV1 UI 图标包装层：生成表 → LVGL 静态图片描述符（flash 驻留，零堆拷贝）。
#include "pet_ui_icons.h"

#include "pet_ui_icons_data.h"

#include <stddef.h>

// 命名映射：pet_icon_t → assets/pet/ui-icons/dock/<name>.png
//   FEED→feed CLEAN→wc STATUS→status LIGHTS→light MED→med GAME→game
//   PAT→pat SHOP→shop JOB→job MATE→family DEX→album SETTINGS→settings
#define ICON_DSC(sym_, width, height) {                                \
    .header = {                                                     \
        .magic = LV_IMAGE_HEADER_MAGIC,                             \
        .cf = LV_COLOR_FORMAT_ARGB8888,                             \
        .w = (width), .h = (height), .stride = (width) * 4u,        \
    },                                                              \
    .data_size = (width) * (height) * 4u,                           \
    .data = (const uint8_t *) (sym_),                               \
}

#define DECL_DOCK(nm)                                                       \
    static const lv_image_dsc_t dsc_dock_##nm##_20 =                        \
        ICON_DSC(pet_ui_dock_##nm##_20, 20, 20);                            \
    static const lv_image_dsc_t dsc_dock_##nm##_25 =                        \
        ICON_DSC(pet_ui_dock_##nm##_25, 25, 25)

DECL_DOCK(feed);
DECL_DOCK(wc);
DECL_DOCK(status);
DECL_DOCK(light);
DECL_DOCK(med);
DECL_DOCK(game);
DECL_DOCK(pat);
DECL_DOCK(shop);
DECL_DOCK(job);
DECL_DOCK(family);
DECL_DOCK(album);
DECL_DOCK(settings);

static const lv_image_dsc_t *const DOCK_20[PET_ICON_COUNT] = {
    [PET_ICON_FEED] = &dsc_dock_feed_20,
    [PET_ICON_CLEAN] = &dsc_dock_wc_20,
    [PET_ICON_STATUS] = &dsc_dock_status_20,
    [PET_ICON_LIGHTS] = &dsc_dock_light_20,
    [PET_ICON_MED] = &dsc_dock_med_20,
    [PET_ICON_GAME] = &dsc_dock_game_20,
    [PET_ICON_PAT] = &dsc_dock_pat_20,
    [PET_ICON_SHOP] = &dsc_dock_shop_20,
    [PET_ICON_JOB] = &dsc_dock_job_20,
    [PET_ICON_MATE] = &dsc_dock_family_20,
    [PET_ICON_DEX] = &dsc_dock_album_20,
    [PET_ICON_SETTINGS] = &dsc_dock_settings_20,
};

static const lv_image_dsc_t *const DOCK_25[PET_ICON_COUNT] = {
    [PET_ICON_FEED] = &dsc_dock_feed_25,
    [PET_ICON_CLEAN] = &dsc_dock_wc_25,
    [PET_ICON_STATUS] = &dsc_dock_status_25,
    [PET_ICON_LIGHTS] = &dsc_dock_light_25,
    [PET_ICON_MED] = &dsc_dock_med_25,
    [PET_ICON_GAME] = &dsc_dock_game_25,
    [PET_ICON_PAT] = &dsc_dock_pat_25,
    [PET_ICON_SHOP] = &dsc_dock_shop_25,
    [PET_ICON_JOB] = &dsc_dock_job_25,
    [PET_ICON_MATE] = &dsc_dock_family_25,
    [PET_ICON_DEX] = &dsc_dock_album_25,
    [PET_ICON_SETTINGS] = &dsc_dock_settings_25,
};

const lv_image_dsc_t *pet_ui_dock_dsc(pet_icon_t icon, bool selected)
{
    if (icon <= PET_ICON_NONE || icon >= PET_ICON_COUNT) {
        return NULL;
    }
    return selected ? DOCK_25[icon] : DOCK_20[icon];
}

// ---- 12px 小图标 ----
static const lv_image_dsc_t DSC_BOWL = ICON_DSC(pet_ui_small_bowl, 12, 12);
static const lv_image_dsc_t DSC_HG = ICON_DSC(pet_ui_small_hg, 12, 12);
static const lv_image_dsc_t DSC_HY = ICON_DSC(pet_ui_small_hy, 12, 12);
static const lv_image_dsc_t DSC_HE = ICON_DSC(pet_ui_small_he, 12, 12);
static const lv_image_dsc_t DSC_HP = ICON_DSC(pet_ui_small_hp, 12, 12);
static const lv_image_dsc_t DSC_SMILE = ICON_DSC(pet_ui_small_smile, 12, 12);
static const lv_image_dsc_t DSC_SUN = ICON_DSC(pet_ui_small_sun, 12, 12);
static const lv_image_dsc_t DSC_MOON = ICON_DSC(pet_ui_small_moon, 12, 12);

static const lv_image_dsc_t *const SMALL_DSC[PET_UI_SMALL_COUNT] = {
    [PET_UI_SMALL_BOWL] = &DSC_BOWL,
    [PET_UI_SMALL_HEART_GREEN] = &DSC_HG,
    [PET_UI_SMALL_HEART_YELLOW] = &DSC_HY,
    [PET_UI_SMALL_HEART_EMPTY] = &DSC_HE,
    [PET_UI_SMALL_HEART_PINK] = &DSC_HP,
    [PET_UI_SMALL_SMILE] = &DSC_SMILE,
    [PET_UI_SMALL_SUN] = &DSC_SUN,
    [PET_UI_SMALL_MOON] = &DSC_MOON,
};

const lv_image_dsc_t *pet_ui_small_dsc(pet_ui_small_t id)
{
    if ((int) id < 0 || (int) id >= PET_UI_SMALL_COUNT) {
        return NULL;
    }
    return SMALL_DSC[id];
}

// ---- 房间贴花 ----
static const lv_image_dsc_t DSC_SUN30 = ICON_DSC(pet_ui_deco_sun30, 30, 30);
static const lv_image_dsc_t DSC_PLANT20 = ICON_DSC(pet_ui_deco_plant20, 18, 20);
static const lv_image_dsc_t DSC_MOON26 = ICON_DSC(pet_ui_deco_moon26, 26, 26);

static const lv_image_dsc_t *const DECO_DSC[PET_UI_DECO_COUNT] = {
    [PET_UI_DECO_SUN] = &DSC_SUN30,
    [PET_UI_DECO_PLANT] = &DSC_PLANT20,
    [PET_UI_DECO_MOON] = &DSC_MOON26,
};

const lv_image_dsc_t *pet_ui_deco_dsc(pet_ui_deco_t id)
{
    if ((int) id < 0 || (int) id >= PET_UI_DECO_COUNT) {
        return NULL;
    }
    return DECO_DSC[id];
}
