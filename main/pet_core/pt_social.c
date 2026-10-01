// pet_core/pt_social.c —— P2-S3a 离线社交状态机实现（designs 08）。
#include "pt_social.h"

#include <string.h>

#include "pt_config.h"
#include "pt_engine.h"
#include "pt_genome.h"
#include "pt_rng.h"

static const char *const SYLLABLES[] = {
    "ni", "bi", "ta", "mo", "lu", "ki",
    "pa", "zu", "ra", "fa", "lo", "mi",
};

void pt_social_name(const pt_genome_t *g, char *out, uint8_t cap)
{
    if (out == NULL || cap == 0U) {
        return;
    }
    uint32_t h = pt_hash32(g, (uint32_t) sizeof(*g));
    uint32_t a = h % 12u;
    uint32_t b = (h >> 4) % 12u;
    if (b == a) {
        b = (b + 1u) % 12u;
    }
    const char *s1 = SYLLABLES[a];
    const char *s2 = SYLLABLES[b];
    char tmp[8];
    int n = 0;
    tmp[n++] = (char) (s1[0] - 'a' + 'A');
    tmp[n++] = s1[1];
    tmp[n++] = s2[0];
    tmp[n++] = s2[1];
    tmp[n] = '\0';
    if ((int) cap <= n) {
        n = (int) cap - 1;
    }
    memcpy(out, tmp, (size_t) n);
    out[n] = '\0';
}

pt_rel_stage_t pt_social_rel_stage(uint8_t bond)
{
    if (bond >= PT_CFG_SOC_LOVE_BOND) {
        return PT_REL_LOVE;
    }
    if (bond >= 50) {
        return PT_REL_CRUSH;
    }
    if (bond >= 25) {
        return PT_REL_FRIEND;
    }
    return PT_REL_MET;
}

const char *pt_social_rel_name(pt_rel_stage_t st)
{
    static const char *const NAMES[] = { "Met", "Friend", "Crush", "Love" };
    if ((unsigned) st > PT_REL_LOVE) {
        return "?";
    }
    return NAMES[st];
}

uint8_t pt_social_candidate_count(const pt_social_t *so)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < PT_SOC_CANDIDATES; i += 1) {
        if (so->cand[i].valid) {
            n += 1;
        }
    }
    return n;
}

void pt_social_init(pt_social_t *so, uint32_t seed, int32_t day_id)
{
    memset(so, 0, sizeof(*so));
    so->phase = PT_SOC_SINGLE;
    pt_rng_seed(&so->rng_state, seed ^ 0x50C1A1u);
    so->day_id = day_id;
    so->last_roll_day = INT32_MIN;
    for (uint8_t i = 0; i < PT_SOC_CANDIDATES; i += 1) {
        so->cand[i].last_interact = -1;
        so->cand[i].last_day = -1;
    }
    so->married_at = -1;
}

// 候选性格相性由其名片基因确定性派生；玩家性格恰好是对方偏好即"送对"。
static uint8_t candidate_pref(const pt_soc_cand_t *c)
{
    return (uint8_t) (pt_hash32(&c->genome, sizeof(c->genome))
                      % PT_PERS_COUNT);
}

static uint8_t free_or_worst_slot(const pt_social_t *so)
{
    for (uint8_t i = 0; i < PT_SOC_CANDIDATES; i += 1) {
        if (!so->cand[i].valid) {
            return i;
        }
    }
    uint8_t worst = 0;
    for (uint8_t i = 1; i < PT_SOC_CANDIDATES; i += 1) {
        if (so->cand[i].bond < so->cand[worst].bond) {
            worst = i;
        }
    }
    return worst;
}

static uint8_t roll_candidate(pt_social_t *so)
{
    uint8_t slot = free_or_worst_slot(so);
    pt_soc_cand_t *c = &so->cand[slot];
    memset(c, 0, sizeof(*c));
    c->valid = true;
    c->last_interact = -1;
    c->last_day = -1;
    // 名片允许亲和位全开（含 SPECIAL）；高世代稀有度加权（08 §3.1）。
    pt_genome_roll(&c->genome, 0xFFu, so->generation, &so->rng_state);
    return slot;
}

