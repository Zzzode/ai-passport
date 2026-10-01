// pet_app/pet_decor.c —— 见 pet_decor.h。纯 C，无 ESP-IDF/LVGL 依赖。
#include "pet_decor.h"

#include <stddef.h>

// 服装表：每槽至少 1 件 G 币款 + 帽子槽多给一件贝壳款（06 §3.2 服装 200–1500G）。
static const pt_outfit_def_t OUTFITS[PT_OUTFIT_COUNT] = {
    [PT_OUTFIT_CAP]      = { "Cap",       PT_SLOT_HAT, 200, 0, PT_STAGE_TEEN },
    [PT_OUTFIT_TOPHAT]   = { "Top Hat",   PT_SLOT_HAT, 600, 0, PT_STAGE_TEEN },
    [PT_OUTFIT_STARHAT]  = { "Star Hat",  PT_SLOT_HAT, 0, 8, PT_STAGE_TEEN },
    [PT_OUTFIT_GLASSES]  = { "Glasses",   PT_SLOT_FACE, 250, 0, PT_STAGE_TEEN },
    [PT_OUTFIT_SCARF]    = { "Scarf",     PT_SLOT_COLLAR, 300, 0, PT_STAGE_TEEN },
    [PT_OUTFIT_BALLOON]  = { "Balloon",   PT_SLOT_HELD, 350, 0, PT_STAGE_TEEN },
};

static const pt_furn_def_t FURN[PT_FURN_COUNT] = {
    [PT_FURN_PLANT]   = { "Plant", 300, PT_STAGE_TEEN },
    [PT_FURN_GYM]     = { "Gym Corner", 800, PT_STAGE_TEEN },
    [PT_FURN_KITCHEN] = { "Kitchen Set", 1200, PT_STAGE_TEEN },
};

static const pt_theme_def_t THEMES[PT_THEME_COUNT] = {
    [PT_THEME_COZY]   = { "Cozy", 0, 0, 0xE6F6D8 },
    [PT_THEME_SKY]    = { "Sky", 400, 0, 0xC7E3F7 },
    [PT_THEME_BERRY]  = { "Berry", 400, 0, 0xF6D7E4 },
    [PT_THEME_STARRY] = { "Starry", 0, 12, 0x242046 },
};

void pt_decor_init(pt_decor_t *d)
{
    for (uint8_t i = 0; i < PT_SLOT_COUNT; i += 1) {
        d->worn[i] = PT_OUTFIT_NONE;
    }
    d->outfits = 0;
    d->furniture = 0;
    d->placed = 0;
    d->themes = (uint8_t) (1u << PT_THEME_COZY);
    d->theme = PT_THEME_COZY;
}

const pt_outfit_def_t *pt_outfit_def(pt_outfit_t id)
{
    if (id <= PT_OUTFIT_NONE || id >= PT_OUTFIT_COUNT) {
        return NULL;
    }
    return &OUTFITS[id];
}

const pt_furn_def_t *pt_furn_def(pt_furn_t id)
{
    if (id >= PT_FURN_COUNT) {
        return NULL;
    }
    return &FURN[id];
}

const pt_theme_def_t *pt_theme_def(pt_theme_t id)
{
    if (id >= PT_THEME_COUNT) {
        return NULL;
    }
    return &THEMES[id];
}

bool pt_decor_owns_outfit(const pt_decor_t *d, pt_outfit_t id)
{
    return id > PT_OUTFIT_NONE && id < PT_OUTFIT_COUNT
           && (d->outfits & (uint8_t) (1u << (id - 1))) != 0;
}

bool pt_decor_owns_furn(const pt_decor_t *d, pt_furn_t id)
{
    return id < PT_FURN_COUNT && (d->furniture & (uint8_t) (1u << id)) != 0;
}

bool pt_decor_owns_theme(const pt_decor_t *d, pt_theme_t id)
{
    return id < PT_THEME_COUNT && (d->themes & (uint8_t) (1u << id)) != 0;
}

bool pt_decor_is_placed(const pt_decor_t *d, pt_furn_t id)
{
    return id < PT_FURN_COUNT && (d->placed & (uint8_t) (1u << id)) != 0;
}

pt_outfit_t pt_decor_worn(const pt_decor_t *d, pt_slot_t slot)
{
    if (slot >= PT_SLOT_COUNT) {
        return PT_OUTFIT_NONE;
    }
    return (pt_outfit_t) d->worn[slot];
}

