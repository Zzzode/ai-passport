// 主机测试：进化分档/物种映射/隐藏角色覆盖/寿命掷定（designs 04 §10）。
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "pt_config.h"
#include "pt_evolve.h"
#include "pt_rng.h"

// 复合字面量是左值，可以直接取地址传参。
#define LED(s, b, f, h, p) \
    &((pt_ledger_t) { .small = (s), .big = (b), .full_topups = (f), \
                      .happy_topups = (h), .perfunctory = (p) })

// 文档 04 §10.1：四档边界组合无空洞。
static void test_tier_boundaries(void)
{
    // 不要求顶满：0 失误即 PERFECT。
    assert(pt_care_tier(LED(0, 0, 0, 0, 0), false) == PT_CARE_PERFECT);
    // 要求顶满（TEEN→ADULT）：两类顶满各 ≥5 且 0 失误才 PERFECT。
    assert(pt_care_tier(LED(0, 0, 5, 5, 0), true) == PT_CARE_PERFECT);
    // 文档 04 §10.2：去掉任意一次顶满即降 GREAT。
    assert(pt_care_tier(LED(0, 0, 4, 5, 0), true) == PT_CARE_GREAT);
    assert(pt_care_tier(LED(0, 0, 5, 4, 0), true) == PT_CARE_GREAT);
    // 小失误 1 且无大失误：GREAT；2 个小失误：NORMAL。
    assert(pt_care_tier(LED(1, 0, 5, 5, 0), true) == PT_CARE_GREAT);
    assert(pt_care_tier(LED(2, 0, 5, 5, 0), true) == PT_CARE_NORMAL);
    // 大失误 1 且小失误 ≤5：NORMAL；越界 NEGLECT。
    assert(pt_care_tier(LED(5, 1, 5, 5, 0), true) == PT_CARE_NORMAL);
    assert(pt_care_tier(LED(6, 1, 5, 5, 0), true) == PT_CARE_NEGLECT);
    assert(pt_care_tier(LED(0, 2, 5, 5, 0), true) == PT_CARE_NEGLECT);
}

// 文档 04 §10.4：隐藏角色条件集的互斥/覆盖优先级。
static void test_hidden_moon_override(void)
{
    assert(pt_hidden_moon_unlocked(PT_CARE_PERFECT, 100, 0));
    // 任一条件不满足即不解锁：低一档、差 1 点亲密度、 1 次敷衍。
    assert(!pt_hidden_moon_unlocked(PT_CARE_GREAT, 100, 0));
    assert(!pt_hidden_moon_unlocked(PT_CARE_PERFECT, 99, 0));
    assert(!pt_hidden_moon_unlocked(PT_CARE_PERFECT, 100, 1));

    // 满足时覆盖普通成年映射为 MOON；否则按档回退。
    assert(pt_pick_adult_full(PT_CARE_PERFECT, 100, 0) == PT_SP_ADULT_MOON);
    assert(pt_pick_adult_full(PT_CARE_PERFECT, 99, 0) == PT_SP_ADULT_PERFECT);
    assert(pt_pick_adult_full(PT_CARE_GREAT, 100, 0) == PT_SP_ADULT_GREAT);
    assert(pt_pick_adult_full(PT_CARE_NEGLECT, 100, 0) == PT_SP_ADULT_NEGLECT);
}

// 普通四档成年与少年分支各有唯一物种，无空洞。
static void test_species_tables(void)
{
    assert(pt_pick_teen(PT_CARE_PERFECT) == PT_SP_TEEN_A);
    assert(pt_pick_teen(PT_CARE_GREAT) == PT_SP_TEEN_A);
    assert(pt_pick_teen(PT_CARE_NORMAL) == PT_SP_TEEN_B);
    assert(pt_pick_teen(PT_CARE_NEGLECT) == PT_SP_TEEN_C);

    assert(pt_pick_adult(PT_CARE_PERFECT) == PT_SP_ADULT_PERFECT);
    assert(pt_pick_adult(PT_CARE_GREAT) == PT_SP_ADULT_GREAT);
    assert(pt_pick_adult(PT_CARE_NORMAL) == PT_SP_ADULT_NORMAL);
    assert(pt_pick_adult(PT_CARE_NEGLECT) == PT_SP_ADULT_NEGLECT);
}

// 文档 04 §10.5：寿命固定种子稳定、照顾档间总体单调（越好照护越长寿）。
static void test_lifespan_roll(void)
{
    uint32_t a = 1234, b = 1234;
    assert(pt_roll_lifespan_days(PT_CARE_PERFECT, &a)
        == pt_roll_lifespan_days(PT_CARE_PERFECT, &b));

    int32_t sum[PT_CARE_NEGLECT + 1] = { 0 };
    for (uint32_t seed = 1; seed <= 500; seed += 1) {
        for (int tier = 0; tier <= PT_CARE_NEGLECT; tier += 1) {
            uint32_t rng = seed * 7919u + (uint32_t) tier * 104729u;
            int32_t days = pt_roll_lifespan_days((pt_care_tier_t) tier, &rng);
            assert(days >= 1);
            sum[tier] += days;
        }
    }
    // 基线 16/15/13/11（±2 同分布扰动），均值必须严格递减。
    assert(sum[PT_CARE_PERFECT] > sum[PT_CARE_GREAT]);
    assert(sum[PT_CARE_GREAT] > sum[PT_CARE_NORMAL]);
    assert(sum[PT_CARE_NORMAL] > sum[PT_CARE_NEGLECT]);
}

// 跨阶段账本汇总：字段齐全，敷衍饱和。
static void test_ledger_sum(void)
{
    pt_ledger_t a = *(LED(1, 2, 3, 4, 200));
    pt_ledger_t b = *(LED(5, 6, 7, 8, 100));
    pt_ledger_t s = pt_ledger_sum(&a, &b);
    assert(s.small == 6 && s.big == 8);
    assert(s.full_topups == 10 && s.happy_topups == 12);
    assert(s.perfunctory == 255);
}

int main(void)
{
    test_tier_boundaries();
    test_hidden_moon_override();
    test_species_tables();
    test_lifespan_roll();
    test_ledger_sum();
    printf("test_pt_evolve: PASS\n");
    return 0;
}
