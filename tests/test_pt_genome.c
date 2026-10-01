// 主机测试：P2-S1 L2 基因与遗传内核（designs 05 §4/§6/§9）。
// 纯逻辑，无 ESP-IDF 依赖。白盒包含 .c 以直接对来源概率表做分布断言。
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../main/pet_core/pt_genome.c"
#include "pt_rng.h"

#define N_BIG 20000u

static pt_genome_t make_genome(uint8_t body, uint8_t eyes, uint8_t face,
                               uint8_t head, uint8_t palette, uint8_t back,
                               uint8_t personality, uint8_t flags)
{
    pt_genome_t g = { body, eyes, face, head, palette, back, personality, flags };
    return g;
}

// 取目录内某稀有度的第 k 个部件（构造测试用父母/祖辈）。
static uint8_t part_of(pt_gene_slot_t slot, pt_rarity_t rar, uint8_t k)
{
    uint8_t seen = 0;
    for (uint8_t i = 0; i < pt_slot_part_count(slot); i += 1) {
        if (pt_catalog_rarity(slot, i) == rar) {
            if (seen == k) {
                return pt_part_make(i, rar);
            }
            seen += 1;
        }
    }
    return pt_part_make(0, PT_RAR_C);
}

static void test_packing_and_catalog(void)
{
    assert(pt_part_make(11, PT_RAR_L) == 0xCB);
    assert(pt_part_index(0x4B) == 11);
    assert(pt_part_rarity(0xCB) == PT_RAR_L);

    assert(pt_slot_part_count(PT_GENE_SLOT_BODY) == 8);
    assert(pt_slot_part_count(PT_GENE_SLOT_EYES) == 12);
    assert(pt_slot_part_count(PT_GENE_SLOT_FACE) == 8);
    assert(pt_slot_part_count(PT_GENE_SLOT_HEAD) == 12);
    assert(pt_slot_part_count(PT_GENE_SLOT_PALETTE) == 16);
    assert(pt_slot_part_count(PT_GENE_SLOT_BACK) == 8);

    // 打包稀有度必须与目录一致（脏数据防护）。
    assert(pt_slot_part_valid(PT_GENE_SLOT_BODY, pt_part_make(0, PT_RAR_C)));
    assert(!pt_slot_part_valid(PT_GENE_SLOT_BODY, pt_part_make(0, PT_RAR_L)));
    assert(pt_slot_part_valid(PT_GENE_SLOT_HEAD, pt_part_make(11, PT_RAR_L)));
    assert(!pt_slot_part_valid(PT_GENE_SLOT_HEAD, 0xFF));
    assert(!pt_slot_part_valid(PT_GENE_SLOT_HEAD, pt_part_make(12, PT_RAR_C)));
    assert(pt_catalog_affinity(PT_GENE_SLOT_HEAD, 11) == PT_AFF_TAG_SPECIAL);
    assert(pt_catalog_affinity(PT_GENE_SLOT_BACK, 7) == 0);
}

static void test_roll_is_legal_and_weighted(void)
{
    uint32_t rng = 12345;
    uint32_t rar_cnt[PT_GENE_SLOT_COUNT][PT_RAR_COUNT] = { 0 };
    for (uint32_t i = 0; i < N_BIG; i += 1) {
        pt_genome_t g;
        pt_genome_roll(&g, 0, 0, &rng);
        assert(pt_genome_validate(&g));
        for (uint8_t s = 0; s < PT_GENE_SLOT_COUNT; s += 1) {
            // 直接检查槽位字段
            uint8_t parts[6] = { g.body, g.eyes, g.face, g.head, g.palette, g.back };
            rar_cnt[s][pt_part_rarity(parts[s])] += 1;
        }
        assert((g.flags & PT_GF_FIRST_EGG) == 0);
    }
    // BACK 四档齐全：70/22/7/1（±2%；1% 档用有符号比较避免下溢）。
    static const int WANT[PT_RAR_COUNT] = { 70, 22, 7, 1 };
    for (uint32_t r = 0; r < PT_RAR_COUNT; r += 1) {
        int pct = (int) (rar_cnt[PT_GENE_SLOT_BACK][r] * 100u / N_BIG);
        assert(pct >= WANT[r] - 2 && pct <= WANT[r] + 2);
    }
    // BODY/FACE 没有传说件：永远不出 L（空池向下降档）。
    assert(rar_cnt[PT_GENE_SLOT_BODY][PT_RAR_L] == 0);
    assert(rar_cnt[PT_GENE_SLOT_FACE][PT_RAR_L] == 0);

    // 首发蛋带首发标记。
    uint32_t r2 = 7;
    pt_genome_t first;
    pt_genome_init_first(&first, &r2);
    assert((first.flags & PT_GF_FIRST_EGG) != 0);
    assert(pt_genome_validate(&first));

    // 亲和：allowed=0 时限定头饰永不出现。
    uint32_t r3 = 999;
    for (uint32_t i = 0; i < N_BIG; i += 1) {
        pt_genome_t g;
        pt_genome_roll(&g, 0, 0, &r3);
        assert(!(pt_part_index(g.head) == 11
                 && pt_part_rarity(g.head) == PT_RAR_L));
    }
}

