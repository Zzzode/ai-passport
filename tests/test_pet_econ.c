// 主机测试：P1 经济底座——目录合法性、签到、货架三刷、购买/库存/玩具次数
// （designs/Tomagotchi/06 §3、§4、§8）。纯逻辑，无 ESP-IDF 依赖。
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "pet_econ.h"

#define MIN0(day, hh, mm) ((day) * 1440 + (hh) * 60 + (mm))

static const pt_item_t TEEN_POOL[] = {
    PT_ITEM_BENTO, PT_ITEM_PUDDING, PT_ITEM_CAKE, PT_ITEM_CREPE,
    PT_ITEM_WATERMELON, PT_ITEM_BALL, PT_ITEM_MUSICBOX, PT_ITEM_BUBBLES,
};

static void test_catalog_is_legal(void)
{
    // 价格带对齐 designs 12：正餐 5–15、零食 8–20、特色 15–40、玩具 100–500。
    for (int id = 1; id < PT_ITEM_COUNT; id += 1) {
        const pt_item_def_t *d = pt_item_def((pt_item_t) id);
        assert(d != NULL && d->name != NULL);
        assert(d->price >= 5 && d->price <= 500);
        assert(d->min_stage >= PT_STAGE_CHILD && d->min_stage <= PT_STAGE_TEEN);
    }
    assert(pt_item_def(PT_ITEM_NONE) == NULL);
    assert(pt_item_def(PT_ITEM_COUNT) == NULL);

    assert(pt_item_eat_intent(PT_ITEM_RICEBALL) == PT_INTENT_FEED_MEAL);
    assert(pt_item_eat_intent(PT_ITEM_WATERMELON) == PT_INTENT_FEED_MEAL);
    assert(pt_item_eat_intent(PT_ITEM_BISCUIT) == PT_INTENT_FEED_SNACK);
    assert(pt_item_eat_intent(PT_ITEM_CREPE) == PT_INTENT_FEED_SNACK);
    assert(pt_item_eat_intent(PT_ITEM_BALL) == 0);
    assert(pt_item_is_toy(PT_ITEM_BALL));
    assert(!pt_item_is_toy(PT_ITEM_CAKE));

    // 常备栏恰好两个，幼儿期可买。
    uint8_t staples = 0;
    for (int id = 1; id < PT_ITEM_COUNT; id += 1) {
        const pt_item_def_t *d = pt_item_def((pt_item_t) id);
        if (d->staple) {
            staples += 1;
            assert(d->min_stage == PT_STAGE_CHILD);
        }
    }
    assert(staples == 2);
}

static void test_checkin_streak_and_caps(void)
{
    pt_econ_t e;
    pt_econ_init(&e, 42u);
    assert(pt_econ_can_checkin(&e, 5));

    pt_checkin_result_t r;
    assert(pt_econ_checkin(&e, 10, &r));
    assert(r.coins_gained == 20 && !r.week_bonus);
    assert(e.coins == 20 && e.streak == 1);
    assert(!pt_econ_checkin(&e, 10, &r));   // 当天不可重复

    // 连续 6 天普通签到后，第 7 天大奖。
    for (int day = 11; day <= 15; day += 1) {
        assert(pt_econ_checkin(&e, day, &r));
        assert(r.coins_gained == 20);
    }
    assert(e.streak == 6);
    assert(pt_econ_checkin(&e, 16, &r));
    assert(r.week_bonus && r.coins_gained == 100 && r.shells_gained == 1);
    assert(e.shells == 1 && e.streak == 0);

    // 断签一天：连签从 1 重新计。
    assert(pt_econ_checkin(&e, 18, &r));
    assert(e.streak == 1 && r.coins_gained == 20);

    // 上限钳制：余额只加到 99,999。
    e.coins = PT_ECON_COIN_CAP - 5;
    assert(pt_econ_checkin(&e, 19, &r));
    assert(e.coins == PT_ECON_COIN_CAP);
}

