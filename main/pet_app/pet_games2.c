#include "pet_games2.h"

#include <stddef.h>

#include "pt_config.h"

// 与 pet_game/pt_rng 同算法的小游戏私有 PRNG：不消耗存档 rng_state。
static uint32_t grand(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static uint8_t below(uint32_t *r, uint8_t range)
{
    return range ? (uint8_t) (grand(r) % range) : 0;
}

uint8_t pet_grade_from_score(uint8_t score, uint8_t energy)
{
    uint16_t s = score;
    if (energy < PT_CFG_GAME_LOW_ENERGY) {
        s = (uint16_t) (s * PT_CFG_GAME_SCORE_PENALTY_NUM
                        / PT_CFG_GAME_SCORE_PENALTY_DEN);
    }
    if (s >= PET_GRADE_SCORE_PERFECT) {
        return PT_GAME_GRADE_PERFECT;
    }
    if (s >= PET_GRADE_SCORE_GREAT) {
        return PT_GAME_GRADE_GREAT;
    }
    return PT_GAME_GRADE_GOOD;
}

uint8_t pet_grade_for_job(uint8_t score, uint8_t energy, bool senior)
{
    uint16_t s = score;
    if (energy < PT_CFG_GAME_LOW_ENERGY) {
        s = (uint16_t) (s * PT_CFG_GAME_SCORE_PENALTY_NUM
                        / PT_CFG_GAME_SCORE_PENALTY_DEN);
    }
    uint16_t perfect = PET_GRADE_SCORE_PERFECT;
    uint16_t great = PET_GRADE_SCORE_GREAT;
    if (senior) {
        perfect = (uint16_t) (perfect * PT_CFG_JOB_SENIOR_RELAX_NUM
                              / PT_CFG_JOB_SENIOR_RELAX_DEN);
        great = (uint16_t) (great * PT_CFG_JOB_SENIOR_RELAX_NUM
                            / PT_CFG_JOB_SENIOR_RELAX_DEN);
    }
    if (s >= perfect) {
        return PT_GAME_GRADE_PERFECT;
    }
    if (s >= great) {
        return PT_GAME_GRADE_GREAT;
    }
    return PT_GAME_GRADE_GOOD;
}

// ---------------------------------------------------------------- G2

#define G2_RANGE 100
#define G2_SPEED 8
#define G2_PERFECT 12
#define G2_GREAT   30
#define G2_GOOD    50

void pet_g2_init(pet_g2_t *g)
{
    g->pos = 0;
    g->dir = 1;
    g->speed = G2_SPEED;
    g->beat = 0;
    g->points = 0;
    g->done = false;
}

void pet_g2_tick(pet_g2_t *g)
{
    if (g->done) {
        return;
    }
    g->pos = (int8_t) (g->pos + g->dir * g->speed);
    if (g->pos >= G2_RANGE) {
        g->pos = G2_RANGE;
        g->dir = -1;
    } else if (g->pos <= -G2_RANGE) {
        g->pos = -G2_RANGE;
        g->dir = 1;
    }
}

static void g2_finish_beat(pet_g2_t *g, uint16_t pts)
{
    g->points = (uint16_t) (g->points + pts);
    g->beat += 1;
    if (g->beat >= PET_G2_BEATS) {
        g->done = true;
    }
}

void pet_g2_press(pet_g2_t *g)
{
    if (g->done) {
        return;
    }
    uint8_t d = (uint8_t) (g->pos < 0 ? -g->pos : g->pos);
    uint16_t pts;
    if (d <= G2_PERFECT) {
        pts = 100;
    } else if (d <= G2_GREAT) {
        pts = 60;
    } else if (d <= G2_GOOD) {
        pts = 30;
    } else {
        pts = 10;
    }
    g2_finish_beat(g, pts);
}

void pet_g2_miss(pet_g2_t *g)
{
    if (!g->done) {
        g2_finish_beat(g, 0);
    }
}

uint8_t pet_g2_score(const pet_g2_t *g)
{
    if (g->beat == 0) {
        return 0;
    }
    return (uint8_t) (g->points / g->beat);
}

// ---------------------------------------------------------------- G3

#define G3_BOTTOM   4      // 落到底的行号
static const int8_t G3_NONE = -1;

void pet_g3_init(pet_g3_t *g, uint32_t seed)
{
    g->rng = seed ? seed : 0x9e3779b9u;
    g->player = 1;
    g->tick = 0;
    g->spawn_in = 8;
    g->score = 0;
    for (uint8_t i = 0; i < 5; i += 1) {
        g->drops[i].lane = G3_NONE;
        g->drops[i].bad = false;
        g->drops[i].row = 0;
    }
}

void pet_g3_move(pet_g3_t *g, int delta)
{
    int p = (int) g->player + (delta < 0 ? -1 : 1);
    if (p < 0) {
        p = PET_G3_LANES - 1;
    } else if (p >= PET_G3_LANES) {
        p = 0;
    }
    g->player = (int8_t) p;
}

static uint16_t g3_bad_permille(uint16_t tick)
{
    // 难度随时间提升：坏东西比例 20% → 40%。
    uint16_t p = (uint16_t) (200u + 200u * tick / PET_G3_TICKS);
    return p > 400u ? 400u : p;
}

static void g3_spawn(pet_g3_t *g)
{
    for (uint8_t i = 0; i < 5; i += 1) {
        if (g->drops[i].lane == G3_NONE) {
            g->drops[i].lane = (int8_t) below(&g->rng, PET_G3_LANES);
            g->drops[i].bad = (grand(&g->rng) % 1000u)
                              < g3_bad_permille(g->tick);
            g->drops[i].row = 0;
            return;
        }
    }
}

static uint8_t g3_fall_period(uint16_t tick)
{
    // 下落速度随时间提升：每步 13 → 8 tick。
    uint8_t p = (uint8_t) (13u - tick / 60u);
    return p < 8u ? 8u : p;
}

void pet_g3_tick(pet_g3_t *g)
{
    if (g->tick >= PET_G3_TICKS) {
        return;
    }
    g->tick += 1;

    if (g->spawn_in > 0) {
        g->spawn_in -= 1;
    }
    uint8_t spawn_period = (uint8_t) (16u - g->tick / 40u);
    if (spawn_period < 8u) {
        spawn_period = 8u;   // 最快每 8 tick 生成一个
    }
    if (g->spawn_in == 0) {
        g3_spawn(g);
        g->spawn_in = spawn_period;
    }

    if (g->tick % g3_fall_period(g->tick) == 0) {
        for (uint8_t i = 0; i < 5; i += 1) {
            if (g->drops[i].lane == G3_NONE) {
                continue;
            }
            g->drops[i].row += 1;
            if (g->drops[i].row >= G3_BOTTOM) {
                bool hit = g->drops[i].lane == g->player;
                if (g->drops[i].bad) {
                    if (!hit) {
                        g->score += 3;          // 躲开坏东西
                    } else {
                        g->score -= 8;
                    }
                } else if (hit) {
                    g->score += 10;
                }
                g->drops[i].lane = G3_NONE;
            }
        }
    }
}

bool pet_g3_done(const pet_g3_t *g)
{
    return g->tick >= PET_G3_TICKS;
}

uint8_t pet_g3_score(const pet_g3_t *g)
{
    int16_t s = g->score;
    if (s < 0) {
        s = 0;
    }
    if (s > 100) {
        s = 100;
    }
    return (uint8_t) s;
}

// ---------------------------------------------------------------- G4

void pet_g4_init(pet_g4_t *g, uint32_t seed)
{
    g->rng = seed ? seed : 0x9e3779b9u;
    for (uint8_t i = 0; i < 7; i += 1) {
        g->seq[i] = below(&g->rng, 3);
    }
    g->phase = PET_G4_SHOW;
    g->level = 3;
    g->show_idx = 0;
    g->input_idx = 0;
    g->best = 0;
}

uint8_t pet_g4_next_pad(pet_g4_t *g)
{
    if (g->phase != PET_G4_SHOW || g->show_idx >= g->level) {
        return 0xFFu;
    }
    return g->seq[g->show_idx++];
}

void pet_g4_start_input(pet_g4_t *g)
{
    if (g->phase == PET_G4_SHOW) {
        g->phase = PET_G4_INPUT;
        g->input_idx = 0;
    }
}

bool pet_g4_input(pet_g4_t *g, uint8_t pad)
{
    if (g->phase != PET_G4_INPUT) {
        return false;
    }
    if (pad != g->seq[g->input_idx]) {
        g->phase = PET_G4_DONE;
        return false;
    }
    g->input_idx += 1;
    if (g->input_idx < g->level) {
        return true;
    }
    // 本轮完整复现。
    g->best = g->level;
    if (g->level >= 7) {
        g->phase = PET_G4_DONE;
        return true;
    }
    g->level += 1;
    g->phase = PET_G4_SHOW;
    g->show_idx = 0;
    return true;
}

uint8_t pet_g4_score(const pet_g4_t *g)
{
    if (g->best >= 7) {
        return 100;
    }
    if (g->best >= 5) {
        return 70;
    }
    if (g->best >= 3) {
        return 40;
    }
    return 10;
}

// ---------------------------------------------------------------- G5

void pet_g5_init(pet_g5_t *g, uint32_t seed)
{
    g->rng = seed ? seed : 0x9e3779b9u;
    g->tick = 0;
    g->combo = 0;
    g->cleared = 0;
    g->next_obs = 0;
    for (uint8_t i = 0; i < PET_G5_OBSTACLES; i += 1) {
        g->obs[i].action = below(&g->rng, 2);
        g->obs[i].arrive = (int16_t) (30 + i * 34);
        g->obs[i].judged = false;
        g->obs[i].cleared = false;
    }
}

static uint8_t g5_window(uint8_t cleared)
{
    // 连击越高窗口越窄（8 → 最小 4 拍）。
    uint8_t w = (uint8_t) (8 - cleared / 3);
    return w < 4u ? 4u : w;
}

void pet_g5_tick(pet_g5_t *g)
{
    if (g->next_obs >= PET_G5_OBSTACLES) {
        return;
    }
    g->tick += 1;
    pet_g5_obs_t *o = &g->obs[g->next_obs];
    if (!o->judged && g->tick > (int16_t) (o->arrive + g5_window(g->cleared))) {
        o->judged = true;      // 超时漏按 = 失误
        g->combo = 0;
        g->next_obs += 1;
    }
}

uint8_t pet_g5_act(pet_g5_t *g, pet_g5_action_t act)
{
    if (g->next_obs >= PET_G5_OBSTACLES) {
        return 0;
    }
    pet_g5_obs_t *o = &g->obs[g->next_obs];
    if (o->judged) {
        return 0;
    }
    int16_t d = g->tick - o->arrive;
    if (d < 0) {
        d = (int16_t) -d;
    }
    uint8_t ok = 0;
    if (((uint8_t) act) == o->action && d <= g5_window(g->cleared)) {
        o->cleared = true;
        g->cleared += 1;
        g->combo += 1;
        ok = 1;
    } else {
        g->combo = 0;
    }
    o->judged = true;
    g->next_obs += 1;
    return ok;
}

bool pet_g5_done(const pet_g5_t *g)
{
    return g->next_obs >= PET_G5_OBSTACLES;
}

uint8_t pet_g5_score(const pet_g5_t *g)
{
    return (uint8_t) (100u * g->cleared / PET_G5_OBSTACLES);
}

// ---------------------------------------------------------------- G6

void pet_g6_init(pet_g6_t *g, uint32_t seed, uint8_t last_food_tag)
{
    g->rng = seed ? seed : 0x9e3779b9u;
    g->question = 0;
    g->correct_n = 0;
    g->last_tag = last_food_tag;
    g->hint = (pet_g6_hint_t) below(&g->rng, 3);
}

pet_g6_hint_t pet_g6_current_hint(const pet_g6_t *g)
{
    return g->hint;
}

pet_g6_choice_t pet_g6_correct_answer(pet_g6_hint_t hint, uint8_t last_tag)
{
    (void) last_tag;   // P2 性格/口味修正上线后使用
    switch (hint) {
    case PET_G6_H_PLAYFUL: return PET_G6_TOY;
    case PET_G6_H_CUDDLY:  return PET_G6_HUG;
    case PET_G6_H_HUNGRY:
    default:               return PET_G6_FOOD;
    }
}

bool pet_g6_answer(pet_g6_t *g, pet_g6_choice_t choice)
{
    if (g->question >= PET_G6_QUESTIONS) {
        return false;
    }
    bool ok = choice == pet_g6_correct_answer(g->hint, g->last_tag);
    if (ok) {
        g->correct_n += 1;
    }
    g->question += 1;
    if (g->question < PET_G6_QUESTIONS) {
        g->hint = (pet_g6_hint_t) below(&g->rng, 3);
    }
    return ok;
}

bool pet_g6_done(const pet_g6_t *g)
{
    return g->question >= PET_G6_QUESTIONS;
}

uint8_t pet_g6_score(const pet_g6_t *g)
{
    return (uint8_t) (100u * g->correct_n / PET_G6_QUESTIONS);
}