// 直接对来源掷骰表做分布断言（05 §4，误差 ≤2%）。
static void test_source_tables(void)
{
    static const struct {
        const uint16_t *tbl;
        int want[5];
    } cases[3] = {
        { TBL_NORMAL, { 40, 40, 12, 7, 1 } },
        { TBL_DOM,    { 45, 45, 6, 3, 0 } },   // 0.5% 四舍五入归 0/1 都接受
        { TBL_PAL,    { 30, 30, 20, 15, 5 } },
    };
    for (int c = 0; c < 3; c += 1) {
        uint32_t cnt[5] = { 0 };
        uint32_t rng = (uint32_t) (0x1000 + c);
        for (uint32_t i = 0; i < N_BIG; i += 1) {
            cnt[roll_source(cases[c].tbl, &rng)] += 1;
        }
        for (int k = 0; k < 5; k += 1) {
            int pct = (int) (cnt[k] * 100u / N_BIG);
            int w = (int) cases[c].want[k];
            if (w == 0) {
                assert(pct <= 2);
            } else {
                assert(pct >= w - 2 && pct <= w + 2);
            }
        }
    }
}

// 黑盒育种：父母/祖辈部件互不相同（PALETTE 槽 16 件够分），按结果分类计数。
static void test_breed_distribution_palette(void)
{
    // 母 0 号、父 1 号、8 个祖辈全部 2 号；其余 3..15 只可能来自突变。
    uint8_t par[PT_GENE_SLOT_COUNT];
    for (uint8_t s = 0; s < PT_GENE_SLOT_COUNT; s += 1) {
        par[s] = part_of((pt_gene_slot_t) s, PT_RAR_C, 0);
    }
    pt_genome_t mother = make_genome(par[0], par[1], par[2], par[3], par[4],
                                     par[5], PT_PERS_TIMID, 0);
    // 父亲取每槽第 2 个 C 件
    uint8_t dad[PT_GENE_SLOT_COUNT];
    for (uint8_t s = 0; s < PT_GENE_SLOT_COUNT; s += 1) {
        dad[s] = part_of((pt_gene_slot_t) s, PT_RAR_C, 1);
    }
    pt_genome_t father = make_genome(dad[0], dad[1], dad[2], dad[3], dad[4],
                                     dad[5], PT_PERS_LIVELY, 0);

    uint32_t cnt[5] = { 0 };   // mother/father/anc/mut(other)/mut-family
    uint32_t rbase = 4242;
    for (uint32_t i = 0; i < N_BIG; i += 1) {
        pt_breed_input_t in;
        memset(&in, 0, sizeof(in));
        pt_genome_t anc[8];
        for (uint8_t a = 0; a < 8; a += 1) {
            uint8_t third[PT_GENE_SLOT_COUNT];
            for (uint8_t s = 0; s < PT_GENE_SLOT_COUNT; s += 1) {
                third[s] = part_of((pt_gene_slot_t) s, PT_RAR_C, 2);
            }
            anc[a] = make_genome(third[0], third[1], third[2], third[3],
                                 third[4], third[5], PT_PERS_SASSY, 0);
        }
        memcpy((void *) in.anc, anc, sizeof(anc));
        in.mother = &mother;
        in.father = &father;
        in.anc_present = 0xFF;
        in.diet_tag = 0xFF;
        in.generation = 0;
        in.seed = rbase + i * 2654435761u;
        pt_genome_t child;
        pt_genome_breed(&child, &in);
        assert(pt_genome_validate(&child));

        uint8_t got = child.palette;
        if (got == mother.palette) {
            cnt[0] += 1;
        } else if (got == father.palette) {
            cnt[1] += 1;
        } else if (got == anc[0].palette) {
            cnt[2] += 1;
        } else {
            cnt[3] += 1;
        }
    }
    // 30/30/20 + 20% 突变（突变落到家庭三件上的部分会被计入前三桶，
    // 但每桶误差仍应在 ±2% 内，突变识别下界 ≥ 11%）。
    assert(cnt[0] * 100u / N_BIG >= 28 && cnt[0] * 100u / N_BIG <= 32);
    assert(cnt[1] * 100u / N_BIG >= 28 && cnt[1] * 100u / N_BIG <= 32);
    assert(cnt[2] * 100u / N_BIG >= 18 && cnt[2] * 100u / N_BIG <= 22);
    assert(cnt[3] * 100u / N_BIG >= 11);
}