static void test_shop_child(void)
{
    pt_econ_t e;
    pt_econ_init(&e, 7u);
    uint16_t shelf[8];

    // 蛋/婴儿无商店。
    assert(pt_econ_shop_build(&e, PT_STAGE_EGG, MIN0(0, 8, 0), shelf, 8) == 0);
    assert(pt_econ_shop_build(&e, PT_STAGE_BABY, MIN0(0, 8, 0), shelf, 8) == 0);

    uint8_t n = pt_econ_shop_build(&e, PT_STAGE_CHILD, MIN0(0, 8, 0),
                                   shelf, 8);
    assert(n == 5);
    assert(shelf[0] == PT_ITEM_RICEBALL);
    assert(shelf[1] == PT_ITEM_BISCUIT);
    bool ball = false, box = false, bub = false;
    for (uint8_t i = 2; i < n; i += 1) {
        ball = ball || shelf[i] == PT_ITEM_BALL;
        box = box || shelf[i] == PT_ITEM_MUSICBOX;
        bub = bub || shelf[i] == PT_ITEM_BUBBLES;
    }
    assert(ball && box && bub);

    // 同会话内多次重建结果恒定。
    uint16_t again[8];
    pt_econ_shop_build(&e, PT_STAGE_CHILD, MIN0(0, 13, 59), again, 8);
    for (uint8_t i = 0; i < n; i += 1) {
        assert(again[i] == shelf[i]);
    }
}

static bool in_pool(pt_item_t id)
{
    for (uint8_t i = 0; i < sizeof(TEEN_POOL) / sizeof(TEEN_POOL[0]); i += 1) {
        if (TEEN_POOL[i] == id) {
            return true;
        }
    }
    return false;
}

static void test_shop_teen_random_and_refresh(void)
{
    // 凌晨货架 = 前一天 20:00 货架（跨刷新会话稳定）。
    for (uint32_t seed = 1; seed <= 20; seed += 1) {
        pt_econ_t e;
        pt_econ_init(&e, seed);
        uint16_t late[8], early[8];
        uint8_t n1 = pt_econ_shop_build(&e, PT_STAGE_TEEN, MIN0(1, 20, 1),
                                        late, 8);
        uint8_t n2 = pt_econ_shop_build(&e, PT_STAGE_TEEN, MIN0(2, 7, 59),
                                        early, 8);
        assert(n1 == 8 && n2 == 8);
        for (uint8_t i = 0; i < 8; i += 1) {
            assert(late[i] == early[i]);
        }
    }

    // 少年货架：2 常备 + 6 个互不重复、全部来自随机池。
    // 三刷时间点跨多种子统计：合法 + 不同会话存在差异（刷新真的会换）。
    bool saw_different_sessions = false;
    for (uint32_t seed = 1; seed <= 40; seed += 1) {
        pt_econ_t e;
        pt_econ_init(&e, seed);
        uint16_t s0[8], s1[8], s2[8];
        uint8_t n0 = pt_econ_shop_build(&e, PT_STAGE_TEEN, MIN0(3, 8, 0),
                                        s0, 8);
        uint8_t n1v = pt_econ_shop_build(&e, PT_STAGE_TEEN, MIN0(3, 14, 0),
                                         s1, 8);
        uint8_t n2v = pt_econ_shop_build(&e, PT_STAGE_TEEN, MIN0(3, 20, 0),
                                         s2, 8);
        assert(n0 == 8 && n1v == 8 && n2v == 8);
        uint16_t *shelves[3] = { s0, s1, s2 };
        for (int k = 0; k < 3; k += 1) {
            assert(shelves[k][0] == PT_ITEM_RICEBALL);
            assert(shelves[k][1] == PT_ITEM_BISCUIT);
            for (uint8_t i = 2; i < 8; i += 1) {
                assert(in_pool((pt_item_t) shelves[k][i]));
                for (uint8_t j = 2; j < i; j += 1) {
                    assert(shelves[k][i] != shelves[k][j]);
                }
            }
        }
        bool diff = false;
        for (uint8_t i = 2; i < 8; i += 1) {
            diff = diff || s0[i] != s1[i] || s1[i] != s2[i];
        }
        saw_different_sessions = saw_different_sessions || diff;
    }
    assert(saw_different_sessions);
}

