// 主机测试：P2-S4 图鉴（物种三级点亮 / 部件三标记 / 纯血链 / 里程碑）。
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pt_config.h"
#include "pt_dex.h"
#include "pt_events.h"
#include "pt_social.h"
#include "pt_types.h"

static pt_genome_t make_genome(uint8_t body_idx, uint8_t eyes_idx,
                               uint8_t face_idx, uint8_t head_idx,
                               uint8_t pal_idx, uint8_t back_idx)
{
    pt_genome_t g;
    memset(&g, 0, sizeof(g));
    g.body = pt_part_make(body_idx, pt_catalog_rarity(PT_GENE_SLOT_BODY,
                                                      body_idx));
    g.eyes = pt_part_make(eyes_idx, pt_catalog_rarity(PT_GENE_SLOT_EYES,
                                                      eyes_idx));
    g.face = pt_part_make(face_idx, pt_catalog_rarity(PT_GENE_SLOT_FACE,
                                                      face_idx));
    g.head = pt_part_make(head_idx, pt_catalog_rarity(PT_GENE_SLOT_HEAD,
                                                      head_idx));
    g.palette = pt_part_make(pal_idx, pt_catalog_rarity(PT_GENE_SLOT_PALETTE,
                                                        pal_idx));
    g.back = pt_part_make(back_idx, pt_catalog_rarity(PT_GENE_SLOT_BACK,
                                                      back_idx));
    return g;
}