// 亲和穷举（05 §9.2）：无放行时任何合法模板育种结果都不出现限定部件。
static void test_breed_affinity(void)
{
    pt_genome_t m = make_genome(0, 0, 0, 0, 0, 0, 0, 0);
    pt_genome_t f = m;
    pt_breed_input_t in;
    memset(&in, 0, sizeof(in));
    in.mother = &m;
    in.father = &f;
    in.allowed_tags = 0;
    in.diet_tag = 0xFF;

    for (uint32_t i = 0; i < N_BIG; i += 1) {
        in.seed = 0xABC000 + i;
        pt_genome_t child;
        pt_genome_breed(&child, &in);
        assert(pt_genome_validate(&child));
        assert(pt_catalog_affinity(PT_GENE_SLOT_HEAD,
                                   pt_part_index(child.head)) == 0);
    }

    // 显式放行 SPECIAL 后传说限定头饰可出现（多给种子观察非零）。
    bool saw_special = false;
    in.allowed_tags = PT_AFF_TAG_SPECIAL;
    for (uint32_t i = 0; i < N_BIG; i += 1) {
        in.seed = 0xDEF000 + i;
        pt_genome_t child;
        pt_genome_breed(&child, &in);
        if (pt_catalog_affinity(PT_GENE_SLOT_HEAD,
                                pt_part_index(child.head)) != 0) {
            saw_special = true;
        }
    }
    assert(saw_special);
}

// 纯血：连续 3 代同件 → 置标记；之后 70% 稳定（05 §4 要点 5）。
static void test_pureblood(void)
{
    // 构造"子 == 母 == 外祖母"的三连：母/父同件 X，anc[0] 也是 X。
    uint8_t x = part_of(PT_GENE_SLOT_EYES, PT_RAR_U, 0);
    pt_genome_t m = make_genome(0, x, 0, 0, 0, 0, 0, 0);
    pt_genome_t f = make_genome(0, x, 0, 0, 0, 0, 0, 0);
    pt_genome_t anc[8];
    memset(anc, 0, sizeof(anc));
    anc[0] = m;

    pt_breed_input_t in;
    memset(&in, 0, sizeof(in));
    in.mother = &m;
    in.father = &f;
    in.anc_present = 0x01;
    in.diet_tag = 0xFF;
    memcpy((void *) in.anc, anc, sizeof(anc));

    bool flagged = false;
    for (uint32_t i = 0; i < 200; i += 1) {
        in.seed = 0x5000 + i;
        pt_genome_t child;
        pt_genome_breed(&child, &in);
        if ((child.flags & PT_GF_PURE_EYES) != 0) {
            assert(child.eyes == x);
            flagged = true;
        }
    }
    assert(flagged);   // 三连条件下必然有子代置标记

    // 带纯血标记的母亲：EYES 槽约 70% 稳定为 x（±6% 宽松带，n=5000）。
    pt_genome_t mp = m;
    mp.flags = PT_GF_PURE_EYES;
    pt_genome_t fp = make_genome(0, part_of(PT_GENE_SLOT_EYES, PT_RAR_C, 0), 0, 0,
                                 0, 0, 0, 0);
    in.mother = &mp;
    in.father = &fp;
    in.anc_present = 0;
    uint32_t stable = 0;
    for (uint32_t i = 0; i < 5000; i += 1) {
        in.seed = 0x9000 + i * 40503u;
        pt_genome_t child;
        pt_genome_breed(&child, &in);
        if (child.eyes == x && (child.flags & PT_GF_PURE_EYES) != 0) {
            stable += 1;
        }
    }
    uint32_t pct = stable * 100u / 5000u;
    assert(pct >= 64 && pct <= 76);
}

