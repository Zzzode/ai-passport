#include "pt_engine.h"

#include <string.h>

#include "pt_config.h"
#include "pt_evolve.h"
#include "pt_rng.h"

// ---------------------------------------------------------------------------
// 房间环境增益（S4，运行期，见 pt_engine_set_room_buffs）
// ---------------------------------------------------------------------------

static uint8_t s_room_meal_bonus;       // 厨房：正餐额外饱腹
static uint8_t s_room_happy_pct;        // 盆栽：心情回复占衰减百分比
static uint8_t s_room_energy_pct = 100; // 健身角：睡眠体力回复百分比
// 盆栽回复是亚整点速率（成年 4 点/时 ×10% = 0.4 点/时），用 1/10 点精度的
// 独立累加器：每分钟累加 deci/时，满 600（=10×60）落地 1 点。
static int16_t s_room_happy_acc;

void pt_engine_set_room_buffs(uint8_t meal_bonus, uint8_t happy_pct,
                              uint8_t energy_pct)
{
    s_room_meal_bonus = meal_bonus;
    s_room_happy_pct = happy_pct > 10 ? 10 : happy_pct;
    s_room_energy_pct = energy_pct;
    s_room_happy_acc = 0;
}

// P2-S3b：育儿期父母同住（08 §4：夜间衰减额外 -10%、呼叫窗口 +15 分钟）。
static bool s_family_care;
// 夜间衰减万分之一单位余数（跨分钟累加，防整数截断吞掉育儿增益）。
static int32_t s_night_rem_full;
static int32_t s_night_rem_happy;

void pt_engine_set_family_care(bool active)
{
    s_family_care = active;
}

bool pt_engine_family_care_active(void)
{
    return s_family_care;
}

// 每分钟清醒心情回复（1/10 点/时）；速率 = 阶段衰减 × pct/10（×10/100 约分）。
static void room_happy_regen(pt_state_t *s, uint8_t happy_rate)
{
    if (s_room_happy_pct == 0) {
        s_room_happy_acc = 0;
        return;
    }
    s_room_happy_acc = (int16_t) (s_room_happy_acc
                                  + (int16_t) happy_rate * s_room_happy_pct
                                        / 10);
    while (s_room_happy_acc >= 600) {
        s_room_happy_acc = (int16_t) (s_room_happy_acc - 600);
        if (s->happiness < 100) {
            s->happiness += 1;
        }
    }
}

// ---------------------------------------------------------------------------
// 小工具
// ---------------------------------------------------------------------------

static int32_t floor_div(int32_t a, int32_t b)
{
    int32_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) {
        q -= 1;
    }
    return q;
}

// 一天内的分钟（0..1439）。
static int32_t minute_of_day(int32_t m)
{
    return m - floor_div(m, 1440) * 1440;
}

// 清醒日界 06:00；day_index 在每天 06:00 递增。
int32_t pt_day_index(int32_t minute)
{
    return floor_div(minute - PT_CFG_DAY_BOUNDARY_MIN, 1440);
}

static int32_t day_index(int32_t m)
{
    return pt_day_index(m);
}

static bool in_morning_window(int32_t m)
{
    int32_t mod = minute_of_day(m);
    return mod >= PT_CFG_EVOLVE_WINDOW_START && mod < PT_CFG_EVOLVE_WINDOW_END;
}

// CHILD 21:00 就寝，TEEN 及以后 22:00；BABY/EGG 无固定作息（P0 简化）。
static bool night_window(pt_stage_t stage, int32_t mod)
{
    if (stage < PT_STAGE_CHILD) {
        return false;
    }
    int32_t start = (stage == PT_STAGE_CHILD)
        ? PT_CFG_SLEEP_CHILD_START
        : PT_CFG_SLEEP_TEEN_START;
    return mod >= start || mod < PT_CFG_NIGHT_END_MIN;
}

static uint16_t call_window(pt_stage_t stage)
{
    uint16_t w = pt_cfg_call_window[stage];
    if (s_family_care) {
        w = (uint16_t) (w + PT_CFG_FAMILY_CALL_BONUS_MIN);
    }
    return w;
}

static uint8_t bond_cap(pt_stage_t stage)
{
    return pt_cfg_bond_cap[stage];
}

static void add_bond(pt_state_t *s, uint8_t delta)
{
    uint8_t cap = bond_cap(s->stage);
    int32_t next = (int32_t) s->bond + delta;
    s->bond = (uint8_t) (next > cap ? cap : next);
}

// 管教失误：它在 SAD 求关注时被塞零食/强行游戏——扣亲密度并记 1 次敷衍
// （designs 03 §管教失误）。账本冻结后不再计数，亲密度照扣。
static void note_perfunctory(pt_state_t *s)
{
    if (s->active_call != PT_CALL_SAD) {
        return;
    }
    if (!s->ledger_frozen) {
        s->ledger.perfunctory = (uint8_t) (s->ledger.perfunctory + 1 > 255
                                               ? 255
                                               : s->ledger.perfunctory + 1);
    }
    s->bond = s->bond >= 2 ? (uint8_t) (s->bond - 2) : 0;
}