static void test_buy_and_inventory(void)
{
    pt_econ_t e;
    pt_econ_init(&e, 99u);
    e.coins = 200;

    // 幼儿买常备饭团：扣款、库存堆叠到同一格。
    assert(pt_econ_buy(&e, PT_STAGE_CHILD, MIN0(0, 9, 0), PT_ITEM_RICEBALL)
           == PT_ECON_OK);
    assert(e.coins == 195 && pt_econ_count(&e, PT_ITEM_RICEBALL) == 1);
    assert(pt_econ_buy(&e, PT_STAGE_CHILD, MIN0(0, 9, 0), PT_ITEM_RICEBALL)
           == PT_ECON_OK);
    assert(e.coins == 190 && pt_econ_count(&e, PT_ITEM_RICEBALL) == 2);
    uint8_t used_slots = 0;
    for (uint8_t i = 0; i < PT_ECON_INV_SLOTS; i += 1) {
        if (e.inv[i].qty > 0) {
            used_slots += 1;
        }
    }
    assert(used_slots == 1);

    // 阶段未到 / 余额不足（饭团常驻货架，保证先走到余额校验）。
    assert(pt_econ_buy(&e, PT_STAGE_CHILD, MIN0(0, 9, 0), PT_ITEM_BENTO)
           == PT_ECON_LOCKED);
    pt_econ_t poor;
    pt_econ_init(&poor, 1u);
    assert(pt_econ_buy(&poor, PT_STAGE_TEEN, MIN0(0, 9, 0), PT_ITEM_RICEBALL)
           == PT_ECON_NO_MONEY);

    // 玩具单件收藏：拥有后拒买。
    assert(pt_econ_buy(&e, PT_STAGE_CHILD, MIN0(0, 9, 0), PT_ITEM_BALL)
           == PT_ECON_OK);
    assert(e.coins == 90);
    assert(pt_econ_buy(&e, PT_STAGE_CHILD, MIN0(0, 9, 0), PT_ITEM_BALL)
           == PT_ECON_OWNED);

    // 随机货架缺席商品不可买（扫多种子构造一个缺席时刻）。
    bool found_absent = false;
    for (uint32_t seed = 1; seed <= 60 && !found_absent; seed += 1) {
        pt_econ_t f;
        pt_econ_init(&f, seed);
        f.coins = 1000;
        uint16_t shelf[8];
        pt_econ_shop_build(&f, PT_STAGE_TEEN, MIN0(0, 8, 0), shelf, 8);
        bool on = false;
        for (uint8_t i = 0; i < 8; i += 1) {
            on = on || shelf[i] == PT_ITEM_BENTO;
        }
        if (!on) {
            assert(pt_econ_buy(&f, PT_STAGE_TEEN, MIN0(0, 8, 0),
                               PT_ITEM_BENTO) == PT_ECON_NOT_ON_SHELF);
            found_absent = true;
        }
    }
    assert(found_absent);
}

