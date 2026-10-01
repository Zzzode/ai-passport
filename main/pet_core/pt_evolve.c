#include "pt_evolve.h"

#include "pt_config.h"
#include "pt_rng.h"

int32_t pt_stage_min_duration(pt_stage_t stage)
{
    switch (stage) {
    case PT_STAGE_EGG:
        return PT_CFG_EGG_MIN;
    case PT_STAGE_BABY:
        return PT_CFG_BABY_MIN;
    case PT_STAGE_CHILD:
        return PT_CFG_CHILD_MIN;
    case PT_STAGE_TEEN:
        return PT_CFG_TEEN_MIN;
    default:
        return 0;
    }
}

pt_care_tier_t pt_care_tier(const pt_ledger_t *ledger, bool require_topups)
{
    if (ledger->big == 0 && ledger->small == 0) {
        if (!require_topups
            || (ledger->full_topups >= PT_CFG_TOPUP_NEED_PERFECT
                && ledger->happy_topups >= PT_CFG_TOPUP_NEED_PERFECT)) {
            return PT_CARE_PERFECT;
        }
    }
    if (ledger->big == 0 && ledger->small <= PT_CFG_CARE_GREAT_SMALL) {
        return PT_CARE_GREAT;
    }
    if (ledger->big <= PT_CFG_CARE_NORMAL_BIG
        && ledger->small <= PT_CFG_CARE_NORMAL_SMALL) {
        return PT_CARE_NORMAL;
    }
    return PT_CARE_NEGLECT;
}

pt_ledger_t pt_ledger_sum(const pt_ledger_t *a, const pt_ledger_t *b)
{
    pt_ledger_t sum;
    sum.small = a->small + b->small;
    sum.big = a->big + b->big;
    sum.full_topups = a->full_topups + b->full_topups;
    sum.happy_topups = a->happy_topups + b->happy_topups;
    // 敷衍次数跨阶段合计（月影判定），uint8 饱和。
    int32_t perfunctory = (int32_t) a->perfunctory + b->perfunctory;
    sum.perfunctory = perfunctory > 255 ? 255 : (uint8_t) perfunctory;
    return sum;
}

pt_species_t pt_pick_teen(pt_care_tier_t tier)
{
    switch (tier) {
    case PT_CARE_PERFECT:
    case PT_CARE_GREAT:
        return PT_SP_TEEN_A;
    case PT_CARE_NORMAL:
        return PT_SP_TEEN_B;
    case PT_CARE_NEGLECT:
        return PT_SP_TEEN_C;
    }
    return PT_SP_TEEN_B;
}

pt_species_t pt_pick_adult(pt_care_tier_t tier)
{
    switch (tier) {
    case PT_CARE_PERFECT:
        return PT_SP_ADULT_PERFECT;
    case PT_CARE_GREAT:
        return PT_SP_ADULT_GREAT;
    case PT_CARE_NORMAL:
        return PT_SP_ADULT_NORMAL;
    case PT_CARE_NEGLECT:
        return PT_SP_ADULT_NEGLECT;
    }
    return PT_SP_ADULT_NORMAL;
}

bool pt_hidden_moon_unlocked(pt_care_tier_t tier, uint8_t bond,
                             uint16_t perfunctory)
{
    // 技能三维 P0 占位为 BALANCED（极差 0 ≤ 10），不再额外判断。
    return tier == PT_CARE_PERFECT && bond >= PT_CFG_HIDDEN_MOON_BOND
        && perfunctory == 0;
}

pt_species_t pt_pick_adult_full(pt_care_tier_t tier, uint8_t bond,
                                uint16_t perfunctory)
{
    if (pt_hidden_moon_unlocked(tier, bond, perfunctory)) {
        return PT_SP_ADULT_MOON;
    }
    return pt_pick_adult(tier);
}

int32_t pt_roll_lifespan_days(pt_care_tier_t tier, uint32_t *rng_state)
{
    static const int32_t base[PT_CARE_NEGLECT + 1] = { 16, 15, 13, 11 };
    int32_t days = base[tier];
    days += (int32_t) pt_rng_below(rng_state, 5) - 2;   // ±2
    if (days < 1) {
        days = 1;
    }
    return days;
}