// 性格 70/20/10：甜食家庭 ~20% 出悠闲；父母性格池外的随机占 ~10%。
static void test_personality(void)
{
    pt_genome_t m = make_genome(0, 0, 0, 0, 0, 0, PT_PERS_TIMID, 0);
    pt_genome_t f = make_genome(0, 0, 0, 0, 0, 0, PT_PERS_LIVELY, 0);
    pt_breed_input_t in;
    memset(&in, 0, sizeof(in));
    in.mother = &m;
    in.father = &f;
    in.diet_tag = 1;   // PT_FOOD_TAG_SWEET

    uint32_t easy = 0;
    uint32_t parent_like = 0;
    for (uint32_t i = 0; i < N_BIG; i += 1) {
        in.seed = 0x7000 + i * 48271u;
        pt_genome_t child;
        pt_genome_breed(&child, &in);
        if (child.personality == PT_PERS_EASYGOING) {
            easy += 1;
        }
        if (child.personality == PT_PERS_TIMID
            || child.personality == PT_PERS_LIVELY) {
            parent_like += 1;
        }
    }
    uint32_t pct = easy * 100u / N_BIG;
    assert(pct >= 18 && pct <= 24);       // 20% 饮食 + 随机 ~1.7%
    assert(parent_like * 100u / N_BIG >= 66);   // 70% 父母池 ±
    assert(strcmp(pt_personality_name(PT_PERS_EASYGOING), "Easygoing") == 0);
}

// 固定父母 + 固定种子可复现（05 §4 要点 4）。
static void test_determinism_and_seed(void)
{
    pt_genome_t m = make_genome(1, 2, 3, 4, 5, 6, PT_PERS_CURIOUS, 0);
    pt_genome_t f = make_genome(6, 5, 4, 3, 2, 1, PT_PERS_CLINGY, 0);
    pt_genome_t anc[8];
    memset(anc, 0, sizeof(anc));
    anc[3] = m;
    anc[7] = f;

    pt_breed_input_t in;
    memset(&in, 0, sizeof(in));
    in.mother = &m;
    in.father = &f;
    in.anc_present = 0x88;
    in.diet_tag = 3;
    in.generation = 2;
    in.seed = pt_genome_child_seed(&m, &f, 2);
    memcpy((void *) in.anc, anc, sizeof(anc));

    pt_genome_t a, b;
    pt_genome_breed(&a, &in);
    pt_genome_breed(&b, &in);
    assert(memcmp(&a, &b, sizeof(a)) == 0);

    in.seed += 1;
    pt_genome_t c;
    pt_genome_breed(&c, &in);
    assert(memcmp(&a, &c, sizeof(a)) != 0);

    // 子种子随代次变化。
    assert(pt_genome_child_seed(&m, &f, 0)
           != pt_genome_child_seed(&m, &f, 1));
}

int main(void)
{
    test_packing_and_catalog();
    test_roll_is_legal_and_weighted();
    test_source_tables();
    test_breed_distribution_palette();
    test_breed_affinity();
    test_pureblood();
    test_personality();
    test_determinism_and_seed();
    printf("test_pt_genome: PASS\n");
    return 0;
}