// 定点累加：units_per_minute 以"点/小时"为量纲，60 单位落 1 点。
static void meter_add_units(uint8_t *meter, int16_t *acc, int32_t units_per_minute)
{
    *acc += (int16_t) units_per_minute;
    while (*acc >= 60) {
        *acc -= 60;
        if (*meter < 100) {
            *meter += 1;
        }
    }
    while (*acc <= -60) {
        *acc += 60;
        if (*meter > 0) {
            *meter -= 1;
        }
    }
}

static void push(pt_events_t *q, pt_event_kind_t kind, int32_t minute, uint8_t a,
                 uint8_t b)
{
    pt_events_push(q, kind, minute, a, b);
}

// ---------------------------------------------------------------------------
// 阶段与生命周期
// ---------------------------------------------------------------------------

static void schedule_poop(pt_state_t *s, int32_t from)
{
    uint16_t base = pt_cfg_poop_interval[s->stage];
    if (base == 0) {
        s->next_poop = -1;
        return;
    }
    uint32_t span = (uint32_t) base * PT_CFG_POOP_JITTER_NUM / PT_CFG_POOP_JITTER_DEN;
    // 抖动由阶段起点与已排次数派生：与推进速度无关，可复现。
    uint32_t h = pt_salted((uint32_t) s->stage_started, s->poops, 0x9au);
    int32_t jitter = (int32_t) (h % (2 * span + 1)) - (int32_t) span;
    s->next_poop = from + (int32_t) base + jitter;
}

static void schedule_sickness(pt_state_t *s, int32_t from, int32_t duration)
{
    if (s->stage == PT_STAGE_CHILD || s->stage == PT_STAGE_TEEN) {
        int32_t half = duration / 2;
        uint32_t h = pt_salted((uint32_t) from, (uint32_t) s->stage, 0x51u);
        int32_t jitter = half == 0 ? 0 : (int32_t) (h % (uint32_t) (half + 1));
        int32_t at = from + half / 2 + jitter;
        int32_t earliest = from + PT_CFG_SICK_GRACE_MIN;
        s->sick_scheduled = at < earliest ? earliest : at;
    } else {
        s->sick_scheduled = -1;
    }
}

static void begin_stage(pt_state_t *s, pt_events_t *q, pt_stage_t stage,
                        pt_species_t species, int32_t minute)
{
    s->stage = stage;
    s->species = species;
    s->stage_started = minute;

    // 账本轮转：CHILD 起算，TEEN/ADULT 保留上一阶段供累计判定。
    if (stage == PT_STAGE_CHILD) {
        memset(&s->prev_ledger, 0, sizeof(s->prev_ledger));
        memset(&s->ledger, 0, sizeof(s->ledger));
    } else if (stage == PT_STAGE_TEEN || stage == PT_STAGE_ADULT) {
        s->prev_ledger = s->ledger;
        memset(&s->ledger, 0, sizeof(s->ledger));
    }
    s->ledger_frozen = false;

    s->poops = 0;
    s->poop_health_at = minute;
    s->poop_sick_at = 0;
    schedule_poop(s, minute);
    schedule_sickness(s, minute, pt_stage_min_duration(stage));
    (void) q;
}

static void make_sick(pt_state_t *s, pt_events_t *q, int32_t minute)
{
    if (s->sick || s->stage == PT_STAGE_EGG || s->stage == PT_STAGE_DEAD) {
        return;
    }
    s->sick = true;
    push(q, PT_EV_SICK, minute, 0, 0);
}

// 小/大失误记账（账本冻结后不再计入，见 designs 03 §9.4）。
static void add_mistake(pt_state_t *s, bool big)
{
    if (s->ledger_frozen || s->stage == PT_STAGE_EGG
        || s->stage >= PT_STAGE_ADULT) {
        return;   // 蛋期不计；成年起不再影响进化档
    }
    if (big) {
        s->ledger.big += 1;
    } else {
        s->ledger.small += 1;
    }
}

static void perform_evolve(pt_state_t *s, pt_events_t *q, int32_t minute)
{
    pt_stage_t next = (pt_stage_t) (s->stage + 1);
    pt_species_t species = s->pending_species;

    s->waiting_evolve = false;
    s->pending_species = PT_SP_NONE;

    if (next == PT_STAGE_ADULT) {
        // 成年定型：失误取本阶段+上一阶段累计，顶满只认本阶段（designs 04 §3）。
        pt_ledger_t combined = pt_ledger_sum(&s->prev_ledger, &s->ledger);
        combined.full_topups = s->ledger.full_topups;
        combined.happy_topups = s->ledger.happy_topups;
        int32_t days = pt_roll_lifespan_days(pt_care_tier(&combined, true),
                                            &s->rng_state);
        s->death_minute = minute + days * 1440;
    }

    begin_stage(s, q, next, species, minute);
    push(q, PT_EV_EVOLUTION, minute, (uint8_t) next, (uint8_t) species);
}

static void freeze_evolution(pt_state_t *s, int32_t minute)
{
    pt_ledger_t sum = pt_ledger_sum(&s->prev_ledger, &s->ledger);
    pt_care_tier_t tier;

    if (s->stage == PT_STAGE_CHILD) {
        tier = pt_care_tier(&sum, false);
        s->pending_species = pt_pick_teen(tier);
    } else {
        tier = pt_care_tier(&sum, true);
        s->pending_species = pt_pick_adult_full(tier, s->bond, sum.perfunctory);
    }
    s->waiting_evolve = true;
    s->ledger_frozen = true;
    (void) minute;
}

