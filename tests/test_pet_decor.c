// 主机测试：换装 / 功能家具 / 房间主题纯逻辑（designs/Tomagotchi/05 §7, 06）。
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pet_decor.h"
#include "pet_econ.h"
#include "pt_types.h"

static void test_init_defaults(void)
{
    pt_decor_t d;
    pt_decor_init(&d);
    assert(d.theme == PT_THEME_COZY);
    assert(pt_decor_owns_theme(&d, PT_THEME_COZY));
    assert(!pt_decor_owns_theme(&d, PT_THEME_SKY));
    assert(d.outfits == 0 && d.furniture == 0 && d.placed == 0);
    for (uint8_t i = 0; i < PT_SLOT_COUNT; i += 1) {
        assert(pt_decor_worn(&d, (pt_slot_t) i) == PT_OUTFIT_NONE);
    }
    assert(pt_decor_validate(&d));
    assert(pt_decor_meal_bonus(&d) == 0);
    assert(pt_decor_happy_pct(&d) == 0);
    assert(pt_decor_energy_pct(&d) == 100);
    assert(pt_decor_score_pct(&d) == 0);
}

static void test_sale_days(void)
{
    // day_id 4 = 月历第 5 日：全场 8 折。
    assert(pt_econ_sale_pct(4, false) == 20);
    assert(pt_econ_sale_pct(4, true) == 20);
    // day_id 9 = 第 10 日：只有家具 9 折。
    assert(pt_econ_sale_pct(9, false) == 0);
    assert(pt_econ_sale_pct(9, true) == 10);
    // 第 15 日全场；普通日无折扣；跨月循环（day_id 34 = 次月 5 号）。
    assert(pt_econ_sale_pct(14, true) == 20);
    assert(pt_econ_sale_pct(0, true) == 0);
    assert(pt_econ_sale_pct(34, false) == 20);
    assert(pt_econ_sale_pct(29, true) == 10);   // 第 30 日家具
}

static void test_buy_outfit(void)
{
    pt_decor_t d;
    pt_econ_t e;
    pt_decor_init(&d);
    pt_econ_init(&e, 1u);
    e.coins = 1000;
    e.shells = 20;

    // 少年前锁。
    assert(pt_decor_buy_outfit(&d, &e, PT_STAGE_CHILD, 0,
                               PT_OUTFIT_CAP) == PT_DECOR_LOCKED);
    assert(e.coins == 1000);

    // 全价日 200G。
    assert(pt_decor_buy_outfit(&d, &e, PT_STAGE_TEEN, 0,
                               PT_OUTFIT_CAP) == PT_DECOR_OK);
    assert(e.coins == 800);
    assert(pt_decor_owns_outfit(&d, PT_OUTFIT_CAP));
    // 重复购买拒绝且不扣钱。
    assert(pt_decor_buy_outfit(&d, &e, PT_STAGE_TEEN, 0,
                               PT_OUTFIT_CAP) == PT_DECOR_OWNED);
    assert(e.coins == 800);

    // 折扣日：围巾 300G → 240G。
    assert(pt_decor_buy_outfit(&d, &e, PT_STAGE_TEEN, 4,
                               PT_OUTFIT_SCARF) == PT_DECOR_OK);
    assert(e.coins == 560);

    // 贝壳款：8 贝壳，不参与 G 折扣。
    assert(pt_decor_buy_outfit(&d, &e, PT_STAGE_TEEN, 4,
                               PT_OUTFIT_STARHAT) == PT_DECOR_OK);
    assert(e.shells == 12);

    // 钱不够。
    e.coins = 100;
    assert(pt_decor_buy_outfit(&d, &e, PT_STAGE_TEEN, 0,
                               PT_OUTFIT_TOPHAT) == PT_DECOR_NO_MONEY);
    assert(!pt_decor_owns_outfit(&d, PT_OUTFIT_TOPHAT));
    e.shells = 3;
    assert(pt_decor_buy_outfit(&d, &e, PT_STAGE_TEEN, 0,
                               PT_OUTFIT_STARHAT) == PT_DECOR_OWNED);
    // 帽子已拥有，不会走到贝壳不足；用新 id 验贝壳不足：本目录只一件贝壳帽，
    // 这里直接验扣款原语。
    assert(!pt_econ_spend_shells(&e, 8));
    assert(e.shells == 3);
    assert(pt_decor_validate(&d));
}

