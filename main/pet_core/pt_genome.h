// pet_core/pt_genome.h —— L2 基因型与外观部件（designs 05）。
// 纯 C，无 ESP-IDF/LVGL 依赖：部件槽/稀有度/调色板/确定性育种，全部可主机单测。
// 核心原则（05 §1）：外观是"算出来的"——形按部件计数，色走 PALETTE 槽。
#pragma once

#include <stdbool.h>
#include <stdint.h>

// ---- 六个外观槽（05 §2，顺序勿改：存档/测试依赖）----
typedef enum {
    PT_GENE_SLOT_BODY = 0,    // 体型/轮廓
    PT_GENE_SLOT_EYES,        // 眼睛（高显性）
    PT_GENE_SLOT_FACE,        // 嘴/脸饰
    PT_GENE_SLOT_HEAD,        // 头饰/耳朵（高显性）
    PT_GENE_SLOT_PALETTE,     // 配色（高随机）
    PT_GENE_SLOT_BACK,        // 背饰（稀有部件集中）
    PT_GENE_SLOT_COUNT,
} pt_gene_slot_t;

// 稀有度：权重 70/22/7/1（permille 700/220/70/10）。
typedef enum {
    PT_RAR_C = 0,        // 普通
    PT_RAR_U,            // 不凡
    PT_RAR_R,            // 稀有
    PT_RAR_L,            // 传说
    PT_RAR_COUNT,
} pt_rarity_t;

#define PT_RAR_WEIGHT_C   700u
#define PT_RAR_WEIGHT_U   220u
#define PT_RAR_WEIGHT_R   70u
#define PT_RAR_WEIGHT_L   10u

// 六种性格（04 §7）；本期只存储与成年揭晓，软修正后续期次接入。
typedef enum {
    PT_PERS_TIMID = 0,   // 胆小
    PT_PERS_LIVELY,      // 活泼
    PT_PERS_EASYGOING,   // 悠闲
    PT_PERS_CURIOUS,     // 好奇
    PT_PERS_SASSY,       // 傲娇
    PT_PERS_CLINGY,      // 黏人
    PT_PERS_COUNT,
} pt_personality_t;

// 基因型 8 字节（05 §3）。每个部件字节 = 槽内序号（低 6 位）| 稀有度（高 2 位）。
typedef struct {
    uint8_t body;
    uint8_t eyes;
    uint8_t face;
    uint8_t head;
    uint8_t palette;
    uint8_t back;
    uint8_t personality;
    uint8_t flags;        // bit0..5 各槽纯血标记；bit6 突变标记；bit7 首发蛋
} pt_genome_t;

#define PT_GF_PURE_BODY     (1u << PT_GENE_SLOT_BODY)
#define PT_GF_PURE_EYES     (1u << PT_GENE_SLOT_EYES)
#define PT_GF_PURE_FACE     (1u << PT_GENE_SLOT_FACE)
#define PT_GF_PURE_HEAD     (1u << PT_GENE_SLOT_HEAD)
#define PT_GF_PURE_PALETTE  (1u << PT_GENE_SLOT_PALETTE)
#define PT_GF_PURE_BACK     (1u << PT_GENE_SLOT_BACK)
#define PT_GF_PURE_MASK     0x3Fu
#define PT_GF_MUTATED       0x40u
#define PT_GF_FIRST_EGG     0x80u

// 部件亲和标签（05 §2）：冲突部件在遗传期重掷为同族普通件，保证不穿模。
// v1 只启用 1 号位（限定头饰，需调用方显式放行）；位 1..7 留给物种/成年亲和。
#define PT_AFF_TAG_SPECIAL  0x01u

// 部件打包/拆包。
uint8_t pt_part_make(uint8_t index, pt_rarity_t rar);
uint8_t pt_part_index(uint8_t part);
pt_rarity_t pt_part_rarity(uint8_t part);

// 槽目录：部件数目标（v1）8/12/8/12/16/8。
uint8_t pt_slot_part_count(pt_gene_slot_t slot);
// 目录中某槽某序号部件的固有稀有度；越界返回 PT_RAR_COUNT。
pt_rarity_t pt_catalog_rarity(pt_gene_slot_t slot, uint8_t index);
// 部件的亲和标签掩码（0 = 全模板通用）。
uint8_t pt_catalog_affinity(pt_gene_slot_t slot, uint8_t index);
// 完整合法性：序号在目录内且打包稀有度与目录一致。
bool pt_slot_part_valid(pt_gene_slot_t slot, uint8_t part);

// 整个基因型的存档/脏数据校验（性格在枚举内、保留 flags 位不拒绝）。
bool pt_genome_validate(const pt_genome_t *g);

// 调色板方案（PALETTE 槽 16 个）：同一张灰度线稿可染成任意方案。
typedef struct {
    uint32_t main;        // 身体主色（RGB hex，喂 lv_color_hex）
    uint32_t edge;        // 轮廓/手脚
    uint32_t belly;       // 腹白辅色（0 = 该方案不叠腹白）
} pt_palette_t;
const pt_palette_t *pt_genome_palette(uint8_t palette_index);

// 首发蛋：按稀有度权重随机整组基因，置首发蛋标记。rng 为存档确定性随机源。
void pt_genome_init_first(pt_genome_t *g, uint32_t *rng);
// 通用随机基因（NPC/迁移用）：allowed_tags 为调用方接受的亲和位掩码。
void pt_genome_roll(pt_genome_t *g, uint8_t allowed_tags, uint32_t generation,
                    uint32_t *rng);

// ---- 育种（05 §4）----
// 祖辈环：anc[0..3] 母系最近四代、anc[4..7] 父系最近四代；anc_present 位掩码
// 表示哪些位置有效。子代基因只用种子重放，与运行环境无关，可单测。
typedef struct {
    const pt_genome_t *mother;
    const pt_genome_t *father;
    pt_genome_t anc[8];
    uint8_t anc_present;          // bit0..7
    uint8_t allowed_tags;         // 模板接受的亲和位
    uint8_t diet_tag;             // 父母高频喂食标签（pt_food_tag_t；0xFF=无偏好）
    uint32_t generation;
    uint32_t seed;
} pt_breed_input_t;

void pt_genome_breed(pt_genome_t *child, const pt_breed_input_t *in);

// 求偶结果可复现：父母基因组 + 存档代次混合出育种种子。
uint32_t pt_genome_child_seed(const pt_genome_t *mother,
                              const pt_genome_t *father, uint32_t generation);

const char *pt_personality_name(pt_personality_t p);