// ---------------------------------------------------------------------------
// 呼叫与需求
// ---------------------------------------------------------------------------

static void raise_call(pt_state_t *s, pt_events_t *q, pt_call_kind_t kind,
                       int32_t minute)
{
    s->active_call = kind;
    s->call_started = minute;
    push(q, PT_EV_CALL_RAISED, minute, (uint8_t) kind, 0);
}

static void clear_call(pt_state_t *s, pt_events_t *q, int32_t minute, bool resolved)
{
    if (s->active_call == PT_CALL_NONE) {
        return;
    }
    if (resolved) {
        add_bond(s, PT_CFG_CALL_RESOLVE_BOND);
        push(q, PT_EV_CALL_RESOLVED, minute, (uint8_t) s->active_call, 0);
    }
    s->active_call = PT_CALL_NONE;
}

// 处理某个需求的状态机（呼叫发起、超时、大失误、重新武装）。
static void service_need(pt_state_t *s, pt_events_t *q, pt_need_t need,
                         uint8_t meter, int32_t minute)
{
    pt_need_state_t state = s->need_state[need];

    if (state == PT_NEED_NEGLECT) {
        if (meter == 0) {
            if (s->zero_since[need] < 0) {
                s->zero_since[need] = minute;
            } else if (minute - s->zero_since[need] >= (int32_t) call_window(s->stage)) {
                add_mistake(s, true);
                push(q, PT_EV_MISTAKE_BIG, minute, (uint8_t) need, 0);
                s->need_state[need] = PT_NEED_COOLDOWN;
                s->zero_since[need] = -1;
                return;
            }
        }
    }

    if (s->need_state[need] != PT_NEED_ARMED && meter > PT_CFG_METER_REARM) {
        s->need_state[need] = PT_NEED_ARMED;
        s->zero_since[need] = -1;
    }
}

static void maybe_raise_need_call(pt_state_t *s, pt_events_t *q, int32_t minute)
{
    if (s->active_call != PT_CALL_NONE || s->sleeping) {
        return;
    }
    if (s->fullness <= PT_CFG_METER_CALL_THRESHOLD
        && s->need_state[PT_NEED_FULL] == PT_NEED_ARMED) {
        raise_call(s, q, PT_CALL_HUNGRY, minute);
    } else if (s->happiness <= PT_CFG_METER_CALL_THRESHOLD
               && s->need_state[PT_NEED_HAPPY] == PT_NEED_ARMED) {
        raise_call(s, q, PT_CALL_SAD, minute);
    }
}

// 当前呼叫是否因底层需求被满足而自然解除。
static void resolve_need_calls(pt_state_t *s, pt_events_t *q, int32_t minute)
{
    if (s->active_call == PT_CALL_HUNGRY
        && s->fullness > PT_CFG_METER_CALL_THRESHOLD) {
        clear_call(s, q, minute, true);
    } else if (s->active_call == PT_CALL_SAD
               && s->happiness > PT_CFG_METER_CALL_THRESHOLD) {
        clear_call(s, q, minute, true);
    }
}

// ---------------------------------------------------------------------------
// 睡眠
// ---------------------------------------------------------------------------

static void sleep_begin(pt_state_t *s, pt_events_t *q, int32_t minute)
{
    s->sleeping = true;
    // 免打扰：入睡时挂起未决的饥饿/心情呼叫，不记失误（窗口在醒后重计）。
    if (s->active_call == PT_CALL_HUNGRY || s->active_call == PT_CALL_SAD) {
        s->active_call = PT_CALL_NONE;
        s->zero_since[PT_NEED_FULL] = -1;
        s->zero_since[PT_NEED_HAPPY] = -1;
    }
    (void) q;
    (void) minute;
}

static void wake_up(pt_state_t *s, int32_t minute)
{
    s->sleeping = false;
    s->lights_off = false;   // 天亮自动开灯
    if (s->poor_sleep) {
        if (s->energy > PT_CFG_POOR_SLEEP_NUM) {
            s->energy = PT_CFG_POOR_SLEEP_NUM;
        }
        s->poor_sleep = false;
    }
    (void) minute;
}

// ---------------------------------------------------------------------------
// 单分钟推进
// ---------------------------------------------------------------------------