int main(void)
{
    // ---- 名册/槽位基础 ----
    assert(pt_dex_roster(0) == PT_SP_TEEN_A);
    assert(pt_dex_roster(7) == PT_SP_ADULT_MOON);
    assert(pt_dex_roster(8) == PT_SP_NONE);
    assert(pt_dex_species_index(PT_SP_ADULT_NEGLECT) == 6);
    assert(pt_dex_species_index(PT_SP_EGG) == 0xFF);
    assert(pt_dex_species_care(PT_SP_ADULT_PERFECT) == PT_CARE_PERFECT);
    assert(pt_dex_species_care(PT_SP_TEEN_C) == PT_CARE_NEGLECT);
    assert(pt_dex_species_care(PT_SP_ADULT_MOON) == PT_CARE_PERFECT);
    assert(pt_dex_slot_offset(PT_GENE_SLOT_BODY) == 0);
    assert(pt_dex_slot_offset(PT_GENE_SLOT_BACK) == 56);
    assert(pt_dex_part_total() == 64);

    // ---- 初始空册 ----
    pt_dex_t d;
    pt_dex_init(&d);
    assert(pt_dex_species_level(&d, PT_SP_TEEN_A) == PT_DEX_LV_NONE);
    assert(pt_dex_species_best_care(&d, PT_SP_TEEN_A) == 0xFF);
    assert(pt_dex_species_seen_count(&d) == 0);
    assert(pt_dex_parts_count(&d, PT_DEX_PART_SEEN) == 0);
    assert(d.oldest_days == 0 && d.claimed == 0);

    // ---- 等级单调提升 ----
    pt_dex_observe_species(&d, PT_SP_TEEN_A, PT_DEX_LV_SEEN);
    assert(pt_dex_species_level(&d, PT_SP_TEEN_A) == PT_DEX_LV_SEEN);
    pt_dex_observe_species(&d, PT_SP_TEEN_A, PT_DEX_LV_MASTERED);
    assert(pt_dex_species_level(&d, PT_SP_TEEN_A) == PT_DEX_LV_MASTERED);
    pt_dex_observe_species(&d, PT_SP_TEEN_A, PT_DEX_LV_SEEN);  // 不降
    assert(pt_dex_species_level(&d, PT_SP_TEEN_A) == PT_DEX_LV_MASTERED);
    // 不在册物种忽略。
    pt_dex_observe_species(&d, PT_SP_EGG, PT_DEX_LV_MASTERED);
    assert(pt_dex_species_seen_count(&d) == 1);

    // ---- 养育计数与最佳 CARE ----
    pt_dex_raise_species(&d, PT_SP_ADULT_GREAT, PT_CARE_NORMAL);
    assert(pt_dex_species_level(&d, PT_SP_ADULT_GREAT) == PT_DEX_LV_RAISED);
    assert(pt_dex_species_raised_count(&d, PT_SP_ADULT_GREAT) == 1);
    pt_dex_raise_species(&d, PT_SP_ADULT_GREAT, PT_CARE_GREAT);
    assert(pt_dex_species_raised_count(&d, PT_SP_ADULT_GREAT) == 2);
    assert(pt_dex_species_best_care(&d, PT_SP_ADULT_GREAT) == PT_CARE_GREAT);
    pt_dex_raise_species(&d, PT_SP_ADULT_GREAT, PT_CARE_PERFECT);  // 该物种不可达更优档，存更小值
    assert(pt_dex_species_best_care(&d, PT_SP_ADULT_GREAT)
           == PT_CARE_PERFECT);

    // ---- 部件三标记 ----
    pt_genome_t g1 = make_genome(0, 0, 0, 0, 0, 0);
    pt_dex_observe_genome(&d, &g1, PT_DEX_PART_OWNED);
    for (uint8_t s = 0; s < PT_GENE_SLOT_COUNT; s += 1) {
        assert(pt_dex_part_has(&d, PT_DEX_PART_OWNED,
                               (pt_gene_slot_t) s, 0));
        assert(pt_dex_part_has(&d, PT_DEX_PART_SEEN,
                               (pt_gene_slot_t) s, 0));
    }
    assert(pt_dex_parts_count(&d, PT_DEX_PART_OWNED) == 6);
    assert(pt_dex_parts_count(&d, PT_DEX_PART_SEEN) == 6);
    pt_genome_t g2 = make_genome(7, 11, 7, 11, 15, 7);
    pt_dex_observe_genome(&d, &g2, PT_DEX_PART_BRED);
    assert(pt_dex_parts_count(&d, PT_DEX_PART_BRED) == 6);
    assert(pt_dex_parts_count(&d, PT_DEX_PART_SEEN) == 12);
    // BRED 不算 OWNED。
    assert(!pt_dex_part_has(&d, PT_DEX_PART_OWNED, PT_GENE_SLOT_BACK, 7));
    assert(pt_dex_part_has(&d, PT_DEX_PART_BRED, PT_GENE_SLOT_BACK, 7));
    // 越界查询安全。
    assert(!pt_dex_part_has(&d, PT_DEX_PART_SEEN, PT_GENE_SLOT_BODY, 8));

    // ---- 寿终记录 ----
    pt_dex_record_age(&d, 24);
    pt_dex_record_age(&d, 18);
    assert(d.oldest_days == 24);

    // ---- 纯血链（hall FIFO，最新在末尾）----
    pt_soc_hall_t hall[3];
    memset(hall, 0, sizeof(hall));
    hall[0].pet = make_genome(3, 1, 0, 0, 0, 0);
    hall[1].pet = make_genome(3, 2, 0, 0, 0, 0);
    hall[2].pet = make_genome(3, 2, 0, 0, 0, 0);
    // BODY：末尾两代 idx3 + 更早 idx3 = 3 连。
    assert(pt_dex_pure_chain(hall, 3, PT_GENE_SLOT_BODY) == 3);
    // EYES：最新两代 idx2，祖父 idx1 → 链 2。
    assert(pt_dex_pure_chain(hall, 3, PT_GENE_SLOT_EYES) == 2);
    // FACE 全 0：链 3。
    assert(pt_dex_pure_chain(hall, 3, PT_GENE_SLOT_FACE) == 3);
    hall[0].pet = make_genome(4, 2, 0, 0, 0, 0);
    // 最新两代 BODY 仍为 idx3 → 2。
    assert(pt_dex_pure_chain(hall, 3, PT_GENE_SLOT_BODY) == 2);
    assert(pt_dex_pure_chain(hall, 1, PT_GENE_SLOT_BODY) == 0);  // 仅一代
    assert(pt_dex_pure_chain(NULL, 3, PT_GENE_SLOT_BODY) == 0);

    // ---- 里程碑轮询：首次养育 → 5 贝壳，只发一次 ----
    uint16_t amount = 0;
    uint8_t fresh = pt_dex_poll_rewards(&d, &amount);
    assert(fresh == PT_DEX_MILE_FIRST_RAISED);
    assert(amount == 5);
    assert((d.claimed & PT_DEX_MILE_FIRST_RAISED) != 0);
    assert(pt_dex_poll_rewards(&d, NULL) == 0);

    // ---- 一次达成多个里程碑：4 物种 + 16/32 部件 ----
    pt_dex_t d2;
    pt_dex_init(&d2);
    pt_dex_observe_species(&d2, PT_SP_TEEN_A, PT_DEX_LV_SEEN);
    pt_dex_observe_species(&d2, PT_SP_TEEN_B, PT_DEX_LV_SEEN);
    pt_dex_observe_species(&d2, PT_SP_TEEN_C, PT_DEX_LV_SEEN);
    pt_dex_observe_species(&d2, PT_SP_ADULT_NORMAL, PT_DEX_LV_SEEN);
    // 直接构造 32 个已见部件（每个字节 4 位 × 8 字节 = 32）。
    for (uint8_t i = 0; i < 8; i += 1) {
        d2.parts_seen[i] = 0x0F;
    }
    amount = 0;
    fresh = pt_dex_poll_rewards(&d2, &amount);
    assert((fresh & PT_DEX_MILE_SPECIES_4) != 0);
    assert((fresh & PT_DEX_MILE_PARTS_16) != 0);
    assert((fresh & PT_DEX_MILE_PARTS_32) != 0);
    assert(!(fresh & PT_DEX_MILE_FIRST_RAISED));
    assert(amount == 10 + 5 + 10);

    // ---- 全收集：8 物种 + 64 部件（无养育也不发 FIRST_RAISED）----
    pt_dex_t d3;
    pt_dex_init(&d3);
    for (uint8_t i = 0; i < PT_DEX_SPECIES; i += 1) {
        pt_dex_observe_species(&d3, pt_dex_roster(i), PT_DEX_LV_SEEN);
    }
    memset(d3.parts_seen, 0xFF, sizeof(d3.parts_seen));
    amount = 0;
    fresh = pt_dex_poll_rewards(&d3, &amount);
    assert((fresh & PT_DEX_MILE_SPECIES_4) != 0);
    assert((fresh & PT_DEX_MILE_SPECIES_ALL) != 0);
    assert((fresh & PT_DEX_MILE_PARTS_ALL) != 0);
    assert(!(fresh & PT_DEX_MILE_FIRST_RAISED));
    // 4/ALL + 16/32/48/ALL = 10+20+5+10+15+30 = 90。
    assert(amount == 90);

    printf("pt_dex: all tests passed\n");
    return 0;
}
