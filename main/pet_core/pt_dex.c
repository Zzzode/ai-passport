// pet_core/pt_dex.c —— P2-S4 图鉴纯逻辑（designs 04 §8.3 / 05 §8）。
#include "pt_dex.h"

#include <string.h>

static const pt_species_t s_roster[PT_DEX_SPECIES] = {
    PT_SP_TEEN_A, PT_SP_TEEN_B, PT_SP_TEEN_C,
    PT_SP_ADULT_PERFECT, PT_SP_ADULT_GREAT, PT_SP_ADULT_NORMAL,
    PT_SP_ADULT_NEGLECT, PT_SP_ADULT_MOON,
};

static const uint8_t s_care[PT_DEX_SPECIES] = {
    PT_CARE_GREAT,    // 糯糯：整洁清秀型（CARE 好）
    PT_CARE_NORMAL,   // 咕咕：普通型
    PT_CARE_NEGLECT,  // 毛毛：潦草炸毛型（CARE 差）
    PT_CARE_PERFECT,
    PT_CARE_GREAT,
    PT_CARE_NORMAL,
    PT_CARE_NEGLECT,
    PT_CARE_PERFECT,  // 月影（隐藏，条件走 PERFECT）
};

static uint8_t s_offset[PT_GENE_SLOT_COUNT + 1];

static void ensure_offsets(void)
{
    static bool done = false;
    if (done) {
        return;
    }
    done = true;
    s_offset[0] = 0;
    for (int s = 0; s < PT_GENE_SLOT_COUNT; s += 1) {
        s_offset[s + 1] = (uint8_t) (s_offset[s]
                                     + pt_slot_part_count((pt_gene_slot_t) s));
    }
}

pt_species_t pt_dex_roster(uint8_t i)
{
    return i < PT_DEX_SPECIES ? s_roster[i] : PT_SP_NONE;
}

uint8_t pt_dex_species_index(pt_species_t sp)
{
    for (uint8_t i = 0; i < PT_DEX_SPECIES; i += 1) {
        if (s_roster[i] == sp) {
            return i;
        }
    }
    return 0xFFu;
}

uint8_t pt_dex_species_care(pt_species_t sp)
{
    uint8_t i = pt_dex_species_index(sp);
    return i < PT_DEX_SPECIES ? s_care[i] : 0xFFu;
}

uint8_t pt_dex_slot_offset(pt_gene_slot_t slot)
{
    ensure_offsets();
    return (uint8_t) slot < PT_GENE_SLOT_COUNT ? s_offset[slot] : 0;
}

uint8_t pt_dex_part_total(void)
{
    ensure_offsets();
    return s_offset[PT_GENE_SLOT_COUNT];
}

void pt_dex_init(pt_dex_t *d)
{
    memset(d, 0, sizeof(*d));
    for (uint8_t i = 0; i < PT_DEX_SPECIES; i += 1) {
        d->best_care[i] = 0xFFu;
    }
}

static uint8_t roster_idx(pt_species_t sp)
{
    return pt_dex_species_index(sp);
}

void pt_dex_observe_species(pt_dex_t *d, pt_species_t sp, pt_dex_level_t lv)
{
    uint8_t i = roster_idx(sp);
    if (i >= PT_DEX_SPECIES || lv <= PT_DEX_LV_NONE
        || lv > PT_DEX_LV_MASTERED) {
        return;
    }
    if (d->level[i] < (uint8_t) lv) {
        d->level[i] = (uint8_t) lv;
    }
}

void pt_dex_raise_species(pt_dex_t *d, pt_species_t sp, uint8_t care)
{
    uint8_t i = roster_idx(sp);
    if (i >= PT_DEX_SPECIES) {
        return;
    }
    if (d->level[i] < PT_DEX_LV_RAISED) {
        d->level[i] = PT_DEX_LV_RAISED;
    }
    if (d->raised[i] < 255) {
        d->raised[i] += 1;
    }
    if (care < PT_DEX_CARE_COUNT
        && (d->best_care[i] >= PT_DEX_CARE_COUNT || care < d->best_care[i])) {
        d->best_care[i] = care;
    }
}

uint8_t pt_dex_species_level(const pt_dex_t *d, pt_species_t sp)
{
    uint8_t i = roster_idx(sp);
    return i < PT_DEX_SPECIES ? d->level[i] : PT_DEX_LV_NONE;
}

uint8_t pt_dex_species_raised_count(const pt_dex_t *d, pt_species_t sp)
{
    uint8_t i = roster_idx(sp);
    return i < PT_DEX_SPECIES ? d->raised[i] : 0;
}

uint8_t pt_dex_species_best_care(const pt_dex_t *d, pt_species_t sp)
{
    uint8_t i = roster_idx(sp);
    return (i < PT_DEX_SPECIES && d->best_care[i] < PT_DEX_CARE_COUNT)
        ? d->best_care[i] : 0xFFu;
}

static uint8_t part_byte_of(const pt_genome_t *g, pt_gene_slot_t slot)
{
    switch (slot) {
    case PT_GENE_SLOT_BODY:    return g->body;
    case PT_GENE_SLOT_EYES:    return g->eyes;
    case PT_GENE_SLOT_FACE:    return g->face;
    case PT_GENE_SLOT_HEAD:    return g->head;
    case PT_GENE_SLOT_PALETTE: return g->palette;
    case PT_GENE_SLOT_BACK:    return g->back;
    default:                   return 0;
    }
}

static void bitset_set(uint8_t *bits, uint8_t idx)
{
    bits[idx >> 3] |= (uint8_t) (1u << (idx & 7));
}