static void step_minute(pt_state_t *s, pt_events_t *q)
{
    int32_t minute = s->minute + 1;
    s->minute = minute;

    if (s->stage == PT_STAGE_DEAD) {
        return;
    }

    bool night = night_window(s->stage, minute_of_day(minute));

    // ① 睡眠 / 灯 / 体力
    if (s->stage >= PT_STAGE_CHILD) {
        if (night && !s->sleeping) {
            if (s->lights_off) {
                sleep_begin(s, q, minute);
            } else if (s->active_call == PT_CALL_NONE) {
                raise_call(s, q, PT_CALL_LIGHTS, minute);
            } else if (s->active_call == PT_CALL_LIGHTS
                       && minute - s->call_started >= (int32_t) call_window(s->stage)) {
                // 忘关灯：记小失误、当晚体力打折，宠物仍然睡下。
                add_mistake(s, false);
                push(q, PT_EV_MISTAKE_SMALL, minute, 0xff, PT_CALL_LIGHTS);
                s->poor_sleep = true;
                s->active_call = PT_CALL_NONE;
                sleep_begin(s, q, minute);
            }
        } else if (!night && s->sleeping) {
            wake_up(s, minute);
        }
    }
    if (s->sleeping && s->active_call == PT_CALL_LIGHTS && s->lights_off) {
        clear_call(s, q, minute, true);
    }

    // ② 计量衰减 / 回复
    int32_t full_rate = pt_cfg_dec_full[s->stage];
    int32_t happy_rate = pt_cfg_dec_happy[s->stage];
    int32_t energy_rate = pt_cfg_dec_energy[s->stage];

    if (s->sleeping) {
        // P2-S3b 育儿期父母同住：夜间衰减再 ×90%（叠在 NIGHT_DECAY 折扣上）。
        // 万分之一单位/分钟精度跨分钟累加，避免低速率档（如 CHILD 7）下
        // 10% 增益被整数除法整碗吞掉。
        int32_t pct = s_family_care ? PT_CFG_FAMILY_NIGHT_DECAY_PCT : 100;
        s_night_rem_full += full_rate * PT_CFG_NIGHT_DECAY_NUM * pct;
        int32_t ufull = s_night_rem_full / 10000;
        s_night_rem_full -= ufull * 10000;
        s_night_rem_happy += happy_rate * PT_CFG_NIGHT_DECAY_NUM * pct;
        int32_t uhap = s_night_rem_happy / 10000;
        s_night_rem_happy -= uhap * 10000;
        meter_add_units(&s->fullness, &s->acc_full, -ufull);
        meter_add_units(&s->happiness, &s->acc_happy, -uhap);
        int32_t regen = PT_CFG_ENERGY_REGEN_PER_H * s_room_energy_pct / 100
            * (s->poor_sleep ? PT_CFG_POOR_SLEEP_NUM : 100) / 100;
        meter_add_units(&s->energy, &s->acc_energy, regen);
        if (s->health < 80 && !s->sick) {
            meter_add_units(&s->health, &s->acc_health, PT_CFG_HEALTH_REGEN_NIGHT);
        }
    } else {
        // 清醒时清掉夜间余数，避免跨睡眠段累积漂移。
        s_night_rem_full = 0;
        s_night_rem_happy = 0;
        meter_add_units(&s->fullness, &s->acc_full, -full_rate);
        meter_add_units(&s->happiness, &s->acc_happy, -happy_rate);
        meter_add_units(&s->energy, &s->acc_energy, -energy_rate);
        // S4 盆栽：居家清醒时微量心情回复（亚整点，独立累加器）。
        room_happy_regen(s, (uint8_t) happy_rate);
        if (s->health < 80 && !s->sick) {
            meter_add_units(&s->health, &s->acc_health, PT_CFG_HEALTH_REGEN_DAY);
        }
    }

    if (s->sick) {
        meter_add_units(&s->happiness, &s->acc_happy, -PT_CFG_SICK_HAPPY_PER_H);
        // 默认模式：健康到 1 进入虚弱但不死亡（硬核模式留待后续）。
        if (s->health > 1) {
            meter_add_units(&s->health, &s->acc_health, -PT_CFG_SICK_HEALTH_PER_H);
        }
    }

    // ③ 排泄
    if (!s->sleeping && s->stage != PT_STAGE_EGG && s->next_poop > 0
        && minute >= s->next_poop) {
        if (s->poops < PT_CFG_POOP_MAX) {
            s->poops += 1;
            push(q, PT_EV_POOP, minute, s->poops, 0);
            if (s->poops == PT_CFG_POOP_MAX) {
                s->poop_sick_at = minute;
                if (!s->sick
                    && pt_rng_chance_permille(&s->rng_state, PT_CFG_POOP_SICK_PERMILLE)) {
                    make_sick(s, q, minute);
                }
            }
            schedule_poop(s, minute);
        } else {
            s->next_poop = -1;
        }
    }

    // ④ 呼叫：超时 → 小失误；归零再等一窗口 → 大失误；回升 → 重新武装
    if (!s->sleeping) {
        if ((s->active_call == PT_CALL_HUNGRY || s->active_call == PT_CALL_SAD)
            && minute - s->call_started >= (int32_t) call_window(s->stage)) {
            pt_need_t need = (s->active_call == PT_CALL_HUNGRY) ? PT_NEED_FULL
                                                               : PT_NEED_HAPPY;
            add_mistake(s, false);
            push(q, PT_EV_MISTAKE_SMALL, minute, (uint8_t) need, 0);
            push(q, PT_EV_CALL_EXPIRED, minute, (uint8_t) s->active_call, 0);
            s->need_state[need] = PT_NEED_NEGLECT;
            s->zero_since[need] = (need == PT_NEED_FULL)
                ? (s->fullness == 0 ? minute : -1)
                : (s->happiness == 0 ? minute : -1);
            s->active_call = PT_CALL_NONE;
        }
        service_need(s, q, PT_NEED_FULL, s->fullness, minute);
        service_need(s, q, PT_NEED_HAPPY, s->happiness, minute);
        maybe_raise_need_call(s, q, minute);
    }

    // ⑤ 卫生惩罚 / 满便致病 / 例行病
    if (s->poops > 0 && minute - s->poop_health_at >= PT_CFG_POOP_HEALTH_TICK_MIN) {
        if (s->health > 1) {
            int32_t hp = (int32_t) s->health - PT_CFG_POOP_HEALTH_TICK;
            s->health = (uint8_t) (hp < 1 ? 1 : hp);
        }
        s->poop_health_at = minute;
        if (s->poops >= PT_CFG_POOP_MAX && !s->sick) {
            uint32_t elapsed = (uint32_t) (minute - s->poop_sick_at);
            uint32_t p = PT_CFG_POOP_SICK_PERMILLE
                + PT_CFG_POOP_SICK_GROWTH_PERMILLE * (elapsed / PT_CFG_POOP_HEALTH_TICK_MIN);
            if (p > PT_CFG_POOP_SICK_CAP_PERMILLE) {
                p = PT_CFG_POOP_SICK_CAP_PERMILLE;
            }
            if (pt_rng_chance_permille(&s->rng_state, p)) {
                make_sick(s, q, minute);
            }
        }
    }
    if (s->sick_scheduled > 0 && minute >= s->sick_scheduled) {
        make_sick(s, q, minute);
        s->sick_scheduled = -1;
    }

    // ⑥ 清醒日界：年龄 +1 与每日配额重置
    int32_t day = day_index(minute);
    if (day != s->day_id) {
        s->day_id = day;
        if (s->stage != PT_STAGE_EGG) {
            s->age_days += 1;
            push(q, PT_EV_AGE_UP, minute, (uint8_t) s->age_days, 0);
        }
        s->snacks_today = 0;
        s->cleans_bond_today = 0;
        s->game_weight_today = 0;
        s->games_today = 0;
        for (uint8_t k = 0; k < PT_SKILL_COUNT; k += 1) {
            s->skill_today[k] = 0;
        }
        s->pats_in_window = 0;
        s->pat_window_start = minute;
    }

    // ⑦ 阶段 FSM 与进化
    int32_t elapsed = minute - s->stage_started;
    if (s->stage == PT_STAGE_EGG && elapsed >= PT_CFG_EGG_MIN) {
        s->weight = PT_CFG_WEIGHT_START;
        begin_stage(s, q, PT_STAGE_BABY, PT_SP_BABY, minute);
        push(q, PT_EV_HATCH, minute, (uint8_t) PT_STAGE_BABY, (uint8_t) PT_SP_BABY);
    } else if (s->stage == PT_STAGE_BABY && elapsed >= PT_CFG_BABY_MIN) {
        begin_stage(s, q, PT_STAGE_CHILD, PT_SP_CHILD, minute);
        push(q, PT_EV_STAGE_CHANGED, minute, (uint8_t) PT_STAGE_CHILD,
             (uint8_t) PT_SP_CHILD);
    } else if (s->stage == PT_STAGE_CHILD || s->stage == PT_STAGE_TEEN) {
        // 少年期至少撑到次日晨窗（避免在临窗时刻才达标、与设计时长打架）。
        int32_t min_duration = pt_stage_min_duration(s->stage);
        if (s->stage == PT_STAGE_TEEN) {
            min_duration += PT_CFG_TEEN_MORNING_SLACK_MIN;
        }
        if (!s->waiting_evolve && elapsed >= min_duration) {
            freeze_evolution(s, minute);   // 判定数据就此冻结
        }
        if (s->waiting_evolve && in_morning_window(minute)) {
            perform_evolve(s, q, minute);
        }
    } else if (s->stage == PT_STAGE_ADULT
               && elapsed >= PT_CFG_SENIOR_AFTER_DAYS * 1440) {
        s->stage = PT_STAGE_SENIOR;
        push(q, PT_EV_STAGE_CHANGED, minute, (uint8_t) PT_STAGE_SENIOR, 0);
    }

    // ⑧ 寿终
    if (s->death_minute > 0 && minute >= s->death_minute
        && s->stage != PT_STAGE_DEAD) {
        s->stage = PT_STAGE_DEAD;
        push(q, PT_EV_DEATH, minute, (uint8_t) s->species, 0);
    }
}

