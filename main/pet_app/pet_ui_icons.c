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

// ---- PV2 卡片缩略图 ----
#define DECL_CARD(nm) \
    static const lv_image_dsc_t DSC_CARD_##nm = ICON_DSC(pet_ui_card_##nm, 28, 28)

DECL_CARD(bottle);
DECL_CARD(meal);
DECL_CARD(snack);
DECL_CARD(gift);
DECL_CARD(emptybag);
DECL_CARD(riceball);
DECL_CARD(biscuit);
DECL_CARD(bento);
DECL_CARD(pudding);
DECL_CARD(cake);
DECL_CARD(crepe);
DECL_CARD(watermelon);
DECL_CARD(ball);
DECL_CARD(musicbox);
DECL_CARD(bubbles);
// PV2 batch2
DECL_CARD(petface);
DECL_CARD(scale);
DECL_CARD(bond);
DECL_CARD(wallet);
DECL_CARD(foodbowl);
DECL_CARD(funface);
DECL_CARD(health);
DECL_CARD(bolt);
DECL_CARD(bandaid);
DECL_CARD(medal);
DECL_CARD(snub);
DECL_CARD(star);
DECL_CARD(speaker);
DECL_CARD(speakeroff);
DECL_CARD(crescent);
// PV2 batch3
DECL_CARD(hilo);
DECL_CARD(rhythm);
DECL_CARD(catch);
DECL_CARD(memory);
DECL_CARD(walk);
DECL_CARD(prefer);

static const lv_image_dsc_t *const CARD_DSC[PET_UI_CARD_COUNT] = {
    [PET_UI_CARD_BOTTLE] = &DSC_CARD_bottle,
    [PET_UI_CARD_MEAL] = &DSC_CARD_meal,
    [PET_UI_CARD_SNACK] = &DSC_CARD_snack,
    [PET_UI_CARD_GIFT] = &DSC_CARD_gift,
    [PET_UI_CARD_EMPTY] = &DSC_CARD_emptybag,
    [PET_UI_CARD_RICEBALL] = &DSC_CARD_riceball,
    [PET_UI_CARD_BISCUIT] = &DSC_CARD_biscuit,
    [PET_UI_CARD_BENTO] = &DSC_CARD_bento,
    [PET_UI_CARD_PUDDING] = &DSC_CARD_pudding,
    [PET_UI_CARD_CAKE] = &DSC_CARD_cake,
    [PET_UI_CARD_CREPE] = &DSC_CARD_crepe,
    [PET_UI_CARD_WATERMELON] = &DSC_CARD_watermelon,
    [PET_UI_CARD_BALL] = &DSC_CARD_ball,
    [PET_UI_CARD_MUSICBOX] = &DSC_CARD_musicbox,
    [PET_UI_CARD_BUBBLES] = &DSC_CARD_bubbles,
    [PET_UI_CARD_PETFACE] = &DSC_CARD_petface,
    [PET_UI_CARD_SCALE] = &DSC_CARD_scale,
    [PET_UI_CARD_BOND] = &DSC_CARD_bond,
    [PET_UI_CARD_WALLET] = &DSC_CARD_wallet,
    [PET_UI_CARD_FOODBOWL] = &DSC_CARD_foodbowl,
    [PET_UI_CARD_FUNFACE] = &DSC_CARD_funface,
    [PET_UI_CARD_HEALTH] = &DSC_CARD_health,
    [PET_UI_CARD_BOLT] = &DSC_CARD_bolt,
    [PET_UI_CARD_BANDAID] = &DSC_CARD_bandaid,
    [PET_UI_CARD_MEDAL] = &DSC_CARD_medal,
    [PET_UI_CARD_SNUB] = &DSC_CARD_snub,
    [PET_UI_CARD_STAR] = &DSC_CARD_star,
    [PET_UI_CARD_SPEAKER] = &DSC_CARD_speaker,
    [PET_UI_CARD_SPEAKER_OFF] = &DSC_CARD_speakeroff,
    [PET_UI_CARD_CRESCENT] = &DSC_CARD_crescent,
    [PET_UI_CARD_HILO] = &DSC_CARD_hilo,
    [PET_UI_CARD_RHYTHM] = &DSC_CARD_rhythm,
    [PET_UI_CARD_CATCH] = &DSC_CARD_catch,
    [PET_UI_CARD_MEMORY] = &DSC_CARD_memory,
    [PET_UI_CARD_WALK] = &DSC_CARD_walk,
    [PET_UI_CARD_PREFER] = &DSC_CARD_prefer,
};

const lv_image_dsc_t *pet_ui_card_dsc(pet_ui_card_t id)
{
    if ((int) id < 0 || (int) id >= PET_UI_CARD_COUNT) {
        return NULL;
    }
    return CARD_DSC[id];
}
