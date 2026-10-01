// pet_core/pt_config.h —— PasPet 全部可调数值（v0.1，对应 designs/Tomagotchi/12）。
// 规则：逻辑代码不得出现魔法数字，一律引用这里的 PT_CFG_*。
#pragma once

#include <stdint.h>

#include "pt_types.h"

// ---- 时间与阶段（分钟） ----
#define PT_CFG_EGG_MIN              5
#define PT_CFG_BABY_MIN             60
#define PT_CFG_CHILD_MIN            (24 * 60)
#define PT_CFG_TEEN_MIN             (48 * 60)
#define PT_CFG_EVOLVE_WINDOW_START  (6 * 60)    // 晨间进化窗口 06:00
#define PT_CFG_EVOLVE_WINDOW_END    (10 * 60)   // 10:00（不含）
#define PT_CFG_DAY_BOUNDARY_MIN     (6 * 60)    // 清醒日界 06:00（年龄/配额重置）

// 作息（分钟时刻）
#define PT_CFG_SLEEP_CHILD_START    (21 * 60)
#define PT_CFG_SLEEP_TEEN_START     (22 * 60)
#define PT_CFG_WAKE_MIN             (8 * 60)
// 夜间区间的清晨上界：与清醒日界 06:00 一致，避免 06:00–08:00 被当成"该睡"。
#define PT_CFG_NIGHT_END_MIN        (6 * 60)

// ---- 计量（内部 0-100 整数；衰减为"点/小时"，用定点累加到分钟） ----
#define PT_CFG_METER_CALL_THRESHOLD 10          // ≤10 发起呼叫
#define PT_CFG_METER_REARM          25          // 回升到 >25 后才允许再次呼叫
#define PT_CFG_TOPUP_FROM           75          // 从 ≥75 补满到 100 计一次顶满
#define PT_CFG_TOPUP_NEED_PERFECT   5

// 清醒衰减（点/小时），按阶段索引 PT_STAGE_*
extern const uint8_t pt_cfg_dec_full[PT_STAGE_COUNT];
extern const uint8_t pt_cfg_dec_happy[PT_STAGE_COUNT];
extern const uint8_t pt_cfg_dec_energy[PT_STAGE_COUNT];

#define PT_CFG_NIGHT_DECAY_NUM      25          // 睡眠中饱腹/心情衰减 ×25/100
#define PT_CFG_NIGHT_DECAY_DEN     100
#define PT_CFG_ENERGY_REGEN_PER_H   35
#define PT_CFG_POOR_SLEEP_NUM       70          // 忘关灯当晚体力回复 ×70/100
#define PT_CFG_LOW_ENERGY           25
#define PT_CFG_LOW_ENERGY_MULT_NUM 150
#define PT_CFG_LOW_ENERGY_MULT_DEN 100

// ---- 呼叫窗口（分钟），按阶段索引 ----
extern const uint16_t pt_cfg_call_window[PT_STAGE_COUNT];

// ---- 动作回复 ----
#define PT_CFG_MEAL_FULL            25
#define PT_CFG_MEAL_WEIGHT          1
#define PT_CFG_MEAL_REFUSE_AT       90
#define PT_CFG_BOTTLE_FULL          30
#define PT_CFG_BOTTLE_HAPPY         10
#define PT_CFG_SNACK_HAPPY          20
#define PT_CFG_SNACK_FULL           5
#define PT_CFG_SNACK_WEIGHT         2
#define PT_CFG_SNACK_SAFE_PER_DAY   3
#define PT_CFG_SNACK_SICK_PERMILLE  100         // 第 4 个起 10%
#define PT_CFG_GAME_HAPPY_GOOD      10
#define PT_CFG_GAME_HAPPY_GREAT     15
#define PT_CFG_GAME_HAPPY_PERFECT   20
#define PT_CFG_GAME_HAPPY_FULL_N    3           // 每日前 3 局全额心情，之后 +5
#define PT_CFG_GAME_HAPPY_EXTRA     5
#define PT_CFG_GAME_WEIGHT_MIN_CAP  3           // 每日靠游戏最多 -3 体重
#define PT_CFG_SKILL_GAIN_GOOD      1
#define PT_CFG_SKILL_GAIN_GREAT     2
#define PT_CFG_SKILL_GAIN_PERFECT   3
#define PT_CFG_SKILL_DAILY_CAP      9           // 每类技能每日 ≤9
#define PT_CFG_SKILL_CAP            99
#define PT_CFG_GAME_LOW_ENERGY      25          // 低于此值游戏得分上限 ×80%
#define PT_CFG_GAME_SCORE_PENALTY_NUM 80
#define PT_CFG_GAME_SCORE_PENALTY_DEN 100
#define PT_CFG_BOND_G6_GOOD         3           // G6 亲密度
#define PT_CFG_BOND_G6_GREAT        5
#define PT_CFG_BOND_G6_PERFECT      8
#define PT_CFG_JOB_MIN_ENERGY       25          // 体力低于此值不能上班（07 §5.2）
#define PT_CFG_JOB_SENIOR_RELAX_NUM 90          // 老年评级阈值放宽 10%（×90%）
#define PT_CFG_JOB_SENIOR_RELAX_DEN 100