// ---------------------------------------------------------------------------
// 对外接口
// ---------------------------------------------------------------------------

void pt_state_new_egg_genome(pt_state_t *s, uint32_t seed, int32_t start_minute,
                             const pt_genome_t *g)
{
    memset(s, 0, sizeof(*s));
    pt_rng_seed(&s->rng_state, seed);
    s->minute = start_minute;
    s->boot_count = 1;
    s->stage = PT_STAGE_EGG;
    s->species = PT_SP_EGG;
    s->pending_species = PT_SP_NONE;
    s->stage_started = start_minute;
    s->day_id = day_index(start_minute);
    s->age_days = 0;
    s->death_minute = -1;

    s->fullness = 100;
    s->happiness = 100;
    s->health = 100;
    s->energy = 100;
    s->bond = 0;
    s->weight = PT_CFG_WEIGHT_START;

    s->active_call = PT_CALL_NONE;
    s->need_state[PT_NEED_FULL] = PT_NEED_ARMED;
    s->need_state[PT_NEED_HAPPY] = PT_NEED_ARMED;

    // L2：首发蛋基因（孵化前掷定，确定性走存档 RNG）；S3b 世代交替用育种基因。
    if (g != NULL) {
        s->genome = *g;
    } else {
        pt_genome_init_first(&s->genome, &s->rng_state);
    }
    s->zero_since[PT_NEED_FULL] = -1;
    s->zero_since[PT_NEED_HAPPY] = -1;

    s->next_poop = -1;
    s->sick_scheduled = -1;
    s->pat_window_start = start_minute;
}