static void test_food_and_toy_consumption(void)
{
    pt_econ_t e;
    pt_econ_init(&e, 5u);
    assert(pt_econ_add(&e, PT_ITEM_BISCUIT, 2) == PT_ECON_OK);
    assert(pt_econ_take_food(&e, PT_ITEM_BISCUIT) == PT_ECON_OK);
    assert(pt_econ_count(&e, PT_ITEM_BISCUIT) == 1);
    assert(pt_econ_take_food(&e, PT_ITEM_BISCUIT) == PT_ECON_OK);
    assert(pt_econ_take_food(&e, PT_ITEM_BISCUIT) == PT_ECON_NO_STOCK);
    assert(pt_econ_take_food(&e, PT_ITEM_BALL) == PT_ECON_BAD_ITEM);
    assert(pt_econ_add(&e, PT_ITEM_NONE, 1) == PT_ECON_BAD_ITEM);

    // 引擎拒绝喂食时的补回路径。
    assert(pt_econ_add(&e, PT_ITEM_RICEBALL, 1) == PT_ECON_OK);
    (void) pt_econ_take_food(&e, PT_ITEM_RICEBALL);
    assert(pt_econ_count(&e, PT_ITEM_RICEBALL) == 0);
    assert(pt_econ_add(&e, PT_ITEM_RICEBALL, 1) == PT_ECON_OK);
    assert(pt_econ_count(&e, PT_ITEM_RICEBALL) == 1);

    // 玩具每日 3 次，预扣可回滚，日界清零。
    assert(pt_econ_add(&e, PT_ITEM_MUSICBOX, 1) == PT_ECON_OK);
    for (int i = 0; i < PT_ECON_TOY_USES_DAY; i += 1) {
        assert(pt_econ_use_toy(&e, PT_ITEM_MUSICBOX) == PT_ECON_OK);
    }
    assert(pt_econ_use_toy(&e, PT_ITEM_MUSICBOX)
           == PT_ECON_USES_EXHAUSTED);
    pt_econ_refund_toy_use(&e, PT_ITEM_MUSICBOX);
    assert(pt_econ_use_toy(&e, PT_ITEM_MUSICBOX) == PT_ECON_OK);
    assert(pt_econ_use_toy(&e, PT_ITEM_BALL) == PT_ECON_NO_STOCK);

    pt_econ_on_day(&e);
    assert(e.toy_uses[PT_ITEM_MUSICBOX] == 0);
    assert(pt_econ_use_toy(&e, PT_ITEM_MUSICBOX) == PT_ECON_OK);
}

static void test_game_payout(void)
{
    pt_econ_t e;
    pt_econ_init(&e, 3u);
    // 前 5 局按评级 10/20/30。
    assert(pt_econ_game_payout(&e, PT_GAME_GRADE_GOOD) == 10);
    assert(pt_econ_game_payout(&e, PT_GAME_GRADE_GREAT) == 20);
    assert(pt_econ_game_payout(&e, PT_GAME_GRADE_PERFECT) == 30);
    assert(pt_econ_game_payout(&e, PT_GAME_GRADE_PERFECT) == 30);
    assert(pt_econ_game_payout(&e, PT_GAME_GRADE_GOOD) == 10);
    assert(e.coins == 100 && e.game_pays_today == 5);
    // 第 6 局起一律 5G 保底（再完美也一样）。
    assert(pt_econ_game_payout(&e, PT_GAME_GRADE_PERFECT) == 5);
    assert(pt_econ_game_payout(&e, PT_GAME_GRADE_GOOD) == 5);
    assert(e.coins == 110);

    // 日界清零重新全额。
    pt_econ_on_day(&e);
    assert(e.game_pays_today == 0);
    assert(pt_econ_game_payout(&e, PT_GAME_GRADE_PERFECT) == 30);

    // 余额上限钳制。
    e.coins = PT_ECON_COIN_CAP - 3;
    assert(pt_econ_game_payout(&e, PT_GAME_GRADE_PERFECT) == 0
           || e.coins == PT_ECON_COIN_CAP);
    assert(e.coins == PT_ECON_COIN_CAP);
}

static void test_s5_stipend_day(void)
{
    // 月历日 = day_id%30+1；5 号 = day_id 4/34/64…
    assert(!pt_econ_is_stipend_day(0));
    assert(!pt_econ_is_stipend_day(3));
    assert(pt_econ_is_stipend_day(4));
    assert(!pt_econ_is_stipend_day(5));
    assert(pt_econ_is_stipend_day(34));
    assert(pt_econ_is_stipend_day(64));
    // 负 day_id（开档当天 06:00 前）也合法取模，不误判。
    assert(pt_econ_is_stipend_day(-26));   // -26%30 取正 = 4 → 月历 5 号
    assert(!pt_econ_is_stipend_day(-27));
}

