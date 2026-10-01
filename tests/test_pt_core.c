// 主机测试：生命系统行为（孵化、成长、呼叫失误、卫生疾病、完美照顾路径）。
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "pt_config.h"
#include "pt_engine.h"
#include "pt_events.h"

// 从一个清爽的白天开始（第 10 天 06:00，正好是清醒日界与晨窗起点）。
#define START (10 * 1440 + 360)

static pt_events_t g_q;

static void drain_events(void)
{
    pt_event_t ev;
    while (pt_events_take(&g_q, &ev)) {
        (void) ev;
    }
}

static void advance(pt_state_t *s, int32_t minutes)
{
    pt_advance_to(s, &g_q, s->minute + minutes);
    drain_events();
}

static int count_events_of(pt_event_kind_t kind)
{
    // 事件在 advance 内被清空，这里改用回调式统计：重放一次不现实，
    // 因此测试改为在各场景内就地统计，本函数仅作占位提示。
    (void) kind;
    return 0;
}

// ---- 场景 1：孵化与婴儿期 ----
static void test_hatch_and_baby_stage(void)
{
    pt_state_t s;
    pt_state_new(&s, 1u, START);
    assert(s.stage == PT_STAGE_EGG);
    assert(s.species == PT_SP_EGG);

    advance(&s, PT_CFG_EGG_MIN - 1);
    assert(s.stage == PT_STAGE_EGG);

    advance(&s, 1);
    assert(s.stage == PT_STAGE_BABY);
    assert(s.species == PT_SP_BABY);
    assert(s.weight == PT_CFG_WEIGHT_START);
    assert(s.age_days == 0);

    advance(&s, PT_CFG_BABY_MIN);
    assert(s.stage == PT_STAGE_CHILD);
    assert(s.species == PT_SP_CHILD);
}

// ---- 场景 2：喂食、拒绝与顶满记账 ----
static void test_feeding_and_topup(void)
{
    pt_state_t s;
    pt_state_new(&s, 2u, START);
    advance(&s, PT_CFG_EGG_MIN);

    // 婴儿期只吃奶瓶；正餐被拒。
    pt_handle_intent(&s, &g_q, PT_INTENT_FEED_MEAL, 0);
    bool refused = false;
    pt_event_t ev;
    while (pt_events_take(&g_q, &ev)) {
        if (ev.kind == PT_EV_REFUSED) {
            refused = true;
        }
    }
    assert(refused);

    // 满腹时补满不计顶满。
    pt_handle_intent(&s, &g_q, PT_INTENT_FEED_BOTTLE, 0);
    drain_events();
    assert(s.ledger.full_topups == 0);
    assert(s.fullness == 100);

    // 衰减半小时后补满：从 ≥75 到 100，计一次顶满并增重。
    uint8_t weight_before = s.weight;
    advance(&s, 30);
    assert(s.fullness < 100);
    pt_handle_intent(&s, &g_q, PT_INTENT_FEED_BOTTLE, 0);
    drain_events();
    assert(s.fullness == 100);
    assert(s.ledger.full_topups == 1);
    assert(s.weight == weight_before + PT_CFG_MEAL_WEIGHT);
}

// ---- 场景 3：呼叫超时记小失误，归零再等一个窗口记大失误 ----
static void test_calls_and_mistakes(void)
{
    pt_state_t s;
    pt_state_new(&s, 3u, START);
    advance(&s, PT_CFG_EGG_MIN + PT_CFG_BABY_MIN);   // 直接进入幼儿期

    int32_t child_start = s.minute;
    advance(&s, 800);                                 // 疏忽约 13 小时（白天）
    assert(s.minute - child_start == 800);
    assert(s.ledger.small >= 1);
    assert(s.need_state[PT_NEED_FULL] != PT_NEED_ARMED
           || s.ledger.small >= 1);

    advance(&s, 60);
    assert(s.ledger.big >= 1);
}

// ---- 场景 4：便便、清理与生病治疗 ----
static void test_poop_and_sickness(void)
{
    pt_state_t s;
    pt_state_new(&s, 4u, START);
    advance(&s, PT_CFG_EGG_MIN + PT_CFG_BABY_MIN + 400);
    assert(s.poops > 0);

    pt_handle_intent(&s, &g_q, PT_INTENT_CLEAN, 0);
    drain_events();
    assert(s.poops == 0);

    // 挂满 4 泡并久置必然致病（含 50% 起始概率的上升路径）。
    advance(&s, 900);
    assert(s.sick);

    // 反复喂药直到痊愈（70% 单次成功率）。
    int try_count = 0;
    while (s.sick && try_count < 50) {
        pt_handle_intent(&s, &g_q, PT_INTENT_MEDICINE, 0);
        drain_events();
        try_count += 1;
    }
    assert(!s.sick);
    assert(try_count <= 50);
}