static bool bitset_get(const uint8_t *bits, uint8_t idx)
{
    return (bits[idx >> 3] & (uint8_t) (1u << (idx & 7))) != 0;
}

void pt_dex_observe_genome(pt_dex_t *d, const pt_genome_t *g,
                           pt_dex_part_mark_t mark)
{
    if (g == NULL) {
        return;
    }
    ensure_offsets();
    for (uint8_t s = 0; s < PT_GENE_SLOT_COUNT; s += 1) {
        uint8_t part = part_byte_of(g, (pt_gene_slot_t) s);
        uint8_t index = pt_part_index(part);
        if (index >= pt_slot_part_count((pt_gene_slot_t) s)) {
            continue;
        }
        uint8_t bit = (uint8_t) (s_offset[s] + index);
        if (bit >= 64) {
            continue;
        }
        bitset_set(d->parts_seen, bit);
        if (mark == PT_DEX_PART_OWNED) {
            bitset_set(d->parts_owned, bit);
        } else if (mark == PT_DEX_PART_BRED) {
            bitset_set(d->parts_bred, bit);
        }
    }
}

bool pt_dex_part_has(const pt_dex_t *d, pt_dex_part_mark_t mark,
                     pt_gene_slot_t slot, uint8_t index)
{
    ensure_offsets();
    if ((uint8_t) slot >= PT_GENE_SLOT_COUNT
        || index >= pt_slot_part_count(slot)) {
        return false;
    }
    uint8_t bit = (uint8_t) (s_offset[slot] + index);
    const uint8_t *bits = mark == PT_DEX_PART_OWNED ? d->parts_owned
                          : mark == PT_DEX_PART_BRED ? d->parts_bred
                                                     : d->parts_seen;
    return bitset_get(bits, bit);
}

static uint8_t bitset_popcount(const uint8_t *bits, uint8_t total)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < total; i += 1) {
        if (bitset_get(bits, i)) {
            n += 1;
        }
    }
    return n;
}

uint8_t pt_dex_parts_count(const pt_dex_t *d, pt_dex_part_mark_t mark)
{
    const uint8_t *bits = mark == PT_DEX_PART_OWNED ? d->parts_owned
                          : mark == PT_DEX_PART_BRED ? d->parts_bred
                                                     : d->parts_seen;
    return bitset_popcount(bits, pt_dex_part_total());
}

void pt_dex_record_age(pt_dex_t *d, uint16_t age_days)
{
    if (age_days > d->oldest_days) {
        d->oldest_days = age_days;
    }
}

uint8_t pt_dex_species_seen_count(const pt_dex_t *d)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < PT_DEX_SPECIES; i += 1) {
        if (d->level[i] > PT_DEX_LV_NONE) {
            n += 1;
        }
    }
    return n;
}

uint8_t pt_dex_species_raised_total(const pt_dex_t *d)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < PT_DEX_SPECIES; i += 1) {
        if (d->level[i] >= PT_DEX_LV_RAISED) {
            n += 1;
        }
    }
    return n;
}

uint8_t pt_dex_pure_chain(const pt_soc_hall_t *hall, uint8_t hall_count,
                          pt_gene_slot_t slot)
{
    if (hall == NULL || hall_count < 2
        || (uint8_t) slot >= PT_GENE_SLOT_COUNT) {
        return 0;
    }
    ensure_offsets();
    const pt_genome_t *newest = &hall[hall_count - 1].pet;
    uint8_t want = pt_part_index(part_byte_of(newest, slot));
    if (want >= pt_slot_part_count(slot)) {
        return 0;
    }
    uint8_t chain = 1;
    for (int i = (int) hall_count - 2; i >= 0; i -= 1) {
        uint8_t idx = pt_part_index(part_byte_of(&hall[i].pet, slot));
        if (idx != want) {
            break;
        }
        chain += 1;
    }
    return chain >= 2 ? chain : 0;
}

static const uint16_t s_reward[7] = { 5, 10, 20, 5, 10, 15, 30 };

uint8_t pt_dex_poll_rewards(pt_dex_t *d, uint16_t *amount)
{
    uint8_t seen_species = pt_dex_species_seen_count(d);
    uint8_t raised_species = pt_dex_species_raised_total(d);
    uint8_t seen_parts = pt_dex_parts_count(d, PT_DEX_PART_SEEN);
    uint8_t total_parts = pt_dex_part_total();

    uint8_t earned = 0;
    if (raised_species >= 1) { earned |= PT_DEX_MILE_FIRST_RAISED; }
    if (seen_species >= 4) { earned |= PT_DEX_MILE_SPECIES_4; }
    if (seen_species >= PT_DEX_SPECIES) { earned |= PT_DEX_MILE_SPECIES_ALL; }
    if (seen_parts >= 16) { earned |= PT_DEX_MILE_PARTS_16; }
    if (seen_parts >= 32) { earned |= PT_DEX_MILE_PARTS_32; }
    if (seen_parts >= 48) { earned |= PT_DEX_MILE_PARTS_48; }
    if (seen_parts >= total_parts) { earned |= PT_DEX_MILE_PARTS_ALL; }

    uint8_t fresh = (uint8_t) (earned & ~d->claimed);
    d->claimed |= fresh;
    if (amount != NULL) {
        *amount = 0;
        for (uint8_t b = 0; b < 7; b += 1) {
            if ((fresh & (uint8_t) (1u << b)) != 0) {
                *amount = (uint16_t) (*amount + s_reward[b]);
            }
        }
    }
    return fresh;
}
