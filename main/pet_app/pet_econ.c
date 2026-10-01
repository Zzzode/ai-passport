// pet_app/pet_econ.c —— 见 pet_econ.h。纯 C，无 ESP-IDF/LVGL 依赖。
#include "pet_econ.h"

#include <stddef.h>
#include <string.h>

#include "pt_config.h"
#include "pt_rng.h"

// 商品表（价格区间对齐 designs 06 §3.2 / 12 数值表：正餐 5–15、零食 8–20、
// 特色 15–40、玩具 100–500）。v0.1 全部 G 币标价。

static const pt_item_def_t ITEMS[PT_ITEM_COUNT] = {
    [PT_ITEM_RICEBALL]   = { "Riceball", 5, PT_ITEM_KIND_FOOD, PT_FOOD_MEAL,
                             PT_FOOD_TAG_MEAL,
                             false, PT_STAGE_CHILD, true },
    [PT_ITEM_BISCUIT]    = { "Biscuit", 8, PT_ITEM_KIND_FOOD, PT_FOOD_SNACK,
                             PT_FOOD_TAG_SWEET,
                             false, PT_STAGE_CHILD, true },
    [PT_ITEM_BENTO]      = { "Bento", 12, PT_ITEM_KIND_FOOD, PT_FOOD_MEAL,
                             PT_FOOD_TAG_MEAL,
                             false, PT_STAGE_TEEN, false },
    [PT_ITEM_PUDDING]    = { "Pudding", 14, PT_ITEM_KIND_FOOD, PT_FOOD_SNACK,
                             PT_FOOD_TAG_SWEET,
                             false, PT_STAGE_TEEN, false },
    [PT_ITEM_CAKE]       = { "Cake", 20, PT_ITEM_KIND_FOOD, PT_FOOD_SNACK,
                             PT_FOOD_TAG_SWEET,
                             false, PT_STAGE_TEEN, false },
    [PT_ITEM_CREPE]      = { "Crepe", 25, PT_ITEM_KIND_FOOD, PT_FOOD_SNACK,
                             PT_FOOD_TAG_SWEET,
                             true, PT_STAGE_TEEN, false },
    [PT_ITEM_WATERMELON] = { "Watermelon", 30, PT_ITEM_KIND_FOOD, PT_FOOD_MEAL,
                             PT_FOOD_TAG_VEG,
                             true, PT_STAGE_TEEN, false },
    [PT_ITEM_BALL]       = { "Ball", 100, PT_ITEM_KIND_TOY, 0, 0,
                             false, PT_STAGE_CHILD, false },
    [PT_ITEM_MUSICBOX]   = { "Music Box", 300, PT_ITEM_KIND_TOY, 0, 0,
                             false, PT_STAGE_CHILD, false },
    [PT_ITEM_BUBBLES]    = { "Bubbles", 500, PT_ITEM_KIND_TOY, 0, 0,
                             false, PT_STAGE_CHILD, false },
};

const pt_item_def_t *pt_item_def(pt_item_t id)
{
    if (id <= PT_ITEM_NONE || id >= PT_ITEM_COUNT) {
        return NULL;
    }
    return &ITEMS[id];
}

bool pt_item_is_toy(pt_item_t id)
{
    const pt_item_def_t *d = pt_item_def(id);
    return d != NULL && d->kind == PT_ITEM_KIND_TOY;
}

pt_intent_kind_t pt_item_eat_intent(pt_item_t id)
{
    const pt_item_def_t *d = pt_item_def(id);
    if (d == NULL || d->kind != PT_ITEM_KIND_FOOD) {
        return 0;
    }
    return d->food_class == PT_FOOD_MEAL ? PT_INTENT_FEED_MEAL
                                         : PT_INTENT_FEED_SNACK;
}

void pt_econ_init(pt_econ_t *e, uint32_t shelf_seed)
{
    // 整体清零：库存槽结构带尾填充，存档按字段显式编解码，memset 保证往返一致。
    memset(e, 0, sizeof(*e));
    for (size_t i = 0; i < sizeof(e->inv) / sizeof(e->inv[0]); i += 1) {
        e->inv[i].item = PT_ITEM_NONE;
        e->inv[i].qty = 0;
    }
    e->checkin_day = -1;
    e->shelf_seed = shelf_seed;
}

void pt_econ_on_day(pt_econ_t *e)
{
    for (int i = 0; i < PT_ITEM_COUNT; i += 1) {
        e->toy_uses[i] = 0;
    }
    e->game_pays_today = 0;
}

