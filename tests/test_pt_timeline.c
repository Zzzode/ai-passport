// 主机测试：时间模型——逐分钟推进与离线大跨度回放必须完全一致（设计 03 §9、11 §3）。
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pt_config.h"
#include "pt_engine.h"
#include "pt_events.h"

#define START (10 * 1440 + 360)

static bool state_equal(const pt_state_t *a, const pt_state_t *b)
{
    return memcmp(a, b, sizeof(*a)) == 0;
}

// 逐分钟推进 N 分钟（模拟"设备一直开着"）。
static void run_minute_by_minute(pt_state_t *s, int32_t minutes)
{
    pt_events_t q;
    pt_events_init(&q);
    for (int32_t i = 0; i < minutes; i += 1) {
        pt_advance_to(s, &q, s->minute + 1);
        pt_events_clear(&q);
    }
}

// 一次性推进 N 分钟（模拟"关机后开机回放"）。
static void run_in_one_jump(pt_state_t *s, int32_t minutes)
{
    pt_events_t q;
    pt_events_init(&q);
    pt_advance_to(s, &q, s->minute + minutes);
    pt_events_clear(&q);
}

static void test_replay_matches_minute_by_minute(void)
{
    // 覆盖多个阶段边界与睡眠窗口的 6 小时区间。
    const int32_t span = 6 * 60;

    pt_state_t a, b;
    pt_state_new(&a, 4242u, START);
    pt_state_new(&b, 4242u, START);

    run_minute_by_minute(&a, span);
    run_in_one_jump(&b, span);

    assert(state_equal(&a, &b));
}

static void test_replay_matches_across_stage_boundaries(void)
{
    // 覆盖 蛋→婴儿→幼儿→少年 的整段（含离线 48h 封顶以内的最长区间）。
    const int32_t span = PT_CFG_EGG_MIN + PT_CFG_BABY_MIN + PT_CFG_CHILD_MIN + 720;

    pt_state_t a, b;
    pt_state_new(&a, 99u, START);
    pt_state_new(&b, 99u, START);

    run_minute_by_minute(&a, span);
    run_in_one_jump(&b, span);

    assert(state_equal(&a, &b));
}

static void test_replay_with_care_stays_consistent(void)
{
    // 有照料介入时也必须一致：两路各自施加相同的照料序列。
    const int32_t span = 12 * 60;
    pt_state_t a, b;
    pt_state_new(&a, 7u, START);
    pt_state_new(&b, 7u, START);

    pt_events_t qa, qb;
    pt_events_init(&qa);
    pt_events_init(&qb);

    for (int32_t i = 0; i < span; i += 15) {
        pt_advance_to(&a, &qa, a.minute + 15);
        pt_advance_to(&b, &qb, b.minute + 15);
        // 完全相同的照料动作应产生完全相同的状态。
        if (a.fullness < 80) {
            pt_handle_intent(&a, &qa, PT_INTENT_FEED_MEAL, 0);
            pt_handle_intent(&b, &qb, PT_INTENT_FEED_MEAL, 0);
        }
        if (a.poops > 0) {
            pt_handle_intent(&a, &qa, PT_INTENT_CLEAN, 0);
            pt_handle_intent(&b, &qb, PT_INTENT_CLEAN, 0);
        }
        pt_events_clear(&qa);
        pt_events_clear(&qb);
    }
    assert(state_equal(&a, &b));
}