// ---- 场景 5：疏忽照顾走 NEGLECT 分支 ----
static void test_neglect_evolution_branch(void)
{
    pt_state_t s;
    pt_state_new(&s, 5u, START);
    advance(&s, PT_CFG_EGG_MIN + PT_CFG_BABY_MIN + 1600);

    // 幼儿→少年在晨窗演出；疏忽路线应落到 TEEN_C。
    assert(s.stage == PT_STAGE_TEEN);
    assert(s.species == PT_SP_TEEN_C);
}

// ---- 场景 6：完美照顾路径可达 PERFECT 成年 ----
static void care_bot(pt_state_t *s)
{
    // 完美照料策略：补到顶（顶满计数要求从 ≥75 补到 100），清便便、看病、睡前关灯。
    if (s->fullness < PT_CFG_MEAL_REFUSE_AT) {
        pt_handle_intent(s, &g_q, PT_INTENT_FEED_MEAL, 0);
    }
    if (s->happiness < 100) {
        pt_handle_intent(s, &g_q, PT_INTENT_GAME_RESULT,
                         PT_GAME_PACK(PT_GAME_G1_HILO, PT_GAME_GRADE_PERFECT));
    }
    if (s->poops > 0) {
        pt_handle_intent(s, &g_q, PT_INTENT_CLEAN, 0);
    }
    if (s->sick) {
        pt_handle_intent(s, &g_q, PT_INTENT_MEDICINE, 0);
    }
    if (!s->lights_off) {
        int32_t mod = s->minute % 1440;
        if (mod >= PT_CFG_SLEEP_CHILD_START) {
            pt_handle_intent(s, &g_q, PT_INTENT_LIGHTS_OFF, 0);
        }
    }
    drain_events();
}

static void test_perfect_care_reaches_perfect_adult(void)
{
    pt_state_t s;
    pt_state_new(&s, 6u, START);

    // 约 4 天的完美照料（覆盖 蛋→婴儿→幼儿→少年→成年）。
    for (int i = 0; i < 4 * 24 * 4; i += 1) {
        advance(&s, 15);
        care_bot(&s);
    }

    assert(s.stage == PT_STAGE_ADULT);
    assert(s.species == PT_SP_ADULT_PERFECT);
    assert(s.ledger.small == 0);
    assert(s.ledger.big == 0);
    assert(s.health > 0);
    assert(s.death_minute > 0);
}

// ---- 场景 7：夜间忘关灯：呼叫超时 = 1 小失误，当晚睡眠打折，次日体力 70 ----
static void test_lights_out_timeout(void)
{
    pt_state_t s;
    pt_state_new(&s, 21u, START);
    advance(&s, PT_CFG_EGG_MIN + PT_CFG_BABY_MIN);   // 幼儿期，约 07:05

    // 白天逐分钟照料真实需求（但绝不开灯/关灯），让 21:00 的关灯呼叫能挂起。
    int32_t night_start = (s.minute / 1440) * 1440 + PT_CFG_SLEEP_CHILD_START;
    while (s.minute < night_start) {
        pt_advance_to(&s, &g_q, s.minute + 1);
        if (s.active_call == PT_CALL_HUNGRY) {
            pt_handle_intent(&s, &g_q, PT_INTENT_FEED_MEAL, 0);
        } else if (s.active_call == PT_CALL_SAD) {
            pt_handle_intent(&s, &g_q, PT_INTENT_PAT, 0);
        }
        drain_events();
    }
    assert(!s.sleeping);

    int16_t small_before = s.ledger.small;

    // 关灯呼叫窗口（幼儿 30 分钟）内不管它，但持续处理真实需求，
    // 让 HUNGRY/SAD 呼叫尽快让位给 LIGHTS 呼叫，且只多记关灯这 1 次失误。
    int32_t guard = night_start + 120;
    while (s.minute < guard) {
        pt_advance_to(&s, &g_q, s.minute + 1);
        if (s.active_call == PT_CALL_HUNGRY) {
            pt_handle_intent(&s, &g_q, PT_INTENT_FEED_MEAL, 0);
        } else if (s.active_call == PT_CALL_SAD) {
            pt_handle_intent(&s, &g_q, PT_INTENT_PAT, 0);
        }
        drain_events();
        if (s.sleeping) {
            break;
        }
    }
    assert(s.sleeping);
    assert(s.poor_sleep);
    assert(s.ledger.small == small_before + 1);

    // 睡到次日清醒日界 06:00：体力被钳到 70，poor_sleep 清除。
    int32_t morning = (s.minute / 1440 + 1) * 1440 + PT_CFG_NIGHT_END_MIN;
    pt_advance_to(&s, &g_q, morning + 1);
    drain_events();
    assert(!s.sleeping);
    assert(!s.poor_sleep);
    assert(s.energy == PT_CFG_POOR_SLEEP_NUM);
}