uint16_t pt_econ_game_payout(pt_econ_t *e, uint8_t grade)
{
    uint16_t gain;
    if (e->game_pays_today < PT_ECON_GAME_FULL_N) {
        gain = grade == PT_GAME_GRADE_PERFECT ? PT_ECON_GAME_PAY_PERFECT
            : grade == PT_GAME_GRADE_GREAT ? PT_ECON_GAME_PAY_GREAT
                                           : PT_ECON_GAME_PAY_GOOD;
    } else {
        gain = PT_ECON_GAME_FLOOR;
    }
    e->game_pays_today = (uint8_t) (e->game_pays_today + 1);
    // S5：游戏奖金同属劳动收入，吃 >10,000G 软边际。
    return pt_econ_credit_income(e, gain);
}

uint16_t pt_econ_add_coins(pt_econ_t *e, uint16_t amount)
{
    uint32_t room = PT_ECON_COIN_CAP - e->coins;
    uint16_t got = amount > room ? (uint16_t) room : amount;
    e->coins = (uint32_t) (e->coins + got);
    return got;
}

uint16_t pt_econ_credit_income(pt_econ_t *e, uint16_t amount)
{
    uint16_t effective = amount;
    if (e->coins > PT_ECON_SOFT_TAX_AT) {
        effective = (uint16_t) ((uint32_t) amount
                                * (100u - PT_ECON_SOFT_TAX_PCT) / 100u);
    }
    return pt_econ_add_coins(e, effective);
}

bool pt_econ_is_stipend_day(int32_t day_id)
{
    int32_t m = day_id % 30;
    if (m < 0) {
        m += 30;
    }
    return m + 1 == PT_ECON_STIPEND_DAY;
}

uint32_t pt_econ_item_price_now(uint16_t price, int32_t day_id)
{
    // 食物/玩具非家具：家具专属 9 折日不参与，只吃 5/15/25 全场 8 折。
    uint8_t pct = pt_econ_sale_pct(day_id, false);
    return (uint32_t) price * (100u - pct) / 100u;
}

bool pt_econ_can_checkin(const pt_econ_t *e, int32_t day_id)
{
    return e->checkin_day != day_id;
}

bool pt_econ_checkin(pt_econ_t *e, int32_t day_id, pt_checkin_result_t *out)
{
    if (out != NULL) {
        out->coins_gained = 0;
        out->shells_gained = 0;
        out->week_bonus = false;
        out->streak_now = e->streak;
    }
    if (!pt_econ_can_checkin(e, day_id)) {
        return false;
    }
    if (e->checkin_day == day_id - 1) {
        e->streak = (uint16_t) (e->streak + 1);
    } else {
        e->streak = 1;
    }

    uint32_t gain = PT_ECON_CHECKIN_COINS;
    uint16_t shell = 0;
    bool week = false;
    if (e->streak >= 7) {
        gain = PT_ECON_CHECKIN_DAY7;
        shell = PT_ECON_CHECKIN_DAY7_SHELL;
        week = true;
        e->streak = 0;
    }
    e->checkin_day = day_id;
    uint32_t coins = e->coins + gain;
    e->coins = coins > PT_ECON_COIN_CAP ? PT_ECON_COIN_CAP : coins;
    uint32_t shells = (uint32_t) e->shells + shell;
    e->shells = (uint16_t) (shells > PT_ECON_SHELL_CAP ? PT_ECON_SHELL_CAP
                                                       : shells);
    if (out != NULL) {
        out->coins_gained = (uint16_t) gain;
        out->shells_gained = shell;
        out->week_bonus = week;
        out->streak_now = e->streak;
    }
    return true;
}

// 把绝对分钟映射到 (day, 货架会话槽 0/1/2)：08–14、14–20、20–次日08。
static void shop_session(int32_t minute, int32_t *day_out, uint32_t *slot_out)
{
    int32_t day = minute / 1440;
    int32_t mod = minute - day * 1440;
    if (day < 0) {
        day = 0;
    }
    if (mod < 0) {
        mod += 1440;
    }
    uint32_t slot;
    if (mod >= PT_ECON_REFRESH_3_MIN) {
        slot = 2;
    } else if (mod >= PT_ECON_REFRESH_2_MIN) {
        slot = 1;
    } else if (mod >= PT_ECON_REFRESH_1_MIN) {
        slot = 0;
    } else {
        // 凌晨仍挂前一天 20:00 的晚间货架。
        slot = 2;
        day -= 1;
        if (day < 0) {
            day = 0;
        }
    }
    *day_out = day;
    *slot_out = slot;
}