// 每个游戏对应的技能维度；0xFF=无技能收益（G6）。
extern const uint8_t pt_cfg_game_skill[PT_GAME_COUNT];
// 每个游戏的体力消耗（5–10，07 §4）。
extern const uint8_t pt_cfg_game_energy[PT_GAME_COUNT];
#define PT_CFG_TOY_HAPPY            12          // 玩具：心情回复
#define PT_CFG_TOY_ENERGY           5           // 玩具：体力消耗
#define PT_CFG_PAT_HAPPY            3
#define PT_CFG_PAT_BOND             1
#define PT_CFG_PAT_HOURLY_CAP       3
#define PT_CFG_CLEAN_BOND_DAY_CAP   3
#define PT_CFG_CALL_RESOLVE_BOND    1
#define PT_CFG_HEALTH_REGEN_DAY     1           // /小时，<80 时
#define PT_CFG_HEALTH_REGEN_NIGHT   3

// ---- 亲密度阶段上限 ----
extern const uint8_t pt_cfg_bond_cap[PT_STAGE_COUNT];

// ---- 排泄 / 卫生 / 疾病 ----
extern const uint16_t pt_cfg_poop_interval[PT_STAGE_COUNT]; // 分钟
#define PT_CFG_POOP_JITTER_NUM      15
#define PT_CFG_POOP_JITTER_DEN     100
#define PT_CFG_POOP_MAX             4
#define PT_CFG_POOP_HEALTH_TICK_MIN 30
#define PT_CFG_POOP_HEALTH_TICK      2
#define PT_CFG_POOP_SICK_PERMILLE  500          // 积满 4 个立即 50%
#define PT_CFG_POOP_SICK_GROWTH_PERMILLE 250    // 每持续 30 分钟 +25%
#define PT_CFG_POOP_SICK_CAP_PERMILLE 950
#define PT_CFG_MED_CURE_PERMILLE   700
#define PT_CFG_MED_HEALTH          20
#define PT_CFG_SICK_HAPPY_PER_H     3
#define PT_CFG_SICK_HEALTH_PER_H    2

// ---- 体重 ----
#define PT_CFG_WEIGHT_START         5
#define PT_CFG_WEIGHT_BASE          30          // 物种基准（P0 统一值）
#define PT_CFG_WEIGHT_OVERWEIGHT    (PT_CFG_WEIGHT_BASE + 25)
#define PT_CFG_WEIGHT_MAX           99
#define PT_CFG_WEIGHT_MIN            5

// ---- 照顾分档阈值 ----
#define PT_CFG_CARE_GREAT_SMALL      1
#define PT_CFG_CARE_NORMAL_SMALL     5
#define PT_CFG_CARE_NORMAL_BIG       1

// ---- 隐藏角色 ----
#define PT_CFG_HIDDEN_MOON_BOND      100   // 月影要求的亲密度

// ---- 离线社交 P2-S3（08 §2–§4） ----
#define PT_CFG_SOC_MATCHMAKER_DELAY_DAYS 1    // 成年第 2 天起每天推荐
#define PT_CFG_SOC_GREET_BOND          5
#define PT_CFG_SOC_GIFT_COINS           80
#define PT_CFG_SOC_GIFT_BOND_MATCH      15   // 送对性格相性
#define PT_CFG_SOC_GIFT_BOND_OTHER       3
#define PT_CFG_SOC_INTERACT_DAY_CAP      3
#define PT_CFG_SOC_INTERACT_COOLDOWN_MIN 120
#define PT_CFG_SOC_DECAY_GRACE_DAYS      3    // 3 天无互动后 -5/天
#define PT_CFG_SOC_DECAY_PER_DAY         5
#define PT_CFG_SOC_LOVE_BOND             80
#define PT_CFG_SOC_PROPOSE_LOCK_MIN      (48 * 60)
#define PT_CFG_SOC_RING_SIMPLE_COINS     500
#define PT_CFG_SOC_RING_DIAMOND_COINS    5000
#define PT_CFG_SOC_ACCEPT_AT_LOVE_PCT    40   // bond=80 基准接受率
#define PT_CFG_SOC_ACCEPT_PER_BOND_PCT    2   // 每超 80 一点 +2%
#define PT_CFG_SOC_ACCEPT_DIAMOND_PCT    25
#define PT_CFG_SOC_ACCEPT_CAP_PCT        95
#define PT_CFG_SOC_COHABIT_MIN           (24 * 60)

// ---- P2-S3b 育儿/世代交替/家谱（08 §4–§5） ----
#define PT_CFG_FAMILY_NIGHT_DECAY_PCT    90   // 父母同住：夜间衰减 ×90%
#define PT_CFG_FAMILY_CALL_BONUS_MIN     15   // 呼叫窗口 +15 分钟
#define PT_CFG_FAMILY_SHELL_REWARD       10   // 成家奖励贝壳券（婚礼时）
#define PT_CFG_FAMILY_COINS_LEAVE_PCT    50   // 父母离队：50% 储蓄留给子代
#define PT_CFG_GEN_RARE_BOOST_PERMILLE   20   // 每代稀有度提升（高世代加权）
#define PT_CFG_GEN_RARE_BOOST_CAP        200

// ---- 事件队列 ----
#define PT_EVENTS_CAP 32

// ---- 阶段时长保护 ----
#define PT_CFG_SICK_GRACE_MIN       20      // 进入阶段后至少这么久不发病
#define PT_CFG_TEEN_MORNING_SLACK_MIN 180   // 少年期至少撑到次日晨窗

// ---- 离线结算 ----
#define PT_CFG_OFFLINE_CAP_MIN      (48 * 60)
#define PT_CFG_OFFLINE_LONG_SMALL_CAP 2
#define PT_CFG_OFFLINE_FLOOR_FULL   20
#define PT_CFG_OFFLINE_FLOOR_HAPPY  20
#define PT_CFG_OFFLINE_FLOOR_HEALTH 30
#define PT_CFG_OFFLINE_WEIGHT_LOSS  2
#define PT_CFG_SENIOR_AFTER_DAYS    10