static void cross_day(pt_social_t *so, int32_t new_day)
{
    for (uint8_t i = 0; i < PT_SOC_CANDIDATES; i += 1) {
        pt_soc_cand_t *c = &so->cand[i];
        if (!c->valid) {
            continue;
        }
        c->interacts_today = 0;
        if (c->last_day >= 0) {
            // 宽限 3 天后每个跨日 -5/天（08 §2）。
            for (int32_t d = so->day_id + 1; d <= new_day; d += 1) {
                if (d - c->last_day > PT_CFG_SOC_DECAY_GRACE_DAYS) {
                    int32_t v = (int32_t) c->bond - PT_CFG_SOC_DECAY_PER_DAY;
                    c->bond = (uint8_t) (v < 0 ? 0 : v);
                }
            }
        }
    }
    so->day_id = new_day;
}

static void maybe_breed_egg(pt_social_t *so, const pt_state_t *s,
                            pt_events_t *q)
{
    uint32_t child_gen = so->generation + 1u;
    pt_breed_input_t in;
    memset(&in, 0, sizeof(in));
    memset(so->child_anc, 0, sizeof(so->child_anc));
    in.mother = &s->genome;
    in.father = &so->spouse;
    in.allowed_tags = 0xFFu;
    in.diet_tag = 0xFFu;
    in.generation = child_gen;
    in.seed = pt_genome_child_seed(&s->genome, &so->spouse, child_gen);
    pt_genome_breed(&so->child_genome, &in);
    // 子代祖辈环（05 §4）：母系 [0]=母、[1..3]=母的最近三代；父系 [4]=父。
    // NPC 配偶无更高祖辈信息（LAN 名片线 P3 再补 [5..7]）。
    memcpy(in.anc, so->active_anc, sizeof(in.anc));
    in.anc_present = so->active_anc_present;
    so->child_anc[0] = s->genome;
    so->child_anc[4] = so->spouse;
    uint8_t present = (uint8_t) ((1u << 0) | (1u << 4));
    for (uint8_t i = 0; i < 3; i += 1) {
        if (so->active_anc_present & (uint8_t) (1u << i)) {
            so->child_anc[i + 1] = so->active_anc[i];
            present |= (uint8_t) (1u << (i + 1));
        }
    }
    pt_genome_breed(&so->child_genome, &in);
    so->child_anc_present = present;
    so->phase = PT_SOC_EGG_READY;
    pt_events_push(q, PT_EV_SOC_EGG_READY, s->minute, 0, 0);
}

void pt_social_on_minute(pt_social_t *so, const pt_state_t *s,
                         pt_events_t *q)
{
    if (so == NULL || s == NULL) {
        return;
    }
    int32_t day = pt_day_index(s->minute);
    if (day > so->day_id) {
        cross_day(so, day);
    }

    if (so->phase == PT_SOC_SINGLE && s->stage == PT_STAGE_ADULT
        && (int32_t) s->age_days >= PT_CFG_SOC_MATCHMAKER_DELAY_DAYS
        && so->last_roll_day != day) {
        uint8_t slot = roll_candidate(so);
        so->last_roll_day = day;
        pt_events_push(q, PT_EV_SOC_CANDIDATE, s->minute, slot, 0);
    }

    if (so->phase == PT_SOC_MARRIED && so->married_at >= 0
        && s->minute - so->married_at >= PT_CFG_SOC_COHABIT_MIN) {
        maybe_breed_egg(so, s, q);
    }

    // 子代进入少年：父母同日离队（08 §1），父母一代入家谱。
    if (so->care_active && s->stage == PT_STAGE_TEEN) {
        pt_social_end_parenting(so, s->minute, q);
    }
}

static pt_soc_rv_t interact_check(const pt_social_t *so, uint8_t ci,
                                  int32_t minute)
{
    if (so->phase != PT_SOC_SINGLE) {
        return PT_SOC_BAD_STATE;
    }
    if (ci >= PT_SOC_CANDIDATES || !so->cand[ci].valid) {
        return PT_SOC_BAD_SLOT;
    }
    const pt_soc_cand_t *c = &so->cand[ci];
    if (c->interacts_today >= PT_CFG_SOC_INTERACT_DAY_CAP) {
        return PT_SOC_CAPPED;
    }
    if (c->last_interact >= 0
        && minute - c->last_interact < PT_CFG_SOC_INTERACT_COOLDOWN_MIN) {
        return PT_SOC_COOLDOWN;
    }
    return PT_SOC_OK;
}

