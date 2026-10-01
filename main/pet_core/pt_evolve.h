// pet_core/pt_evolve.h —— 阶段时长、照顾分档、进化物种与寿命的纯函数。
#pragma once

#include "pt_types.h"

// 该阶段至少持续多少分钟；EGG/BABY 为定时，CHILD/TEEN 到点后等晨间窗口。
int32_t pt_stage_min_duration(pt_stage_t stage);

// 依据账本计算照顾档。require_topups 为 true 时（TEEN→ADULT），
// PERFECT 还要求本阶段两类顶满各 ≥5。
pt_care_tier_t pt_care_tier(const pt_ledger_t *ledger, bool require_topups);

// 汇总两个阶段账本（CHILD→TEEN 用上一阶段+本阶段）。
pt_ledger_t pt_ledger_sum(const pt_ledger_t *a, const pt_ledger_t *b);

// CHILD→TEEN 简化分流（PERFECT/GREAT 同归 A）。
pt_species_t pt_pick_teen(pt_care_tier_t tier);

// TEEN→ADULT：P0 仅按照顾档常规映射 4 个普通成年；满足月影隐藏条件时覆盖结果。
pt_species_t pt_pick_adult(pt_care_tier_t tier);

// 隐藏角色「月影」解锁判定（designs 04 §6）：
// PERFECT 档 + 亲密度达到 PT_CFG_HIDDEN_MOON_BOND + 0 次敷衍。
// 技能维度（art/body/mind 极差 ≤10）P0 尚未上线，按 BALANCED 占位恒成立；
// 又因 TEEN 期亲密度上限为 80（pt_cfg_bond_cap），正常玩法 P0 不可达，
// 条件随亲密度/技能系统在后续期次开放（"条件随系统上线才开放"）。
bool pt_hidden_moon_unlocked(pt_care_tier_t tier, uint8_t bond,
                             uint16_t perfunctory);

// 按账本汇总信息在普通/隐藏成年物种间做最终选择。
pt_species_t pt_pick_adult_full(pt_care_tier_t tier, uint8_t bond,
                                uint16_t perfunctory);

// 成年定型时掷定自然寿命（成年后天数，含 hash 扰动 ±2）。
int32_t pt_roll_lifespan_days(pt_care_tier_t tier, uint32_t *rng_state);