static void test_equip_rules(void)
{
    pt_decor_t d;
    pt_econ_t e;
    pt_decor_init(&d);
    pt_econ_init(&e, 1u);
    e.coins = 2000;
    assert(pt_decor_buy_outfit(&d, &e, PT_STAGE_TEEN, 0,
                               PT_OUTFIT_CAP) == PT_DECOR_OK);
    assert(pt_decor_buy_outfit(&d, &e, PT_STAGE_TEEN, 0,
                               PT_OUTFIT_GLASSES) == PT_DECOR_OK);

    // 未拥有不能穿。
    assert(pt_decor_equip(&d, PT_OUTFIT_SCARF) == PT_DECOR_NOT_OWNED);
    assert(pt_decor_worn(&d, PT_SLOT_COLLAR) == PT_OUTFIT_NONE);

    assert(pt_decor_equip(&d, PT_OUTFIT_CAP) == PT_DECOR_OK);
    assert(pt_decor_worn(&d, PT_SLOT_HAT) == PT_OUTFIT_CAP);
    // 同槽换装直接替换。
    assert(pt_decor_buy_outfit(&d, &e, PT_STAGE_TEEN, 0,
                               PT_OUTFIT_TOPHAT) == PT_DECOR_OK);
    assert(pt_decor_equip(&d, PT_OUTFIT_TOPHAT) == PT_DECOR_OK);
    assert(pt_decor_worn(&d, PT_SLOT_HAT) == PT_OUTFIT_TOPHAT);
    // 再点一次脱下。
    assert(pt_decor_equip(&d, PT_OUTFIT_TOPHAT) == PT_DECOR_OK);
    assert(pt_decor_worn(&d, PT_SLOT_HAT) == PT_OUTFIT_NONE);
    // 眼镜在脸槽，与帽子互不干扰。
    assert(pt_decor_equip(&d, PT_OUTFIT_GLASSES) == PT_DECOR_OK);
    assert(pt_decor_worn(&d, PT_SLOT_FACE) == PT_OUTFIT_GLASSES);
    assert(pt_decor_unequip_slot(&d, PT_SLOT_FACE) == PT_DECOR_OK);
    assert(pt_decor_worn(&d, PT_SLOT_FACE) == PT_OUTFIT_NONE);
    assert(pt_decor_validate(&d));
}

static void test_furniture_place_and_buffs(void)
{
    pt_decor_t d;
    pt_econ_t e;
    pt_decor_init(&d);
    pt_econ_init(&e, 1u);
    e.coins = 3000;

    // 未拥有不能摆放。
    assert(pt_decor_place(&d, PT_FURN_PLANT) == PT_DECOR_NOT_OWNED);

    // 厨房 1200G，第 20 日家具 9 折 = 1080G。
    assert(pt_decor_buy_furn(&d, &e, PT_STAGE_TEEN, 19,
                             PT_FURN_KITCHEN) == PT_DECOR_OK);
    assert(e.coins == 3000 - 1080);
    assert(pt_decor_place(&d, PT_FURN_KITCHEN) == PT_DECOR_OK);
    assert(pt_decor_is_placed(&d, PT_FURN_KITCHEN));
    assert(pt_decor_meal_bonus(&d) == PT_DECOR_MEAL_BONUS);
    // 重复摆放幂等；收起后增益消失。
    assert(pt_decor_place(&d, PT_FURN_KITCHEN) == PT_DECOR_OK);
    assert(pt_decor_unplace(&d, PT_FURN_KITCHEN) == PT_DECOR_OK);
    assert(!pt_decor_is_placed(&d, PT_FURN_KITCHEN));
    assert(pt_decor_meal_bonus(&d) == 0);

    // 健身角：第 5 日全场 8 折 = 640G。
    assert(pt_decor_buy_furn(&d, &e, PT_STAGE_TEEN, 4,
                             PT_FURN_GYM) == PT_DECOR_OK);
    assert(e.coins == 1920 - 640);
    assert(pt_decor_place(&d, PT_FURN_GYM) == PT_DECOR_OK);
    assert(pt_decor_energy_pct(&d) == PT_DECOR_GYM_ENERGY_PCT);
    assert(pt_decor_score_pct(&d) == PT_DECOR_GYM_SCORE_PCT);

    // 盆栽：拥有即验心情增益。
    assert(pt_decor_buy_furn(&d, &e, PT_STAGE_TEEN, 0,
                             PT_FURN_PLANT) == PT_DECOR_OK);
    assert(pt_decor_place(&d, PT_FURN_PLANT) == PT_DECOR_OK);
    assert(pt_decor_happy_pct(&d) == PT_DECOR_PLANT_HAPPY_PCT);
    assert(pt_decor_validate(&d));

    // 幼儿期不能买家具（阶段闸先于重复拥有判定）。
    assert(pt_decor_buy_furn(&d, &e, PT_STAGE_CHILD, 0,
                             PT_FURN_PLANT) == PT_DECOR_LOCKED);
}