uint32_t pt_decor_outfit_price_g(const pt_outfit_def_t *def_, int32_t day_id)
{
    if (def_ == NULL || def_->shells > 0) {
        return 0;
    }
    uint8_t pct = pt_econ_sale_pct(day_id, false);
    return (uint32_t) def_->price * (100u - pct) / 100u;
}

uint32_t pt_decor_furn_price_g(const pt_furn_def_t *def_, int32_t day_id)
{
    if (def_ == NULL) {
        return 0;
    }
    uint8_t pct = pt_econ_sale_pct(day_id, true);
    return (uint32_t) def_->price * (100u - pct) / 100u;
}

pt_decor_rv_t pt_decor_buy_outfit(pt_decor_t *d, pt_econ_t *e,
                                  pt_stage_t stage, int32_t day_id,
                                  pt_outfit_t id)
{
    const pt_outfit_def_t *def_ = pt_outfit_def(id);
    if (def_ == NULL) {
        return PT_DECOR_BAD_ID;
    }
    if (stage == PT_STAGE_DEAD || stage < def_->min_stage) {
        return PT_DECOR_LOCKED;
    }
    if (pt_decor_owns_outfit(d, id)) {
        return PT_DECOR_OWNED;
    }
    if (def_->shells > 0) {
        if (!pt_econ_spend_shells(e, def_->shells)) {
            return PT_DECOR_NO_MONEY;
        }
    } else {
        if (!pt_econ_spend_coins(e, pt_decor_outfit_price_g(def_, day_id))) {
            return PT_DECOR_NO_MONEY;
        }
    }
    d->outfits |= (uint8_t) (1u << (id - 1));
    return PT_DECOR_OK;
}

pt_decor_rv_t pt_decor_buy_furn(pt_decor_t *d, pt_econ_t *e,
                                pt_stage_t stage, int32_t day_id,
                                pt_furn_t id)
{
    const pt_furn_def_t *def_ = pt_furn_def(id);
    if (def_ == NULL) {
        return PT_DECOR_BAD_ID;
    }
    if (stage == PT_STAGE_DEAD || stage < def_->min_stage) {
        return PT_DECOR_LOCKED;
    }
    if (pt_decor_owns_furn(d, id)) {
        return PT_DECOR_OWNED;
    }
    if (!pt_econ_spend_coins(e, pt_decor_furn_price_g(def_, day_id))) {
        return PT_DECOR_NO_MONEY;
    }
    d->furniture |= (uint8_t) (1u << id);
    return PT_DECOR_OK;
}

pt_decor_rv_t pt_decor_buy_theme(pt_decor_t *d, pt_econ_t *e, pt_theme_t id)
{
    const pt_theme_def_t *def_ = pt_theme_def(id);
    if (def_ == NULL) {
        return PT_DECOR_BAD_ID;
    }
    if (pt_decor_owns_theme(d, id)) {
        return PT_DECOR_OWNED;
    }
    if (def_->shells > 0) {
        if (!pt_econ_spend_shells(e, def_->shells)) {
            return PT_DECOR_NO_MONEY;
        }
    } else {
        if (!pt_econ_spend_coins(e, def_->price)) {
            return PT_DECOR_NO_MONEY;
        }
    }
    d->themes |= (uint8_t) (1u << id);
    return PT_DECOR_OK;
}

pt_decor_rv_t pt_decor_equip(pt_decor_t *d, pt_outfit_t id)
{
    if (id == PT_OUTFIT_NONE) {
        // 由 UI 指定槽位的场景走 pt_decor_unequip_slot。
        // 避免不知道脱哪一件。
        return PT_DECOR_BAD_ID;
    }
    const pt_outfit_def_t *def_ = pt_outfit_def(id);
    if (def_ == NULL) {
        return PT_DECOR_BAD_ID;
    }
    if (!pt_decor_owns_outfit(d, id)) {
        return PT_DECOR_NOT_OWNED;
    }
    // 同槽再点一次 = 脱下。
    if (d->worn[def_->slot] == id) {
        d->worn[def_->slot] = PT_OUTFIT_NONE;
        return PT_DECOR_OK;
    }
    d->worn[def_->slot] = (uint8_t) id;
    return PT_DECOR_OK;
}

