// pet_core/pt_genome.c —— 见 pt_genome.h。纯 C，无 ESP-IDF/LVGL 依赖。
#include "pt_genome.h"

#include <stddef.h>

#include "pt_config.h"
#include "pt_rng.h"

// ---- 目录（v1，05 §2）：每槽部件数与逐序号稀有度 ----
static const uint8_t SLOT_COUNT[PT_GENE_SLOT_COUNT] = {
    8,    // BODY
    12,   // EYES
    8,    // FACE
    12,   // HEAD
    16,   // PALETTE
    8,    // BACK
};

// 逐序号稀有度（稀有部件向 BACK/HEAD 集中，05 §2）。
static const uint8_t CAT_BODY[8]  = {0,0,0,0,0,1,1,2};
static const uint8_t CAT_EYES[12] = {0,0,0,0,0,0,0,1,1,1,2,3};
static const uint8_t CAT_FACE[8]  = {0,0,0,0,0,1,1,2};
static const uint8_t CAT_HEAD[12] = {0,0,0,0,0,0,0,1,1,1,2,3};
static const uint8_t CAT_PAL[16]  = {0,0,0,0,0,0,0,0,0,1,1,1,1,2,2,3};
static const uint8_t CAT_BACK[8]  = {0,0,0,1,1,1,2,3};

static const uint8_t *const CATALOG[PT_GENE_SLOT_COUNT] = {
    CAT_BODY, CAT_EYES, CAT_FACE, CAT_HEAD, CAT_PAL, CAT_BACK,
};

// 16 个配色方案（主色/轮廓/腹白）。占位期供几何渲染染色；S2 灰度线稿上线后
// 同一组方案进 256 项 LUT（05 §5.2）。
static const pt_palette_t PALETTES[16] = {
    { 0xF4F6FA, 0xC8CFDD, 0xFFFFFF }, // 0  雪团白
    { 0xFFE7B0, 0xE5B86B, 0xFFF8E8 }, // 1  奶油黄
    { 0xFFC2C2, 0xE08585, 0xFFEEEE }, // 2  蜜桃粉
    { 0xF6A8D6, 0xC86FAE, 0xFFEAF7 }, // 3  樱花
    { 0xC8A2F0, 0x8E63C9, 0xF1E7FF }, // 4  葡萄紫
    { 0x9FB8F5, 0x5F7FD0, 0xE8EEFF }, // 5  晴空蓝
    { 0x8FE3E0, 0x4FB6B2, 0xE2FBFA }, // 6  薄荷青
    { 0xB8E89A, 0x7CB45E, 0xF0FCE4 }, // 7  新芽绿
    { 0xD9E8B0, 0xA6BC6B, 0xF7FCE6 }, // 8  抹茶
    { 0xF5C18E, 0xCC8850, 0xFFF0DE }, // 9  南瓜橙
    { 0xE8A48A, 0xBE7158, 0xFFEADF }, // 10 珊瑚
    { 0xC99A6E, 0x96683F, 0xEFD9C2 }, // 11 可可棕
    { 0x8A8F9E, 0x5C6172, 0xD6D9E2 }, // 12 炭灰
    { 0x5F6675, 0x3D424E, 0x9BA1B0 }, // 13 煤球黑
    { 0x7BE0C9, 0x3FB596, 0xDCFFF7 }, // 14 翡翠
    { 0xF2E8B5, 0xD4C584, 0xFFFDF2 }, // 15 月光金
};

uint8_t pt_part_make(uint8_t index, pt_rarity_t rar)
{
    return (uint8_t) ((index & 0x3Fu) | ((uint8_t) rar << 6));
}

uint8_t pt_part_index(uint8_t part)
{
    return (uint8_t) (part & 0x3Fu);
}

pt_rarity_t pt_part_rarity(uint8_t part)
{
    return (pt_rarity_t) (part >> 6);
}

uint8_t pt_slot_part_count(pt_gene_slot_t slot)
{
    if ((unsigned) slot >= PT_GENE_SLOT_COUNT) {
        return 0;
    }
    return SLOT_COUNT[slot];
}

pt_rarity_t pt_catalog_rarity(pt_gene_slot_t slot, uint8_t index)
{
    if ((unsigned) slot >= PT_GENE_SLOT_COUNT || index >= SLOT_COUNT[slot]) {
        return PT_RAR_COUNT;
    }
    return (pt_rarity_t) CATALOG[slot][index];
}