static void mark_interact(pt_social_t *so, uint8_t ci, int32_t minute,
                          uint8_t gain)
{
    pt_soc_cand_t *c = &so->cand[ci];
    int32_t v = (int32_t) c->bond + gain;
    if (v > 100) {
        v = 100;
    }
    c->bond = (uint8_t) v;
    c->interacts_today += 1;
    c->last_interact = minute;
    c->last_day = pt_day_index(minute);
}

pt_soc_rv_t pt_social_greet(pt_social_t *so, uint8_t ci, int32_t minute)
{
    pt_soc_rv_t rv = interact_check(so, ci, minute);
    if (rv != PT_SOC_OK) {
        return rv;
    }
    mark_interact(so, ci, minute, PT_CFG_SOC_GREET_BOND);
    return PT_SOC_OK;
}

pt_soc_rv_t pt_social_gift(pt_social_t *so, uint8_t ci, int32_t minute,
                           uint32_t *coins, uint8_t player_personality)
{
    if (coins == NULL) {
        return PT_SOC_BAD_STATE;
    }
    pt_soc_rv_t rv = interact_check(so, ci, minute);
    if (rv != PT_SOC_OK) {
        return rv;
    }
    if (*coins < PT_CFG_SOC_GIFT_COINS) {
        return PT_SOC_POOR;
    }
    bool match = candidate_pref(&so->cand[ci]) == player_personality;
    *coins -= PT_CFG_SOC_GIFT_COINS;
    mark_interact(so, ci, minute,
                  (uint8_t) (match ? PT_CFG_SOC_GIFT_BOND_MATCH
                                   : PT_CFG_SOC_GIFT_BOND_OTHER));
    return PT_SOC_OK;
}

pt_soc_rv_t pt_social_propose(pt_social_t *so, uint8_t ci, pt_ring_t ring,
                              int32_t minute, uint32_t *coins,
                              pt_events_t *q)
{
    if (coins == NULL) {
        return PT_SOC_BAD_STATE;
    }
    if (so->phase != PT_SOC_SINGLE) {
        return PT_SOC_BAD_STATE;
    }
    if (ci >= PT_SOC_CANDIDATES || !so->cand[ci].valid) {
        return PT_SOC_BAD_SLOT;
    }
    pt_soc_cand_t *c = &so->cand[ci];
    if (c->bond < PT_CFG_SOC_LOVE_BOND) {
        return PT_SOC_NOT_LOVE;
    }
    if (c->propose_lock != 0 && minute < c->propose_lock) {
        return PT_SOC_LOCKED;
    }
    uint32_t price = (ring == PT_RING_DIAMOND)
                     ? PT_CFG_SOC_RING_DIAMOND_COINS
                     : PT_CFG_SOC_RING_SIMPLE_COINS;
    if (*coins < price) {
        return PT_SOC_POOR;
    }

    int32_t pct = PT_CFG_SOC_ACCEPT_AT_LOVE_PCT
                  + (int32_t) (c->bond - PT_CFG_SOC_LOVE_BOND)
                    * PT_CFG_SOC_ACCEPT_PER_BOND_PCT;
    if (ring == PT_RING_DIAMOND) {
        pct += PT_CFG_SOC_ACCEPT_DIAMOND_PCT;
    }
    if (pct > PT_CFG_SOC_ACCEPT_CAP_PCT) {
        pct = PT_CFG_SOC_ACCEPT_CAP_PCT;
    }
    uint32_t roll = pt_rng_below(&so->rng_state, 100);
    if ((int32_t) roll >= pct) {
        c->propose_lock = minute + PT_CFG_SOC_PROPOSE_LOCK_MIN;
        return PT_SOC_REJECTED;
    }

    *coins -= price;
    so->spouse = c->genome;
    so->spouse_personality = c->genome.personality;
    so->married_at = minute;
    so->phase = PT_SOC_MARRIED;
    for (uint8_t i = 0; i < PT_SOC_CANDIDATES; i += 1) {
        memset(&so->cand[i], 0, sizeof(so->cand[i]));
        so->cand[i].last_interact = -1;
        so->cand[i].last_day = -1;
    }
    pt_events_push(q, PT_EV_SOC_WEDDING, minute, 0, 0);
    return PT_SOC_OK;
}