// ---- 场景 8：寿终与终态 ----
static void test_lifespan_death(void)
{
    pt_state_t s;
    pt_state_new(&s, 7u, START);

    // 先养到成年（完美路线），再快进超过寿命。
    for (int i = 0; i < 4 * 24 * 4; i += 1) {
        advance(&s, 15);
        care_bot(&s);
    }
    assert(s.stage == PT_STAGE_ADULT);
    int32_t death_in = s.death_minute - s.minute;
    assert(death_in > 0);

    int32_t remaining = death_in + 60;
    while (remaining > 0 && s.stage != PT_STAGE_DEAD) {
        int32_t chunk = remaining > 1440 ? 1440 : remaining;   // 每段 ≤ 离线上限
        advance(&s, chunk);
        remaining -= chunk;
        if (s.stage == PT_STAGE_DEAD) {
            break;
        }
        care_bot(&s);
    }
    assert(s.stage == PT_STAGE_DEAD);
}

// ---- 场景 9：小游戏统一结算（07 §4）：心情衰减/技能日上限/体力/体重/G6 亲密度 ----
static void test_game_budget_and_skills(void)
{
    pt_state_t s;
    pt_state_new(&s, 11u, START);
    advance(&s, PT_CFG_EGG_MIN + PT_CFG_BABY_MIN);   // 幼儿期
    assert(s.stage == PT_STAGE_CHILD);
    s.energy = 100;
    s.happiness = 0;

    // 前 3 局 PERFECT 全额 +20 心情，MIND 各 +3（累计 9，触顶每日上限）。
    for (int i = 0; i < 3; i += 1) {
        pt_handle_intent(&s, &g_q, PT_INTENT_GAME_RESULT,
                         PT_GAME_PACK(PT_GAME_G1_HILO, PT_GAME_GRADE_PERFECT));
        drain_events();
    }
    assert(s.happiness == 60);
    assert(s.skill[PT_SKILL_MIND] == 9);
    assert(s.skill_today[PT_SKILL_MIND] == 9);
    assert(s.energy == 100 - 3 * 5);
    assert(s.games_today == 3);

    // 第 4 局：心情衰减为 +5；MIND 日上限已满，技能不再增加。
    pt_handle_intent(&s, &g_q, PT_INTENT_GAME_RESULT,
                     PT_GAME_PACK(PT_GAME_G1_HILO, PT_GAME_GRADE_PERFECT));
    drain_events();
    assert(s.happiness == 65);
    assert(s.skill[PT_SKILL_MIND] == 9);
    assert(s.energy == 100 - 4 * 5);

    // G2 属 ART：+3 技能、体力消耗 10。
    pt_handle_intent(&s, &g_q, PT_INTENT_GAME_RESULT,
                     PT_GAME_PACK(PT_GAME_G2_RHYTHM, PT_GAME_GRADE_GREAT));
    drain_events();
    assert(s.skill[PT_SKILL_ART] == 2);
    assert(s.energy == 100 - 4 * 5 - 10);

    // 生病时游戏被拒：局数/技能/体力都不变。
    s.sick = true;
    pt_handle_intent(&s, &g_q, PT_INTENT_GAME_RESULT,
                     PT_GAME_PACK(PT_GAME_G3_CATCH, PT_GAME_GRADE_PERFECT));
    bool refused = false;
    pt_event_t ev;
    while (pt_events_take(&g_q, &ev)) {
        refused = refused || ev.kind == PT_EV_REFUSED;
    }
    assert(refused);
    assert(s.games_today == 5 && s.skill[PT_SKILL_BODY] == 0);
    s.sick = false;

    // G6 不给技能，给亲密度（PERFECT +8，幼儿期上限 60）。
    uint8_t bond_before = s.bond;
    pt_handle_intent(&s, &g_q, PT_INTENT_GAME_RESULT,
                     PT_GAME_PACK(PT_GAME_G6_PREFER, PT_GAME_GRADE_PERFECT));
    drain_events();
    assert(s.skill[PT_SKILL_MIND] == 9 && s.skill[PT_SKILL_BODY] == 0);
    assert(s.bond == bond_before + PT_CFG_BOND_G6_PERFECT);

    // 体重每日最多因游戏 -3：已打 6 局，只掉 3。
    assert(s.game_weight_today == 3);

    // 终身上限 99：BODY 已满时不再增长（日计数也不虚增）。
    s.skill[PT_SKILL_BODY] = PT_CFG_SKILL_CAP;
    s.skill_today[PT_SKILL_BODY] = 0;
    pt_handle_intent(&s, &g_q, PT_INTENT_GAME_RESULT,
                     PT_GAME_PACK(PT_GAME_G3_CATCH, PT_GAME_GRADE_GOOD));
    drain_events();
    assert(s.skill[PT_SKILL_BODY] == 99);
    assert(s.skill_today[PT_SKILL_BODY] == 0);

    // 维度③：成功喂食按标签入阶段账本。
    s.fullness = 50;
    pt_handle_intent(&s, &g_q, PT_INTENT_FEED_MEAL, PT_FOOD_TAG_VEG);
    drain_events();
    assert(s.ledger.diet[PT_FOOD_TAG_VEG] == 1);
    assert(s.ledger.diet[PT_FOOD_TAG_MEAL] == 0);
}