static void test_s5_soft_tax(void)
{
    pt_econ_t e;
    pt_econ_init(&e, 3u);

    // 边界：恰好 10,000G 不打折扣；>10,000 后劳动收益 ×80%。
    e.coins = PT_ECON_SOFT_TAX_AT;
    assert(pt_econ_credit_income(&e, 100) == 100);
    assert(e.coins == 10100);
    assert(pt_econ_credit_income(&e, 100) == 80);
    assert(e.coins == 10180);

    // 100G 收益打 8 折 = 80G；30G = 24G。
    e.coins = 50000;
    assert(pt_econ_credit_income(&e, 30) == 24);

    // 游戏奖金同样打折。
    e.coins = 20000;
    e.game_pays_today = 0;
    assert(pt_econ_game_payout(&e, PT_GAME_GRADE_PERFECT) == 24);
    // 保底 5G ×80% = 4G。
    for (int i = 0; i < 5; i += 1) {
        (void) pt_econ_game_payout(&e, PT_GAME_GRADE_PERFECT);
    }
    assert(pt_econ_game_payout(&e, PT_GAME_GRADE_PERFECT) == 4);

    // 99,999 上限仍然钳制。
    e.coins = PT_ECON_COIN_CAP;
    assert(pt_econ_credit_income(&e, 100) == 0);

    // 软税是"持有现金"税：花回 10,000 以下立即恢复全额（鼓励消费）。
    e.coins = 10001;
    (void) pt_econ_spend_coins(&e, 2);
    assert(e.coins == 9999);
    assert(pt_econ_credit_income(&e, 100) == 100);
}

static void test_s5_shelf_discount(void)
{
    // 价格 helper：5/15/25（day_id 4/14/24）8 折；家具日（9/19/29）食物不折。
    assert(pt_econ_item_price_now(5, 0) == 5);
    assert(pt_econ_item_price_now(5, 4) == 4);
    assert(pt_econ_item_price_now(5, 14) == 4);
    assert(pt_econ_item_price_now(5, 24) == 4);
    assert(pt_econ_item_price_now(5, 9) == 5);
    assert(pt_econ_item_price_now(100, 4) == 80);

    // 实购：MIN0(4,9,0) → day_id=4（月历 5 号），饭团 5G 卖 4G。
    pt_econ_t e;
    pt_econ_init(&e, 99u);
    e.coins = 10;
    assert(pt_econ_buy(&e, PT_STAGE_CHILD, MIN0(4, 9, 0), PT_ITEM_RICEBALL)
           == PT_ECON_OK);
    assert(e.coins == 6 && pt_econ_count(&e, PT_ITEM_RICEBALL) == 1);

    // 次日（day_id=5，非折扣日）恢复 5G。
    assert(pt_econ_buy(&e, PT_STAGE_CHILD, MIN0(5, 9, 0), PT_ITEM_RICEBALL)
           == PT_ECON_OK);
    assert(e.coins == 1);

    // 折扣后才买得起：全价 5G 不够、折后 4G 可以。
    pt_econ_t poor;
    pt_econ_init(&poor, 99u);
    poor.coins = 4;
    assert(pt_econ_buy(&poor, PT_STAGE_CHILD, MIN0(5, 9, 0), PT_ITEM_RICEBALL)
           == PT_ECON_NO_MONEY);
    assert(pt_econ_buy(&poor, PT_STAGE_CHILD, MIN0(4, 9, 0), PT_ITEM_RICEBALL)
           == PT_ECON_OK);
    assert(poor.coins == 0);
}

int main(void)
{
    test_catalog_is_legal();
    test_checkin_streak_and_caps();
    test_shop_child();
    test_shop_teen_random_and_refresh();
    test_buy_and_inventory();
    test_food_and_toy_consumption();
    test_game_payout();
    test_s5_stipend_day();
    test_s5_soft_tax();
    test_s5_shelf_discount();
    printf("test_pet_econ: PASS\n");
    return 0;
}