void pt_state_new(pt_state_t *s, uint32_t seed, int32_t start_minute)
{
    pt_state_new_egg_genome(s, seed, start_minute, NULL);
}

static void apply_topup(pt_state_t *s, pt_events_t *q, uint8_t before, uint8_t after,
                        bool is_full, int32_t minute)
{
    if (before >= PT_CFG_TOPUP_FROM && before < 100 && after == 100) {
        if (is_full) {
            s->ledger.full_topups += 1;
            push(q, PT_EV_TOPUP, minute, (uint8_t) PT_NEED_FULL, 0);
        } else {
            s->ledger.happy_topups += 1;
            push(q, PT_EV_TOPUP, minute, (uint8_t) PT_NEED_HAPPY, 0);
        }
    }
}

static void add_weight(pt_state_t *s, int32_t delta)
{
    int32_t w = (int32_t) s->weight + delta;
    if (w < PT_CFG_WEIGHT_MIN) {
        w = PT_CFG_WEIGHT_MIN;
    }
    if (w > PT_CFG_WEIGHT_MAX) {
        w = PT_CFG_WEIGHT_MAX;
    }
    s->weight = (uint8_t) w;
}

// 维度③：成功喂食按标签计入阶段账本（非法标签按主食兜底）。
static void note_diet(pt_state_t *s, uint8_t tag)
{
    if (tag >= PT_FOOD_TAG_COUNT) {
        tag = PT_FOOD_TAG_MEAL;
    }
    if (s->ledger.diet[tag] < 0xFFFFu) {
        s->ledger.diet[tag] += 1;
    }
}

static uint8_t skill_gain_for(uint8_t grade)
{
    if (grade == PT_GAME_GRADE_PERFECT) {
        return PT_CFG_SKILL_GAIN_PERFECT;
    }
    if (grade == PT_GAME_GRADE_GREAT) {
        return PT_CFG_SKILL_GAIN_GREAT;
    }
    return PT_CFG_SKILL_GAIN_GOOD;
}

static uint8_t happy_gain_for(uint8_t grade, uint8_t games_today)
{
    if (games_today >= PT_CFG_GAME_HAPPY_FULL_N) {
        return PT_CFG_GAME_HAPPY_EXTRA;   // 衰减后保底 +5
    }
    if (grade == PT_GAME_GRADE_PERFECT) {
        return PT_CFG_GAME_HAPPY_PERFECT;
    }
    if (grade == PT_GAME_GRADE_GREAT) {
        return PT_CFG_GAME_HAPPY_GREAT;
    }
    return PT_CFG_GAME_HAPPY_GOOD;
}