// ---- 场景 10：打工班次（07 §5.2）：成年/体力闸/只耗体力，工资在经济层 ----
static void test_job_shift_gates(void)
{
    pt_state_t s;
    pt_state_new(&s, 13u, START);
    for (int i = 0; i < 4 * 24 * 4; i += 1) {   // 完美照料养到成年
        advance(&s, 15);
        care_bot(&s);
    }
    assert(s.stage == PT_STAGE_ADULT);
    s.energy = 80;
    s.sick = false;

    // 正常班次：只扣体力，心情/技能/体重/局数都不动。
    uint8_t happy0 = s.happiness;
    uint8_t games0 = s.games_today;
    uint8_t mind0 = s.skill[PT_SKILL_MIND];
    pt_handle_intent(&s, &g_q, PT_INTENT_JOB_SHIFT, 8);
    drain_events();
    assert(s.energy == 72 && s.happiness == happy0);
    assert(s.games_today == games0 && s.skill[PT_SKILL_MIND] == mind0);

    // 体力 <25 拒绝，不扣体力。
    s.energy = 24;
    pt_handle_intent(&s, &g_q, PT_INTENT_JOB_SHIFT, 8);
    bool refused = false;
    pt_event_t ev;
    while (pt_events_take(&g_q, &ev)) {
        refused = refused || ev.kind == PT_EV_REFUSED;
    }
    assert(refused && s.energy == 24);

    // 生病拒绝。
    s.energy = 80;
    s.sick = true;
    pt_handle_intent(&s, &g_q, PT_INTENT_JOB_SHIFT, 8);
    refused = false;
    while (pt_events_take(&g_q, &ev)) {
        refused = refused || ev.kind == PT_EV_REFUSED;
    }
    assert(refused && s.energy == 80);
    s.sick = false;

    // 超扣参数被钳到当前体力（不为负）。
    s.energy = 30;
    pt_handle_intent(&s, &g_q, PT_INTENT_JOB_SHIFT, 40);
    drain_events();
    assert(s.energy == 0);
}

// ---- S4 房间增益（功能家具注入引擎的运行期参数，05 §7.2） ----
static pt_state_t grow_adult(uint32_t seed)
{
    pt_state_t s;
    pt_state_new(&s, seed, START);
    for (int i = 0; i < 4 * 24 * 4; i += 1) {
        advance(&s, 15);
        care_bot(&s);
    }
    assert(s.stage == PT_STAGE_ADULT);
    drain_events();
    return s;
}