uint8_t pt_catalog_affinity(pt_gene_slot_t slot, uint8_t index)
{
    // v1：传说限定头饰（HEAD 槽末位）需 SPECIAL 放行，其余部件全模板通用。
    if (slot == PT_GENE_SLOT_HEAD && index == SLOT_COUNT[PT_GENE_SLOT_HEAD] - 1) {
        return PT_AFF_TAG_SPECIAL;
    }
    return 0;
}

bool pt_slot_part_valid(pt_gene_slot_t slot, uint8_t part)
{
    uint8_t idx = pt_part_index(part);
    pt_rarity_t packed = pt_part_rarity(part);
    return (unsigned) slot < PT_GENE_SLOT_COUNT
           && idx < SLOT_COUNT[slot]
           && (pt_rarity_t) CATALOG[slot][idx] == packed;
}

bool pt_genome_validate(const pt_genome_t *g)
{
    if (g == NULL) {
        return false;
    }
    const uint8_t parts[PT_GENE_SLOT_COUNT] = {
        g->body, g->eyes, g->face, g->head, g->palette, g->back,
    };
    for (uint8_t s = 0; s < PT_GENE_SLOT_COUNT; s += 1) {
        if (!pt_slot_part_valid((pt_gene_slot_t) s, parts[s])) {
            return false;
        }
    }
    return g->personality < PT_PERS_COUNT;
}

const pt_palette_t *pt_genome_palette(uint8_t palette_index)
{
    if (palette_index >= 16u) {
        return NULL;
    }
    return &PALETTES[palette_index];
}

// 700/220/70/10 → 稀有度。
static pt_rarity_t roll_rarity(uint32_t *rng)
{
    uint32_t r = pt_rng_below(rng, 1000u);
    if (r < PT_RAR_WEIGHT_C) {
        return PT_RAR_C;
    }
    if (r < PT_RAR_WEIGHT_C + PT_RAR_WEIGHT_U) {
        return PT_RAR_U;
    }
    if (r < PT_RAR_WEIGHT_C + PT_RAR_WEIGHT_U + PT_RAR_WEIGHT_R) {
        return PT_RAR_R;
    }
    return PT_RAR_L;
}

// 在槽内取指定稀有度的随机部件；该稀有度池为空时向下降档（如 BODY 无 L）。
static uint8_t roll_part(pt_gene_slot_t slot, pt_rarity_t rar, uint32_t *rng)
{
    for (int r = (int) rar; r >= 0; r -= 1) {
        uint8_t pool[16];
        uint8_t n = 0;
        for (uint8_t i = 0; i < SLOT_COUNT[slot]; i += 1) {
            if (CATALOG[slot][i] == (uint8_t) r) {
                pool[n++] = i;
            }
        }
        if (n > 0) {
            return pt_part_make(pool[pt_rng_below(rng, n)],
                                (pt_rarity_t) r);
        }
    }
    return pt_part_make(0, PT_RAR_C);   // 理论不可达：每槽必有 C 件
}

// 亲和不合法 → 重掷为同族（同槽）通用普通件（05 §2）。
static uint8_t legalize(pt_gene_slot_t slot, uint8_t part,
                        uint8_t allowed_tags, uint32_t *rng)
{
    uint8_t idx = pt_part_index(part);
    uint8_t need = pt_catalog_affinity(slot, idx);
    if ((need & ~allowed_tags) == 0u) {
        return part;
    }
    uint8_t pool[16];
    uint8_t n = 0;
    for (uint8_t i = 0; i < SLOT_COUNT[slot]; i += 1) {
        if (CATALOG[slot][i] == PT_RAR_C && pt_catalog_affinity(slot, i) == 0) {
            pool[n++] = i;
        }
    }
    if (n == 0) {
        return pt_part_make(0, PT_RAR_C);
    }
    return pt_part_make(pool[pt_rng_below(rng, n)], PT_RAR_C);
}

static uint8_t genome_slot(const pt_genome_t *g, pt_gene_slot_t slot)
{
    switch (slot) {
    case PT_GENE_SLOT_BODY:    return g->body;
    case PT_GENE_SLOT_EYES:    return g->eyes;
    case PT_GENE_SLOT_FACE:    return g->face;
    case PT_GENE_SLOT_HEAD:    return g->head;
    case PT_GENE_SLOT_PALETTE: return g->palette;
    case PT_GENE_SLOT_BACK:    return g->back;
    default:              return 0;
    }
}