pt_decor_rv_t pt_decor_unequip_slot(pt_decor_t *d, pt_slot_t slot)
{
    if (slot >= PT_SLOT_COUNT) {
        return PT_DECOR_BAD_ID;
    }
    d->worn[slot] = PT_OUTFIT_NONE;
    return PT_DECOR_OK;
}

static uint8_t popcount8(uint8_t v)
{
    uint8_t n = 0;
    while (v != 0) {
        v = (uint8_t) (v & (v - 1));
        n += 1;
    }
    return n;
}

pt_decor_rv_t pt_decor_place(pt_decor_t *d, pt_furn_t id)
{
    if (id >= PT_FURN_COUNT) {
        return PT_DECOR_BAD_ID;
    }
    if (!pt_decor_owns_furn(d, id)) {
        return PT_DECOR_NOT_OWNED;
    }
    uint8_t bit = (uint8_t) (1u << id);
    if ((d->placed & bit) != 0) {
        return PT_DECOR_OK;   // 幂等
    }
    if (popcount8(d->placed) >= PT_DECOR_PLACED_MAX) {
        return PT_DECOR_PLACE_CAP;
    }
    d->placed |= bit;
    return PT_DECOR_OK;
}

pt_decor_rv_t pt_decor_unplace(pt_decor_t *d, pt_furn_t id)
{
    if (id >= PT_FURN_COUNT) {
        return PT_DECOR_BAD_ID;
    }
    d->placed &= (uint8_t) ~(1u << id);
    return PT_DECOR_OK;
}

pt_decor_rv_t pt_decor_set_theme(pt_decor_t *d, pt_theme_t id)
{
    if (id >= PT_THEME_COUNT) {
        return PT_DECOR_BAD_ID;
    }
    if (!pt_decor_owns_theme(d, id)) {
        return PT_DECOR_NOT_OWNED;
    }
    d->theme = (uint8_t) id;
    return PT_DECOR_OK;
}

uint8_t pt_decor_meal_bonus(const pt_decor_t *d)
{
    return pt_decor_is_placed(d, PT_FURN_KITCHEN) ? PT_DECOR_MEAL_BONUS : 0;
}

uint8_t pt_decor_happy_pct(const pt_decor_t *d)
{
    return pt_decor_is_placed(d, PT_FURN_PLANT) ? PT_DECOR_PLANT_HAPPY_PCT : 0;
}

uint8_t pt_decor_energy_pct(const pt_decor_t *d)
{
    return pt_decor_is_placed(d, PT_FURN_GYM) ? PT_DECOR_GYM_ENERGY_PCT : 100;
}

uint8_t pt_decor_score_pct(const pt_decor_t *d)
{
    return pt_decor_is_placed(d, PT_FURN_GYM) ? PT_DECOR_GYM_SCORE_PCT : 0;
}

bool pt_decor_validate(const pt_decor_t *d)
{
    // 合法服装所有权位：bit0..bit(COUNT-2)。
    uint8_t outfit_bits = (uint8_t) ((1u << (PT_OUTFIT_COUNT - 1)) - 1u);
    if (d->outfits & (uint8_t) ~outfit_bits) {
        return false;
    }
    uint8_t furn_bits = (uint8_t) ((1u << PT_FURN_COUNT) - 1u);
    if (d->furniture & (uint8_t) ~furn_bits) {
        return false;
    }
    if (d->placed & (uint8_t) ~d->furniture) {
        return false;   // 摆放了未拥有的家具
    }
    if (popcount8(d->placed) > PT_DECOR_PLACED_MAX) {
        return false;
    }
    uint8_t theme_bits = (uint8_t) ((1u << PT_THEME_COUNT) - 1u);
    if (d->themes & (uint8_t) ~theme_bits) {
        return false;
    }
    if (d->theme >= PT_THEME_COUNT
        || (d->themes & (uint8_t) (1u << d->theme)) == 0) {
        return false;
    }
    for (uint8_t i = 0; i < PT_SLOT_COUNT; i += 1) {
        uint8_t w = d->worn[i];
        if (w >= PT_OUTFIT_COUNT) {
            return false;
        }
        if (w != PT_OUTFIT_NONE) {
            const pt_outfit_def_t *def_ = pt_outfit_def((pt_outfit_t) w);
            if (def_ == NULL || def_->slot != i
                || !pt_decor_owns_outfit(d, (pt_outfit_t) w)) {
                return false;
            }
        }
    }
    return true;
}