// ---- 性能与离线封顶：48h 封顶之外走温柔结算 ----
static void test_long_absence_is_gentle(void)
{
    pt_state_t s;
    pt_state_new(&s, 5u, START);
    pt_events_t q;
    pt_events_init(&q);

    // 先养到幼儿期避免蛋期边界干扰。
    pt_advance_to(&s, &q, s.minute + PT_CFG_EGG_MIN + PT_CFG_BABY_MIN);
    pt_events_clear(&q);

    // 关机 72 小时（超过 48h 封顶）。
    int32_t before = s.minute;
    pt_advance_to(&s, &q, before + 72 * 60);

    assert(s.minute == before + 72 * 60);
    assert(s.stage != PT_STAGE_DEAD);                      // 默认模式不死亡
    assert(s.fullness <= PT_CFG_OFFLINE_FLOOR_FULL + 1);   // 落到温柔下限附近
    assert(s.happiness <= PT_CFG_OFFLINE_FLOOR_HAPPY + 1);
    assert(s.health >= 1);
    assert(s.ledger.small <= 2);                           // 失误封顶
    assert(!s.sick);                                       // 期间不生病
    assert(s.poops == 0);
}

// ---- 时间套利：向后拨钟不回滚、不复制奖励 ----
static void test_clock_rollback_does_not_rewind(void)
{
    pt_state_t s;
    pt_state_new(&s, 8u, START);
    pt_events_t q;
    pt_events_init(&q);

    pt_advance_to(&s, &q, s.minute + 120);
    pt_events_clear(&q);
    uint16_t age_before = s.age_days;
    uint8_t full_before = s.fullness;

    // 回拨 2 小时：advance_to 目标小于当前时刻应直接返回，状态原样。
    pt_advance_to(&s, &q, s.minute - 120);
    assert(s.age_days == age_before);
    assert(s.fullness == full_before);
    assert(s.minute == START + 120);
}

// ---- 寿终发生在离线大跨度中途：不得被温柔结算复活或错乱 ----
static void test_death_during_long_jump_stays_dead(void)
{
    pt_state_t s;
    pt_state_new(&s, 11u, START);
    pt_events_t q;
    pt_events_init(&q);

    pt_advance_to(&s, &q, s.minute + PT_CFG_EGG_MIN + PT_CFG_BABY_MIN);
    // 直接构造一个成年个体与临近的寿终时刻。
    s.stage = PT_STAGE_ADULT;
    s.species = PT_SP_ADULT_NORMAL;
    s.stage_started = s.minute;
    s.death_minute = s.minute + 100;

    pt_advance_to(&s, &q, s.minute + 3000);   // 一次性跨过寿终
    assert(s.stage == PT_STAGE_DEAD);

    // 死后推进不应改变状态。
    pt_state_t frozen = s;
    pt_advance_to(&s, &q, s.minute + 500);
    assert(state_equal(&s, &frozen));
}

// P2-S3b：父母同住期夜间衰减 ×90%（08 §4）。
static void test_family_care_night_decay(void)
{
    const int32_t night = 10 * 1440 + 21 * 60 + 30;  // 21:30，CHILD 已就寝段
    pt_state_t a, b;
    pt_state_new(&a, 7u, night);
    pt_state_new(&b, 7u, night);
    a.stage = PT_STAGE_CHILD;
    a.stage_started = night - 1000;
    a.lights_off = true;
    b.stage = PT_STAGE_CHILD;
    b.stage_started = night - 1000;
    b.lights_off = true;

    pt_engine_set_family_care(false);
    pt_events_t qa;
    pt_events_init(&qa);
    pt_advance_to(&a, &qa, night + 120);

    pt_engine_set_family_care(true);
    pt_events_t qb;
    pt_events_init(&qb);
    pt_advance_to(&b, &qb, night + 120);
    pt_engine_set_family_care(false);

    // 120 分钟夜间：有照顾的饱腹损失严格更少（90%）。
    assert(b.acc_full > a.acc_full);
    // 睡眠体力回复不受影响。
    assert(b.acc_energy == a.acc_energy);
}

int main(void)
{
    test_replay_matches_minute_by_minute();
    test_replay_matches_across_stage_boundaries();
    test_replay_with_care_stays_consistent();
    test_long_absence_is_gentle();
    test_clock_rollback_does_not_rewind();
    test_death_during_long_jump_stays_dead();
    test_family_care_night_decay();
    printf("test_pt_timeline: PASS\n");
    return 0;
}