int32_t pt_social_egg_minutes_left(const pt_social_t *so, int32_t minute)
{
    if (so->phase != PT_SOC_MARRIED || so->married_at < 0) {
        return -1;
    }
    int32_t left = so->married_at + PT_CFG_SOC_COHABIT_MIN - minute;
    return left < 0 ? 0 : left;
}

// ---- P2-S3b 世代交替 / 育儿 / 家谱 ----------------------------------------

uint32_t pt_social_generation(const pt_social_t *so)
{
    return so->generation;
}

bool pt_social_care_active(const pt_social_t *so)
{
    return so->care_active != 0;
}

uint8_t pt_social_hall_count(const pt_social_t *so)
{
    return so->hall_count;
}

const pt_soc_hall_t *pt_social_hall_entry(const pt_social_t *so, uint8_t i)
{
    if (i >= so->hall_count) {
        return NULL;
    }
    return &so->hall[i];
}

// 成年定型物种 → CARE 档（月影隐藏角色按 PERFECT 教养计）。
static uint8_t care_from_species(pt_species_t sp)
{
    switch (sp) {
    case PT_SP_ADULT_PERFECT:
    case PT_SP_ADULT_MOON:
        return PT_CARE_PERFECT;
    case PT_SP_ADULT_GREAT:
        return PT_CARE_GREAT;
    case PT_SP_ADULT_NEGLECT:
        return PT_CARE_NEGLECT;
    case PT_SP_ADULT_NORMAL:
    default:
        return PT_CARE_NORMAL;
    }
}

bool pt_social_begin_generation(pt_social_t *so, const pt_state_t *parent)
{
    if (so->phase != PT_SOC_EGG_READY) {
        return false;
    }
    // 归档父母一代（少年离队时正式入堂）。
    memset(&so->pending, 0, sizeof(so->pending));
    so->pending.pet = parent->genome;
    so->pending.mate = so->spouse;
    so->pending.generation = so->generation;
    so->pending.species = (uint8_t) parent->species;
    so->pending.care = care_from_species(parent->species);
    so->pending.adult_age_days = parent->age_days;
    so->pending_valid = 1u;

    // 代次 +1，祖辈环过户给子代。
    so->generation += 1u;
    memcpy(so->active_anc, so->child_anc, sizeof(so->active_anc));
    so->active_anc_present = so->child_anc_present;

    // 社交回单身：候选清空、配偶信息清空，成年后重新走婚介。
    so->phase = PT_SOC_SINGLE;
    for (uint8_t i = 0; i < PT_SOC_CANDIDATES; i += 1) {
        memset(&so->cand[i], 0, sizeof(so->cand[i]));
        so->cand[i].last_interact = -1;
        so->cand[i].last_day = -1;
    }
    memset(&so->spouse, 0, sizeof(so->spouse));
    so->spouse_personality = 0;
    so->married_at = -1;
    so->last_roll_day = INT32_MIN;
    memset(&so->child_genome, 0, sizeof(so->child_genome));
    memset(so->child_anc, 0, sizeof(so->child_anc));
    so->child_anc_present = 0;

    so->care_active = 1u;
    return true;
}

bool pt_social_end_parenting(pt_social_t *so, int32_t minute, pt_events_t *q)
{
    if (!so->care_active) {
        return false;
    }
    so->care_active = 0u;
    if (so->pending_valid) {
        if (so->hall_count >= PT_SOC_HALL_MAX) {
            // 名人堂满：最旧一代淘汰（FIFO），代际深度随图鉴切片再扩。
            memmove(&so->hall[0], &so->hall[1],
                    sizeof(pt_soc_hall_t) * (PT_SOC_HALL_MAX - 1u));
            so->hall_count = PT_SOC_HALL_MAX - 1u;
        }
        so->hall[so->hall_count] = so->pending;
        so->hall_count += 1u;
        so->pending_valid = 0u;
    }
    pt_events_push(q, PT_EV_SOC_PARENTS_LEAVE, minute, 0, 0);
    return true;
}