uint8_t pt_econ_shop_build(const pt_econ_t *e, pt_stage_t stage,
                           int32_t minute, uint16_t *out, uint8_t cap)
{
    if (out == NULL || cap < 2 || stage < PT_STAGE_CHILD) {
        return 0;
    }
    uint8_t n = 0;
    out[n++] = PT_ITEM_RICEBALL;
    out[n++] = PT_ITEM_BISCUIT;

    if (stage == PT_STAGE_CHILD) {
        // 幼儿期：3 个固定玩具，无随机货架（06 §7）。
        static const pt_item_t fixed_toys[] = {
            PT_ITEM_BALL, PT_ITEM_MUSICBOX, PT_ITEM_BUBBLES,
        };
        for (uint8_t i = 0; i < 3 && n < cap; i += 1) {
            out[n++] = fixed_toys[i];
        }
        return n;
    }

    // 少年起：从随机池无放回抽 6 个（Fisher-Yates 前 6 步），种子按
    // “存档 × 日 × 会话槽”盐派生，同会话恒定、跨刷新变化。
    static const pt_item_t pool[] = {
        PT_ITEM_BENTO, PT_ITEM_PUDDING, PT_ITEM_CAKE, PT_ITEM_CREPE,
        PT_ITEM_WATERMELON, PT_ITEM_BALL, PT_ITEM_MUSICBOX, PT_ITEM_BUBBLES,
    };
    uint8_t cand[sizeof(pool) / sizeof(pool[0])];
    for (uint8_t i = 0; i < sizeof(cand); i += 1) {
        cand[i] = (uint8_t) pool[i];
    }
    int32_t day;
    uint32_t slot;
    shop_session(minute, &day, &slot);
    uint32_t rng = pt_salted(e->shelf_seed, (uint32_t) day, slot);

    uint8_t picks = PT_ECON_SHOP_RANDOM;
    if (picks > (uint8_t) (cap - n)) {
        picks = (uint8_t) (cap - n);
    }
    if (picks > (uint8_t) sizeof(cand)) {
        picks = (uint8_t) sizeof(cand);
    }
    for (uint8_t i = 0; i < picks; i += 1) {
        uint8_t rest = (uint8_t) (sizeof(cand) - i);
        uint8_t j = (uint8_t) (i + pt_rng_below(&rng, rest));
        uint8_t tmp = cand[i];
        cand[i] = cand[j];
        cand[j] = tmp;
        out[n++] = cand[i];
    }
    return n;
}

static int8_t find_slot(const pt_econ_t *e, pt_item_t item)
{
    for (int8_t i = 0; i < PT_ECON_INV_SLOTS; i += 1) {
        if (e->inv[i].item == (uint16_t) item) {
            return i;
        }
    }
    return -1;
}

static int8_t find_empty(const pt_econ_t *e)
{
    for (int8_t i = 0; i < PT_ECON_INV_SLOTS; i += 1) {
        if (e->inv[i].item == PT_ITEM_NONE || e->inv[i].qty == 0) {
            return i;
        }
    }
    return -1;
}

pt_econ_rv_t pt_econ_add(pt_econ_t *e, pt_item_t item, uint8_t qty)
{
    const pt_item_def_t *d = pt_item_def(item);
    if (d == NULL || qty == 0) {
        return PT_ECON_BAD_ITEM;
    }
    int8_t slot = find_slot(e, item);
    if (slot < 0) {
        slot = find_empty(e);
        if (slot < 0) {
            return PT_ECON_INV_FULL;
        }
        e->inv[slot].item = (uint16_t) item;
        e->inv[slot].qty = qty;
    } else {
        uint16_t sum = (uint16_t) e->inv[slot].qty + qty;
        if (sum > 255) {
            sum = 255;
        }
        e->inv[slot].qty = (uint8_t) sum;
    }
    return PT_ECON_OK;
}

