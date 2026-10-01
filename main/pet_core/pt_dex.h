// pet_core/pt_dex.h —— P2-S4 图鉴：物种收藏 + 部件见闻 + 家族徽章（designs 04 §8.3 / 05 §8）。
// 纯逻辑：不碰 NVS/LVGL/经济；贝壳里程碑只产出"新达成位 + 总额"，由 app 层发奖。
// 物种三级点亮：SEEN（见过）→ RAISED（养育成年/养大）→ MASTERED（养老或成家）。
// 部件三标记：SEEN（任何名片上见过）→ OWNED（自家宠携带）→ BRED（育种子代携带）。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pt_genome.h"
#include "pt_social.h"
#include "pt_types.h"

#define PT_DEX_SPECIES 8    // 3 少年 + 4 档成年 + 月影（虹光 P3 前不入册）
#define PT_DEX_CARE_COUNT 4 // pt_care_tier_t 档位数（核心枚举无 COUNT 哨兵）

typedef enum {
    PT_DEX_LV_NONE = 0,    // 未见：剪影隐藏
    PT_DEX_LV_SEEN,        // 见过剪影
    PT_DEX_LV_RAISED,      // 自家养育达成（少年=养大离阶；成年=养成该形态）
    PT_DEX_LV_MASTERED,    // 养至老年或成家
} pt_dex_level_t;

typedef enum {
    PT_DEX_PART_SEEN = 0,
    PT_DEX_PART_OWNED,     // 自家宠基因型携带（含当前一代）
    PT_DEX_PART_BRED,      // 育种产出的子代携带
} pt_dex_part_mark_t;

// 图鉴存档体（35B 逻辑负载；走 PET3 256B blob）。
typedef struct {
    uint8_t level[PT_DEX_SPECIES];       // pt_dex_level_t
    uint8_t raised[PT_DEX_SPECIES];      // 累计养育次数（饱和 255）
    uint8_t best_care[PT_DEX_SPECIES];   // 最佳 CARE 档（0xFF=无；枚举越小越好）
    uint8_t parts_seen[8];               // 64 位全局部件位索引（按槽偏移）
    uint8_t parts_owned[8];
    uint8_t parts_bred[8];
    uint16_t oldest_days;                // 历史最长寿年龄（游戏天）
    uint8_t claimed;                     // 已发奖里程碑位掩码
} pt_dex_t;

// 图鉴名册顺序（编号 1..8）：三少年、四档成年、月影。
pt_species_t pt_dex_roster(uint8_t i);
// 物种在名册中的序号；不在册返回 0xFF。
uint8_t pt_dex_species_index(pt_species_t sp);
// 该物种对应的 CARE 档（少年按其养护倾向，月影=PERFECT）；不在册返回 0xFF。
uint8_t pt_dex_species_care(pt_species_t sp);

// 槽内全局位索引偏移与全局部件总数（v1 目录 8/12/8/12/16/8 = 64）。
uint8_t pt_dex_slot_offset(pt_gene_slot_t slot);
uint8_t pt_dex_part_total(void);

void pt_dex_init(pt_dex_t *d);

// 物种点亮（单调提升等级）；raise 额外累计一次养育并记最佳 CARE。
void pt_dex_observe_species(pt_dex_t *d, pt_species_t sp, pt_dex_level_t lv);
void pt_dex_raise_species(pt_dex_t *d, pt_species_t sp, uint8_t care);
uint8_t pt_dex_species_level(const pt_dex_t *d, pt_species_t sp);
uint8_t pt_dex_species_raised_count(const pt_dex_t *d, pt_species_t sp);
uint8_t pt_dex_species_best_care(const pt_dex_t *d, pt_species_t sp);

// 记录基因组全部 6 个部件（OWNED/BRED 自动包含 SEEN）。
void pt_dex_observe_genome(pt_dex_t *d, const pt_genome_t *g,
                           pt_dex_part_mark_t mark);
bool pt_dex_part_has(const pt_dex_t *d, pt_dex_part_mark_t mark,
                     pt_gene_slot_t slot, uint8_t index);
uint8_t pt_dex_parts_count(const pt_dex_t *d, pt_dex_part_mark_t mark);

// 寿终记录最长寿年龄。
void pt_dex_record_age(pt_dex_t *d, uint16_t age_days);

// 统计：已见物种数 / 养育成年物种数。
uint8_t pt_dex_species_seen_count(const pt_dex_t *d);
uint8_t pt_dex_species_raised_total(const pt_dex_t *d);

// 纯血链：从最近一代（hall FIFO 的末尾）向旧代数，该槽部件序号连续相同
// 的代数；<2 返回 0（不成徽章）。
uint8_t pt_dex_pure_chain(const pt_soc_hall_t *hall, uint8_t hall_count,
                          pt_gene_slot_t slot);

// ---- 图鉴里程碑贝壳奖励（06 §2：图鉴里程碑是贝壳券产出之一）----
enum {
    PT_DEX_MILE_FIRST_RAISED = 1u << 0,  // 首个物种养至 RAISED：5
    PT_DEX_MILE_SPECIES_4   = 1u << 1,   // 见过 4 种：10
    PT_DEX_MILE_SPECIES_ALL = 1u << 2,   // 名册全见过：20
    PT_DEX_MILE_PARTS_16    = 1u << 3,   // 见过 16 个部件：5
    PT_DEX_MILE_PARTS_32    = 1u << 4,   // 32：10
    PT_DEX_MILE_PARTS_48    = 1u << 5,   // 48：15
    PT_DEX_MILE_PARTS_ALL   = 1u << 6,   // 64 件全见：30
};

// 轮询：把本次新达成的里程碑写入 claimed，返回新位掩码；*amount 为对应贝壳
// 总额（NULL 可省）。无新达成返回 0。
uint8_t pt_dex_poll_rewards(pt_dex_t *d, uint16_t *amount);