static void genome_set_slot(pt_genome_t *g, pt_gene_slot_t slot, uint8_t part)
{
    switch (slot) {
    case PT_GENE_SLOT_BODY:    g->body = part; break;
    case PT_GENE_SLOT_EYES:    g->eyes = part; break;
    case PT_GENE_SLOT_FACE:    g->face = part; break;
    case PT_GENE_SLOT_HEAD:    g->head = part; break;
    case PT_GENE_SLOT_PALETTE: g->palette = part; break;
    case PT_GENE_SLOT_BACK:    g->back = part; break;
    default:              break;
    }
}

// 05 §4 来源表（permille）：父/母/祖辈/同稀有度突变/升档突变。
#define SRC_F       0u
#define SRC_M       1u
#define SRC_ANC     2u
#define SRC_MUT_S   3u
#define SRC_MUT_UP  4u

static const uint16_t TBL_NORMAL[5] = { 400, 400, 120, 70, 10 };
static const uint16_t TBL_DOM[5]    = { 450, 450, 60, 35, 5 };
static const uint16_t TBL_PAL[5]    = { 300, 300, 200, 150, 50 };

static uint8_t roll_source(const uint16_t *tbl, uint32_t *rng)
{
    uint32_t r = pt_rng_below(rng, 1000u);
    uint16_t acc = 0;
    for (uint8_t i = 0; i < 5; i += 1) {
        acc = (uint16_t) (acc + tbl[i]);
        if (r < acc) {
            return i;
        }
    }
    return SRC_MUT_UP;
}

static pt_personality_t personality_roll(const pt_breed_input_t *in,
                                         uint32_t *rng)
{
    uint32_t r = pt_rng_below(rng, 1000u);
    if (r < 700) {
        // 70% 父母性格池。
        const pt_genome_t *p = pt_rng_below(rng, 2) == 0
                                   ? in->mother : in->father;
        if (p != NULL && p->personality < PT_PERS_COUNT) {
            return (pt_personality_t) p->personality;
        }
    } else if (r < 900 && in->diet_tag <= 4u) {
        // 20% 饮食标签（04 §7：甜 → 悠闲；肉 → 活泼；果蔬 → 温柔黏人；咸 → 好奇）。
        switch (in->diet_tag) {
        case 1:  return PT_PERS_EASYGOING;
        case 4:  return PT_PERS_LIVELY;
        case 3:  return PT_PERS_CLINGY;
        case 2:  return PT_PERS_CURIOUS;
        default: return PT_PERS_TIMID;
        }
    }
    return (pt_personality_t) pt_rng_below(rng, PT_PERS_COUNT);
}

void pt_genome_breed(pt_genome_t *child, const pt_breed_input_t *in)
{
    uint32_t rng = in->seed;
    pt_genome_t zero = { 0 };   // 空父母按全 C 号部件，不影响概率口径
    const pt_genome_t *m = in->mother != NULL ? in->mother : &zero;
    const pt_genome_t *f = in->father != NULL ? in->father : &zero;

    uint8_t flags = 0;
    bool mutated = false;

    for (uint8_t s = 0; s < PT_GENE_SLOT_COUNT; s += 1) {
        pt_gene_slot_t slot = (pt_gene_slot_t) s;
        uint8_t pure = 0xFFu;
        if ((m->flags & (1u << s)) != 0u) {
            pure = genome_slot(m, slot);
        } else if ((f->flags & (1u << s)) != 0u) {
            pure = genome_slot(f, slot);
        }

        bool from_mutation = false;
        uint8_t cand;

        // 纯血：70% 稳定遗传（05 §4 要点 5）。
        if (pure != 0xFFu && pt_rng_chance_permille(&rng, 700)) {
            cand = pure;
            flags |= (uint8_t) (1u << s);
        } else {
            const uint16_t *tbl = slot == PT_GENE_SLOT_EYES
                                      || slot == PT_GENE_SLOT_HEAD ? TBL_DOM
                                  : slot == PT_GENE_SLOT_PALETTE ? TBL_PAL
                                                            : TBL_NORMAL;
            uint8_t src = roll_source(tbl, &rng);
            switch (src) {
            case SRC_F:
                cand = genome_slot(f, slot);
                break;
            case SRC_M:
                cand = genome_slot(m, slot);
                break;
            case SRC_ANC: {
                // 在场祖辈等概率（4 代环形缓冲在 S3 维护）。
                uint8_t present[8];
                uint8_t n = 0;
                for (uint8_t i = 0; i < 8; i += 1) {
                    if ((in->anc_present & (1u << i)) != 0u) {
                        present[n++] = i;
                    }
                }
                cand = n > 0 ? genome_slot(&in->anc[present[pt_rng_below(&rng, n)]],
                                           slot)
                             : genome_slot(m, slot);
                break;
            }
            case SRC_MUT_UP:
                // 稀有度 +1 档（05 §4）：先按全局权重掷档再 +1，L 封顶；
                // 该档池为空时 roll_part 自动向下降档（如 BODY 无传说件）。
                from_mutation = true;
                {
                    pt_rarity_t base = roll_rarity(&rng);
                    pt_rarity_t up = (pt_rarity_t) ((base + 1 >= PT_RAR_COUNT)
                                                        ? PT_RAR_L : base + 1);
                    cand = roll_part(slot, up, &rng);
                }
                break;
            default:   // SRC_MUT_S
                from_mutation = true;
                cand = roll_part(slot, roll_rarity(&rng), &rng);
                break;
            }
            cand = legalize(slot, cand, in->allowed_tags, &rng);
            if (from_mutation) {
                mutated = true;
            }
            // 连续 3 代同部件（子→母→外祖母 或 子→父→祖父）→ 置纯血标记。
            uint8_t mv = genome_slot(m, slot);
            uint8_t fv = genome_slot(f, slot);
            if (cand == mv && (in->anc_present & 0x01u) != 0u
                && genome_slot(&in->anc[0], slot) == mv) {
                flags |= (uint8_t) (1u << s);
            } else if (cand == fv && (in->anc_present & 0x10u) != 0u
                       && genome_slot(&in->anc[4], slot) == fv) {
                flags |= (uint8_t) (1u << s);
            }
        }
        genome_set_slot(child, slot, cand);
    }

    child->personality = (uint8_t) personality_roll(in, &rng);
    child->flags = flags | (mutated ? PT_GF_MUTATED : 0);
}