void pt_handle_intent(pt_state_t *s, pt_events_t *q, pt_intent_kind_t kind,
                      uint8_t param)
{
    int32_t minute = s->minute;
    if (s->stage == PT_STAGE_DEAD) {
        return;
    }

    switch (kind) {
    case PT_INTENT_FEED_MEAL: {
        if (s->stage == PT_STAGE_EGG || s->stage == PT_STAGE_BABY
            || s->fullness >= PT_CFG_MEAL_REFUSE_AT) {
            push(q, PT_EV_REFUSED, minute, (uint8_t) kind, 0);
            return;
        }
        uint8_t before = s->fullness;
        int32_t next = (int32_t) s->fullness + PT_CFG_MEAL_FULL
                       + s_room_meal_bonus;
        s->fullness = (uint8_t) (next > 100 ? 100 : next);
        apply_topup(s, q, before, s->fullness, true, minute);
        add_weight(s, PT_CFG_MEAL_WEIGHT);
        note_diet(s, param);
        s->zero_since[PT_NEED_FULL] = -1;
        resolve_need_calls(s, q, minute);
        return;
    }
    case PT_INTENT_FEED_BOTTLE: {
        if (s->stage != PT_STAGE_BABY) {
            push(q, PT_EV_REFUSED, minute, (uint8_t) kind, 0);
            return;
        }
        uint8_t before = s->fullness;
        int32_t next = (int32_t) s->fullness + PT_CFG_BOTTLE_FULL;
        s->fullness = (uint8_t) (next > 100 ? 100 : next);
        apply_topup(s, q, before, s->fullness, true, minute);
        before = s->happiness;
        next = (int32_t) s->happiness + PT_CFG_BOTTLE_HAPPY;
        s->happiness = (uint8_t) (next > 100 ? 100 : next);
        apply_topup(s, q, before, s->happiness, false, minute);
        add_weight(s, PT_CFG_MEAL_WEIGHT);
        note_diet(s, PT_FOOD_TAG_MEAL);
        s->zero_since[PT_NEED_FULL] = -1;
        resolve_need_calls(s, q, minute);
        return;
    }
    case PT_INTENT_FEED_SNACK: {
        if (s->stage == PT_STAGE_EGG || s->stage == PT_STAGE_BABY) {
            push(q, PT_EV_REFUSED, minute, (uint8_t) kind, 0);
            return;
        }
        // 它正在求关注却塞零食：敷衍，扣亲密度 + 记账（仍有零食效果）。
        note_perfunctory(s);
        uint8_t before = s->happiness;
        int32_t next = (int32_t) s->happiness + PT_CFG_SNACK_HAPPY;
        s->happiness = (uint8_t) (next > 100 ? 100 : next);
        apply_topup(s, q, before, s->happiness, false, minute);
        next = (int32_t) s->fullness + PT_CFG_SNACK_FULL;
        s->fullness = (uint8_t) (next > 100 ? 100 : next);
        add_weight(s, PT_CFG_SNACK_WEIGHT);
        s->zero_since[PT_NEED_HAPPY] = -1;

        s->snacks_today += 1;
        if (s->snacks_today > PT_CFG_SNACK_SAFE_PER_DAY) {
            uint32_t p = PT_CFG_SNACK_SICK_PERMILLE;
            if (s->weight > PT_CFG_WEIGHT_OVERWEIGHT) {
                p *= 2;
            }
            if (pt_rng_chance_permille(&s->rng_state, p)) {
                make_sick(s, q, minute);
            }
        }
        resolve_need_calls(s, q, minute);
        note_diet(s, param);
        return;
    }
    case PT_INTENT_PAT: {
        if (minute - s->pat_window_start >= 60) {
            s->pat_window_start = minute;
            s->pats_in_window = 0;
        }
        if (s->pats_in_window >= PT_CFG_PAT_HOURLY_CAP) {
            return;
        }
        s->pats_in_window += 1;
        int32_t next = (int32_t) s->happiness + PT_CFG_PAT_HAPPY;
        uint8_t before = s->happiness;
        s->happiness = (uint8_t) (next > 100 ? 100 : next);
        apply_topup(s, q, before, s->happiness, false, minute);
        add_bond(s, PT_CFG_PAT_BOND);
        resolve_need_calls(s, q, minute);
        return;
    }
    case PT_INTENT_CLEAN: {
        if (s->poops > 0) {
            s->poops = 0;
            s->poop_sick_at = 0;
            s->poop_health_at = minute;
            schedule_poop(s, minute);
            if (s->cleans_bond_today < PT_CFG_CLEAN_BOND_DAY_CAP) {
                s->cleans_bond_today += 1;
                add_bond(s, 1);
            }
        }
        return;
    }
    case PT_INTENT_MEDICINE: {
        if (!s->sick) {
            return;
        }
        if (pt_rng_chance_permille(&s->rng_state, PT_CFG_MED_CURE_PERMILLE)) {
            s->sick = false;
            int32_t next = (int32_t) s->health + PT_CFG_MED_HEALTH;
            s->health = (uint8_t) (next > 100 ? 100 : next);
            push(q, PT_EV_CURED, minute, 0, 0);
        } else {
            push(q, PT_EV_MEDICINE_FAILED, minute, 0, 0);
        }
        return;
    }
    case PT_INTENT_LIGHTS_OFF:
        s->lights_off = true;
        if (s->active_call == PT_CALL_LIGHTS) {
            clear_call(s, q, minute, true);
        }
        return;
    case PT_INTENT_LIGHTS_ON:
        s->lights_off = false;
        return;
    case PT_INTENT_GAME_RESULT: {
        // 统一游戏结算（07 §4）：心情（前 3 局全额后 +5）、技能（每类日上限 9）、
        // 体重（日 −3 封顶）、体力（按游戏消耗）；G6 改给亲密度。金币由 L3 另记。
        if (s->sick || s->stage == PT_STAGE_EGG) {
            push(q, PT_EV_REFUSED, minute, (uint8_t) kind, 0);
            return;
        }
        uint8_t gid = PT_GAME_UNPACK_ID(param);
        uint8_t grade = PT_GAME_UNPACK_GRADE(param);
        if (gid >= PT_GAME_COUNT) {
            gid = PT_GAME_G1_HILO;
        }
        if (grade > PT_GAME_GRADE_PERFECT) {
            grade = PT_GAME_GRADE_GOOD;
        }
        // 它正在求关注却强行玩游戏：记一次敷衍。
        note_perfunctory(s);

        int32_t next = (int32_t) s->energy - pt_cfg_game_energy[gid];
        s->energy = (uint8_t) (next < 0 ? 0 : next);

        uint8_t before = s->happiness;
        uint8_t hg = happy_gain_for(grade, s->games_today);
        next = (int32_t) s->happiness + hg;
        s->happiness = (uint8_t) (next > 100 ? 100 : next);
        apply_topup(s, q, before, s->happiness, false, minute);

        uint8_t sk = pt_cfg_game_skill[gid];
        if (sk != 0xFFu) {
            uint8_t room_day = (uint8_t) (PT_CFG_SKILL_DAILY_CAP
                                          - s->skill_today[sk]);
            uint8_t gain = skill_gain_for(grade);
            if (gain > room_day) {
                gain = room_day;
            }
            uint8_t room_life = (uint8_t) (PT_CFG_SKILL_CAP - s->skill[sk]);
            if (gain > room_life) {
                gain = room_life;
            }
            s->skill[sk] = (uint8_t) (s->skill[sk] + gain);
            s->skill_today[sk] = (uint8_t) (s->skill_today[sk] + gain);
        }

        if (s->game_weight_today < PT_CFG_GAME_WEIGHT_MIN_CAP) {
            s->game_weight_today += 1;
            add_weight(s, -1);
        }

        if (gid == PT_GAME_G6_PREFER) {
            uint8_t b = grade == PT_GAME_GRADE_PERFECT ? PT_CFG_BOND_G6_PERFECT
                : grade == PT_GAME_GRADE_GREAT ? PT_CFG_BOND_G6_GREAT
                                               : PT_CFG_BOND_G6_GOOD;
            add_bond(s, b);
        }

        s->games_today += 1;
        s->zero_since[PT_NEED_HAPPY] = -1;
        resolve_need_calls(s, q, minute);
        return;
    }
    case PT_INTENT_PLAY_TOY: {
        // 玩具是"心情动作"：大开心、小耗体力；生病/蛋期拒绝（每日次数由经济层闸）。
        // 与 PAT 一样回应关注，不记敷衍；次数闸在 L3（PT_ECON_TOY_USES_DAY）。
        if (s->sick || s->stage == PT_STAGE_EGG) {
            push(q, PT_EV_REFUSED, minute, (uint8_t) kind, 0);
            return;
        }
        uint8_t before = s->happiness;
        int32_t next = (int32_t) s->happiness + PT_CFG_TOY_HAPPY;
        s->happiness = (uint8_t) (next > 100 ? 100 : next);
        apply_topup(s, q, before, s->happiness, false, minute);
        next = (int32_t) s->energy - PT_CFG_TOY_ENERGY;
        s->energy = (uint8_t) (next < 0 ? 0 : next);
        s->zero_since[PT_NEED_HAPPY] = -1;
        resolve_need_calls(s, q, minute);
        return;
    }
    case PT_INTENT_JOB_SHIFT: {
        // 打工只结算体力（07 §5.2）：成年/老年、非病、体力 ≥25 才能上班。
        // 工资是 L3 的事；心情/技能/体重不随工作变化（两套预算独立）。
        if (s->sick
            || (s->stage != PT_STAGE_ADULT && s->stage != PT_STAGE_SENIOR)
            || s->energy < PT_CFG_JOB_MIN_ENERGY) {
            push(q, PT_EV_REFUSED, minute, (uint8_t) kind, 0);
            return;
        }
        int32_t cost = param;
        if (cost > s->energy) {
            cost = s->energy;
        }
        s->energy = (uint8_t) (s->energy - cost);
        return;
    }
    default:
        return;
    }
}