static void test_themes(void)
{
    pt_decor_t d;
    pt_econ_t e;
    pt_decor_init(&d);
    pt_econ_init(&e, 1u);
    e.coins = 1000;
    e.shells = 20;

    // 未拥有不能启用。
    assert(pt_decor_set_theme(&d, PT_THEME_SKY) == PT_DECOR_NOT_OWNED);
    assert(pt_decor_buy_theme(&d, &e, PT_THEME_SKY) == PT_DECOR_OK);
    assert(e.coins == 600);
    // 买下不自动启用（UI 层会再发 set），启用后生效。
    assert(d.theme == PT_THEME_COZY);
    assert(pt_decor_set_theme(&d, PT_THEME_SKY) == PT_DECOR_OK);
    assert(d.theme == PT_THEME_SKY);
    // 重复买拒绝。
    assert(pt_decor_buy_theme(&d, &e, PT_THEME_SKY) == PT_DECOR_OWNED);
    // 贝壳主题。
    assert(pt_decor_buy_theme(&d, &e, PT_THEME_STARRY) == PT_DECOR_OK);
    assert(e.shells == 8);
    assert(pt_decor_set_theme(&d, PT_THEME_STARRY) == PT_DECOR_OK);
    e.shells = 3;
    // 贝壳不足（换新账户状态直接试）。
    pt_decor_t d2;
    pt_econ_t e2;
    pt_decor_init(&d2);
    pt_econ_init(&e2, 1u);
    assert(pt_decor_buy_theme(&d2, &e2, PT_THEME_STARRY)
           == PT_DECOR_NO_MONEY);
    assert(pt_decor_validate(&d));
}

static void test_validate_rejects_dirty(void)
{
    pt_decor_t d;
    pt_decor_init(&d);
    assert(pt_decor_validate(&d));

    pt_decor_t bad = d;
    bad.outfits = 0x80;   // 杂位
    assert(!pt_decor_validate(&bad));

    bad = d;
    bad.furniture = 0x08; // 不存在的家具
    assert(!pt_decor_validate(&bad));

    bad = d;
    bad.placed = 0x01;    // 摆放了未拥有的家具
    assert(!pt_decor_validate(&bad));

    bad = d;
    bad.themes = 0x10;    // 不存在的主题
    assert(!pt_decor_validate(&bad));

    bad = d;
    bad.theme = PT_THEME_COUNT;
    assert(!pt_decor_validate(&bad));

    bad = d;
    bad.worn[PT_SLOT_HAT] = PT_OUTFIT_CAP;   // 没拥有却穿着
    assert(!pt_decor_validate(&bad));

    // 拥有并穿着合法。
    pt_econ_t e;
    pt_econ_init(&e, 1u);
    e.coins = 500;
    assert(pt_decor_buy_outfit(&d, &e, PT_STAGE_TEEN, 0,
                               PT_OUTFIT_CAP) == PT_DECOR_OK);
    assert(pt_decor_equip(&d, PT_OUTFIT_CAP) == PT_DECOR_OK);
    assert(pt_decor_validate(&d));
}

int main(void)
{
    test_init_defaults();
    test_sale_days();
    test_buy_outfit();
    test_equip_rules();
    test_furniture_place_and_buffs();
    test_themes();
    test_validate_rejects_dirty();
    printf("test_pet_decor: PASS\n");
    return 0;
}