pt_econ_rv_t pt_econ_buy(pt_econ_t *e, pt_stage_t stage, int32_t minute,
                         pt_item_t item)
{
    const pt_item_def_t *d = pt_item_def(item);
    if (d == NULL) {
        return PT_ECON_BAD_ITEM;
    }
    if (stage == PT_STAGE_DEAD || stage < d->min_stage) {
        return PT_ECON_LOCKED;
    }
    // 只能买当前货架上的东西。
    uint16_t shelf[2 + PT_ECON_SHOP_RANDOM];
    uint8_t n = pt_econ_shop_build(e, stage, minute, shelf, (uint8_t) sizeof(shelf)
                                                               / sizeof(shelf[0]));
    bool on_shelf = false;
    for (uint8_t i = 0; i < n; i += 1) {
        if (shelf[i] == (uint16_t) item) {
            on_shelf = true;
            break;
        }
    }
    if (!on_shelf) {
        return PT_ECON_NOT_ON_SHELF;
    }
    if (d->kind == PT_ITEM_KIND_TOY && find_slot(e, item) >= 0) {
        return PT_ECON_OWNED;
    }
    // S5：货架参与折扣日（与引擎同一清醒日界：06:00 = day_id 边界）。
    int32_t q = (minute - PT_CFG_DAY_BOUNDARY_MIN) / 1440;
    if ((minute - PT_CFG_DAY_BOUNDARY_MIN) < 0
        && (minute - PT_CFG_DAY_BOUNDARY_MIN) % 1440 != 0) {
        q -= 1;   // 负分钟向下取整
    }
    uint32_t price = pt_econ_item_price_now(d->price, (int32_t) q);
    if (e->coins < price) {
        return PT_ECON_NO_MONEY;
    }
    pt_econ_rv_t rv = pt_econ_add(e, item, 1);
    if (rv != PT_ECON_OK) {
        return rv;
    }
    e->coins -= price;
    return PT_ECON_OK;
}

uint8_t pt_econ_count(const pt_econ_t *e, pt_item_t item)
{
    int8_t slot = find_slot(e, item);
    return slot < 0 ? 0 : e->inv[slot].qty;
}

pt_econ_rv_t pt_econ_take_food(pt_econ_t *e, pt_item_t item)
{
    const pt_item_def_t *d = pt_item_def(item);
    if (d == NULL || d->kind != PT_ITEM_KIND_FOOD) {
        return PT_ECON_BAD_ITEM;
    }
    int8_t slot = find_slot(e, item);
    if (slot < 0 || e->inv[slot].qty == 0) {
        return PT_ECON_NO_STOCK;
    }
    e->inv[slot].qty -= 1;
    if (e->inv[slot].qty == 0) {
        e->inv[slot].item = PT_ITEM_NONE;
    }
    return PT_ECON_OK;
}

pt_econ_rv_t pt_econ_use_toy(pt_econ_t *e, pt_item_t item)
{
    const pt_item_def_t *d = pt_item_def(item);
    if (d == NULL || d->kind != PT_ITEM_KIND_TOY) {
        return PT_ECON_BAD_ITEM;
    }
    if (find_slot(e, item) < 0) {
        return PT_ECON_NO_STOCK;
    }
    if (e->toy_uses[item] >= PT_ECON_TOY_USES_DAY) {
        return PT_ECON_USES_EXHAUSTED;
    }
    e->toy_uses[item] += 1;
    return PT_ECON_OK;
}

void pt_econ_refund_toy_use(pt_econ_t *e, pt_item_t item)
{
    if (item > PT_ITEM_NONE && item < PT_ITEM_COUNT
        && e->toy_uses[item] > 0) {
        e->toy_uses[item] -= 1;
    }
}

bool pt_econ_spend_coins(pt_econ_t *e, uint32_t amount)
{
    if (e->coins < amount) {
        return false;
    }
    e->coins -= amount;
    return true;
}

bool pt_econ_spend_shells(pt_econ_t *e, uint16_t amount)
{
    if (e->shells < amount) {
        return false;
    }
    e->shells = (uint16_t) (e->shells - amount);
    return true;
}

// P2-S3b：成家奖励等通用贝壳券发放（封顶 PT_ECON_SHELL_CAP）。
void pt_econ_grant_shells(pt_econ_t *e, uint16_t amount)
{
    uint32_t v = (uint32_t) e->shells + amount;
    e->shells = (uint16_t) (v > PT_ECON_SHELL_CAP ? PT_ECON_SHELL_CAP : v);
}

uint8_t pt_econ_sale_pct(int32_t day_id, bool furniture)
{
    int32_t m = day_id % 30;
    if (m < 0) {
        m += 30;
    }
    int32_t dom = m + 1;   // 1..30
    if (dom == 5 || dom == 15 || dom == 25) {
        return 20;
    }
    if (furniture && (dom == 10 || dom == 20 || dom == 30)) {
        return 10;
    }
    return 0;
}