uint32_t pt_genome_child_seed(const pt_genome_t *mother,
                              const pt_genome_t *father, uint32_t generation)
{
    uint8_t buf[20];
    for (uint8_t i = 0; i < 8; i += 1) {
        buf[i] = ((const uint8_t *) mother)[i];
        buf[8 + i] = ((const uint8_t *) father)[i];
    }
    for (uint8_t i = 0; i < 4; i += 1) {
        buf[16 + i] = (uint8_t) (generation >> (8 * i));
    }
    return pt_hash32(buf, sizeof(buf));
}

static pt_personality_t roll_personality(uint32_t *rng)
{
    return (pt_personality_t) pt_rng_below(rng, PT_PERS_COUNT);
}

// 高世代加权（08 §3.1）：每代 +20‰ 概率把本次稀有度提升一档（L 封顶）。
static pt_rarity_t roll_rarity_gen(uint32_t *rng, uint32_t generation)
{
    pt_rarity_t r = roll_rarity(rng);
    if (generation > 0u && r != PT_RAR_L) {
        uint32_t p = generation * PT_CFG_GEN_RARE_BOOST_PERMILLE;
        if (p > PT_CFG_GEN_RARE_BOOST_CAP) {
            p = PT_CFG_GEN_RARE_BOOST_CAP;
        }
        if (pt_rng_chance_permille(rng, p)) {
            r = (pt_rarity_t) (r + 1);
        }
    }
    return r;
}

void pt_genome_roll(pt_genome_t *g, uint8_t allowed_tags, uint32_t generation,
                    uint32_t *rng)
{
    uint8_t flags = 0;
    for (uint8_t s = 0; s < PT_GENE_SLOT_COUNT; s += 1) {
        uint8_t part = roll_part((pt_gene_slot_t) s,
                                 roll_rarity_gen(rng, generation), rng);
        part = legalize((pt_gene_slot_t) s, part, allowed_tags, rng);
        genome_set_slot(g, (pt_gene_slot_t) s, part);
    }
    g->personality = (uint8_t) roll_personality(rng);
    g->flags = flags;
}

void pt_genome_init_first(pt_genome_t *g, uint32_t *rng)
{
    pt_genome_roll(g, 0, 0, rng);
    g->flags |= PT_GF_FIRST_EGG;
}

const char *pt_personality_name(pt_personality_t p)
{
    static const char *const NAMES[PT_PERS_COUNT] = {
        "Timid", "Lively", "Easygoing", "Curious", "Sassy", "Clingy",
    };
    if ((unsigned) p >= PT_PERS_COUNT) {
        return "?";
    }
    return NAMES[p];
}