static void test_room_buff_meal(void)
{
    pt_engine_set_room_buffs(0, 0, 100);
    pt_state_t base = grow_adult(21u);

    // 厨房摆件：正餐额外 +3 饱腹。
    pt_state_t s = base;
    s.fullness = 40;
    s.acc_full = 0;
    pt_engine_set_room_buffs(3, 0, 100);
    pt_handle_intent(&s, &g_q, PT_INTENT_FEED_MEAL, PT_FOOD_TAG_MEAL);
    drain_events();
    assert(s.fullness == 68);

    // 复位后恢复标准 +25。
    s = base;
    s.fullness = 40;
    s.acc_full = 0;
    pt_engine_set_room_buffs(0, 0, 100);
    pt_handle_intent(&s, &g_q, PT_INTENT_FEED_MEAL, PT_FOOD_TAG_MEAL);
    drain_events();
    assert(s.fullness == 65);
}

static void test_room_buff_happy_regen(void)
{
    pt_state_t base = grow_adult(22u);
    int32_t day0 = base.minute - base.minute % 1440;

    pt_state_t a = base;
    pt_state_t b = base;
    // 固定为成年（避免 10 小时窗内进入老年改衰减率），11:00 起 10 小时清醒窗
    // （避开 06–10 晨窗与 22:00 入睡）。
    a.stage = b.stage = PT_STAGE_ADULT;
    a.waiting_evolve = b.waiting_evolve = false;
    a.minute = b.minute = day0 + 11 * 60;
    a.happiness = b.happiness = 80;
    a.acc_happy = b.acc_happy = 0;
    a.fullness = b.fullness = 100;
    a.acc_full = b.acc_full = 0;
    // 屏蔽 10 小时窗内的排便积病/例行病：本测试只验心情衰减/回复算术。
    a.poops = b.poops = 0;
    a.next_poop = b.next_poop = a.minute + 100000;
    a.sick_scheduled = b.sick_scheduled = a.minute + 100000;
    a.sick = b.sick = false;

    pt_engine_set_room_buffs(0, 0, 100);
    advance(&a, 600);                       // 成年心情衰减 4/时 × 10h = 40
    pt_engine_set_room_buffs(0, 10, 100);   // 盆栽：10% 速率 = 0.4/时
    advance(&b, 600);
    pt_engine_set_room_buffs(0, 0, 100);

    assert(a.stage == PT_STAGE_ADULT && b.stage == PT_STAGE_ADULT);
    assert(a.happiness == 40);
    assert(b.happiness == 44);              // 多回 4 点，且永不超量
}

static void test_room_buff_energy_regen(void)
{
    pt_state_t base = grow_adult(23u);

    // 摆到当晚 23:00，灯已关 → 下一分钟入睡。
    int32_t night = base.minute - base.minute % 1440 + 23 * 60;
    if (night <= base.minute) {
        night += 1440;
    }
    pt_state_t a = base;
    pt_state_t b = base;
    a.minute = b.minute = night;
    a.energy = b.energy = 50;
    a.acc_energy = b.acc_energy = 0;
    a.sleeping = b.sleeping = false;
    a.lights_off = b.lights_off = true;
    a.active_call = b.active_call = PT_CALL_NONE;

    pt_engine_set_room_buffs(0, 0, 100);
    advance(&a, 60);                        // 35/时
    pt_engine_set_room_buffs(0, 0, 110);    // 健身角：35×110/100 = 38/时
    advance(&b, 60);
    pt_engine_set_room_buffs(0, 0, 100);

    assert(a.sleeping && b.sleeping);
    assert(a.energy == 85);
    assert(b.energy == 88);
}

int main(void)
{
    pt_events_init(&g_q);
    pt_engine_set_room_buffs(0, 0, 100);
    test_hatch_and_baby_stage();
    test_feeding_and_topup();
    test_calls_and_mistakes();
    test_poop_and_sickness();
    test_neglect_evolution_branch();
    test_perfect_care_reaches_perfect_adult();
    test_lights_out_timeout();
    test_lifespan_death();
    test_game_budget_and_skills();
    test_job_shift_gates();
    test_room_buff_meal();
    test_room_buff_happy_regen();
    test_room_buff_energy_regen();
    pt_engine_set_room_buffs(0, 0, 100);
    (void) count_events_of;
    printf("test_pt_core: PASS\n");
    return 0;
}