bool pt_needs_call_active(const pt_state_t *s)
{
    return s->active_call != PT_CALL_NONE;
}

// 长期离家（离线超过 48h）的温柔结算：不死亡、不生病，成长暂停。
static void soft_land(pt_state_t *s, int32_t extra_minutes)
{
    int32_t days = extra_minutes / 1440;
    if (days > PT_CFG_OFFLINE_LONG_SMALL_CAP) {
        days = PT_CFG_OFFLINE_LONG_SMALL_CAP;
    }
    add_mistake(s, false);              // 只记 1 次小失误（days 仅作叙事强度）
    (void) days;

    if (s->fullness > PT_CFG_OFFLINE_FLOOR_FULL) {
        s->fullness = PT_CFG_OFFLINE_FLOOR_FULL;
    }
    if (s->happiness > PT_CFG_OFFLINE_FLOOR_HAPPY) {
        s->happiness = PT_CFG_OFFLINE_FLOOR_HAPPY;
    }
    if (s->health > PT_CFG_OFFLINE_FLOOR_HEALTH) {
        s->health = PT_CFG_OFFLINE_FLOOR_HEALTH;
    }
    s->energy = 100;
    add_weight(s, -PT_CFG_OFFLINE_WEIGHT_LOSS);

    s->sick = false;
    s->poops = 0;
    s->active_call = PT_CALL_NONE;
    s->pats_in_window = 0;

    // 成长与所有未来事件顺延：期间不老化、不进化、不寿终。
    s->stage_started += extra_minutes;
    if (s->next_poop > 0) {
        s->next_poop += extra_minutes;
    }
    if (s->sick_scheduled > 0) {
        s->sick_scheduled += extra_minutes;
    }
    if (s->death_minute > 0) {
        s->death_minute += extra_minutes;
    }
    if (s->waiting_evolve) {
        // 冻结的进化结果保留，等下一次晨间窗口演出（无需改动）。
    }
    int32_t day = day_index(s->minute + extra_minutes);
    s->day_id = day;
    s->sleeping = night_window(s->stage, minute_of_day(s->minute + extra_minutes));
    s->lights_off = s->sleeping;
}

void pt_advance_to(pt_state_t *s, pt_events_t *q, int32_t target_minute)
{
    if (target_minute <= s->minute || s->stage == PT_STAGE_DEAD) {
        return;
    }
    int32_t cap = s->minute + PT_CFG_OFFLINE_CAP_MIN;
    int32_t normal_end = target_minute < cap ? target_minute : cap;
    while (s->minute < normal_end) {
        step_minute(s, q);
    }
    if (target_minute > normal_end) {
        int32_t extra = target_minute - normal_end;
        s->minute = target_minute;
        soft_land(s, extra);
    }
}
