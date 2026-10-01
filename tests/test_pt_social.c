// 主机测试：P2-S3a 离线社交状态机（designs 08 §1–§4）。
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pt_config.h"
#include "pt_engine.h"
#include "pt_events.h"
#include "pt_social.h"
#include "pt_types.h"

// day1 08:00（清醒日界 06:00 之后）。
#define DAY1_M    (1440 + 8 * 60)
#define DAY(d)    (DAY1_M + (d) * 1440)

static pt_state_t adult_state(int32_t minute, uint8_t age_days)
{
    pt_state_t s;
    memset(&s, 0, sizeof(s));
    s.stage = PT_STAGE_ADULT;
    s.minute = minute;
    s.age_days = age_days;
    pt_genome_init_first(&s.genome, (uint32_t[1]){ 12345u });
    return s;
}

static int pop_kind(pt_events_t *q, pt_event_kind_t k)
{
    int n = 0;
    pt_event_t ev;
    while (pt_events_take(q, &ev)) {
        if (ev.kind == k) n += 1;
    }
    return n;
}

int main(void)
{
    // ---- 初始化：单身、无候选 ----
    pt_social_t so;
    pt_social_init(&so, 42, 1);
    assert(so.phase == PT_SOC_SINGLE);
    assert(pt_social_candidate_count(&so) == 0);

    // 关系阶段阈值
    assert(pt_social_rel_stage(0) == PT_REL_MET);
    assert(pt_social_rel_stage(24) == PT_REL_MET);
    assert(pt_social_rel_stage(25) == PT_REL_FRIEND);
    assert(pt_social_rel_stage(50) == PT_REL_CRUSH);
    assert(pt_social_rel_stage(80) == PT_REL_LOVE);

    // 名字确定性派生
    char n1[8], n2[8];
    pt_social_name(&so.cand[0].genome, n1, sizeof(n1));
    pt_genome_roll(&so.cand[0].genome, 0xFF, 0, &so.rng_state);
    pt_social_name(&so.cand[0].genome, n1, sizeof(n1));
    pt_social_name(&so.cand[0].genome, n2, sizeof(n2));
    assert(strcmp(n1, n2) == 0);
    assert(strlen(n1) == 4);
    assert(n1[0] >= 'A' && n1[0] <= 'Z');

    // ---- 成年第 1 天（age_days=1）即推荐；未成年/首日不推 ----
    pt_events_t q;
    pt_events_init(&q);
    pt_state_t teen = adult_state(DAY1_M, 1);
    teen.stage = PT_STAGE_TEEN;
    pt_social_on_minute(&so, &teen, &q);
    assert(pt_social_candidate_count(&so) == 0);

    pt_state_t ad = adult_state(DAY1_M, 0);
    pt_social_on_minute(&so, &ad, &q);
    assert(pt_social_candidate_count(&so) == 0);

    ad.age_days = 1;
    pt_social_on_minute(&so, &ad, &q);
    assert(pt_social_candidate_count(&so) == 1);
    assert(pop_kind(&q, PT_EV_SOC_CANDIDATE) == 1);

    // 同一天不重复推荐
    ad.minute += 60;
    pt_social_on_minute(&so, &ad, &q);
    assert(pt_social_candidate_count(&so) == 1);
    assert(pop_kind(&q, PT_EV_SOC_CANDIDATE) == 0);

    // ---- 打招呼：+5、冷却 120m、每日 3 次 ----
    assert(so.cand[0].bond == 0);
    assert(pt_social_greet(&so, 0, DAY1_M) == PT_SOC_OK);
    assert(so.cand[0].bond == 5);
    assert(pt_social_greet(&so, 0, DAY1_M + 60) == PT_SOC_COOLDOWN);
    assert(pt_social_greet(&so, 0, DAY1_M + 120) == PT_SOC_OK);
    assert(so.cand[0].bond == 10);
    assert(pt_social_greet(&so, 0, DAY1_M + 240) == PT_SOC_OK);
    assert(so.cand[0].bond == 15);
    assert(pt_social_greet(&so, 0, DAY1_M + 360) == PT_SOC_CAPPED);
    assert(so.cand[0].bond == 15);

    // 非法槽位/相位
    assert(pt_social_greet(&so, 2, DAY1_M + 400) == PT_SOC_BAD_SLOT);

    // ---- 衰减：宽限 3 天，之后 -5/天 ----
    {
        pt_social_t s2;
        pt_social_init(&s2, 7, 1);
        pt_events_t q2;
        pt_events_init(&q2);
        pt_state_t a2 = adult_state(DAY1_M, 1);
        pt_social_on_minute(&s2, &a2, &q2);   // day1 落候选
        assert(s2.cand[0].valid);
        s2.cand[0].bond = 30;
        s2.cand[0].last_day = 1;
        // 直接结算到 day5：day5 距 last=4 > 3 → -5
        a2.minute = DAY(4);
        pt_social_on_minute(&s2, &a2, &q2);
        assert(s2.cand[0].bond == 25);
        a2.minute = DAY(5);
        pt_social_on_minute(&s2, &a2, &q2);
        assert(s2.cand[0].bond == 20);
        // 跨日后配额重置
        assert(s2.cand[0].interacts_today == 0);
    }

    // ---- 名单满后替换关系最低者 ----
    {
        pt_social_t s3;
        pt_social_init(&s3, 9, 1);
        pt_events_t q3;
        pt_events_init(&q3);
        pt_state_t a3 = adult_state(DAY1_M, 1);
        for (int d = 0; d < 3; d += 1) {
            a3.minute = DAY(d);
            pt_social_on_minute(&s3, &a3, &q3);
        }
        assert(pt_social_candidate_count(&s3) == 3);
        s3.cand[0].bond = 10;
        s3.cand[1].bond = 20;
        s3.cand[2].bond = 30;
        a3.minute = DAY(3);
        pt_social_on_minute(&s3, &a3, &q3);
        assert(s3.cand[0].bond == 0);    // 最低位被新人替换
        assert(s3.cand[1].bond == 20);
        assert(s3.cand[2].bond == 30);
    }

    // ---- 送礼：80G；相性 +15/+3；余额不足 ----
    {
        pt_social_t s4;
        pt_social_init(&s4, 11, 1);
        pt_events_t q4;
        pt_events_init(&q4);
        pt_state_t a4 = adult_state(DAY1_M, 1);
        pt_social_on_minute(&s4, &a4, &q4);
        uint32_t coins = 200;
        pt_soc_rv_t rv = pt_social_gift(&s4, 0, DAY1_M, &coins,
                                        a4.genome.personality);
        assert(rv == PT_SOC_OK);
        assert(coins == 120);
        assert(s4.cand[0].bond == 15 || s4.cand[0].bond == 3);
        uint32_t poor = 10;
        assert(pt_social_gift(&s4, 0, DAY1_M + 120, &poor, 0)
               == PT_SOC_POOR);
        assert(poor == 10);
    }

    // ---- 求婚：未满热恋拒绝；接受后婚礼 + 同住 24h 育种蛋 ----
    {
        pt_social_t s5;
        pt_social_init(&s5, 99, 1);
        pt_events_t q5;
        pt_events_init(&q5);
        pt_state_t a5 = adult_state(DAY1_M, 1);
        // 连续 6 天、每天 3 次打招呼（间隔 120m），bond 到 85。
        int32_t m = DAY1_M;
        for (int d = 0; d < 6; d += 1) {
            a5.minute = m;
            pt_social_on_minute(&s5, &a5, &q5);
            (void) pop_kind(&q5, PT_EV_SOC_CANDIDATE);
            for (int k = 0; k < 3; k += 1) {
                pt_soc_rv_t rv = pt_social_greet(&s5, 0, m);
                assert(rv == PT_SOC_OK);
                m += 121;
            }
            m = DAY(d + 1) + 8 * 60;
        }
        assert(s5.cand[0].bond >= 80 && s5.cand[0].bond <= 100);

        uint32_t coins = 100000;
        // 求婚裁决有随机接受率；被拒则等过 48h 冷静期再试，终会接受。
        pt_soc_rv_t rv;
        int32_t pm = m;
        int attempts = 0;
        do {
            rv = pt_social_propose(&s5, 0, PT_RING_SIMPLE, pm, &coins, &q5);
            if (rv == PT_SOC_REJECTED) {
                assert(s5.phase == PT_SOC_SINGLE);
                pm += PT_CFG_SOC_PROPOSE_LOCK_MIN + 1;
            }
            attempts += 1;
        } while (rv == PT_SOC_REJECTED && attempts < 20);
        assert(rv == PT_SOC_OK);
        assert(coins == 100000 - PT_CFG_SOC_RING_SIMPLE_COINS);
        assert(s5.phase == PT_SOC_MARRIED);
        assert(pt_social_candidate_count(&s5) == 0);
        assert(pop_kind(&q5, PT_EV_SOC_WEDDING) == 1);

        // 已婚再互动 = BAD_STATE
        assert(pt_social_greet(&s5, 0, pm) == PT_SOC_BAD_STATE);

        // 同住倒计时
        int32_t left = pt_social_egg_minutes_left(&s5, pm);
        assert(left == PT_CFG_SOC_COHABIT_MIN);
        assert(pt_social_egg_minutes_left(&s5, pm + 720) == 720);

        // 满 24h → 育种蛋就绪，事件 + 祖辈环
        a5.minute = pm + PT_CFG_SOC_COHABIT_MIN;
        pt_social_on_minute(&s5, &a5, &q5);
        assert(s5.phase == PT_SOC_EGG_READY);
        assert(pop_kind(&q5, PT_EV_SOC_EGG_READY) == 1);
        assert(s5.child_anc_present & (1u << 0));
        assert(s5.child_anc_present & (1u << 4));
        // 子代基因的某个部件序号在合法范围
        assert(s5.child_genome.body != 0 || s5.child_genome.eyes != 0
               || s5.child_genome.face != 0);
        // 蛋就绪后不再重复推事件
        a5.minute += 1440;
        pt_social_on_minute(&s5, &a5, &q5);
        assert(pop_kind(&q5, PT_EV_SOC_EGG_READY) == 0);

        // 未婚状态下倒计时无效
        pt_social_t s6;
        pt_social_init(&s6, 1, 1);
        assert(pt_social_egg_minutes_left(&s6, 100) == -1);
    }

    // ---- 钻戒接受率高于普通戒（同一状态多注：价格不同）----
    {
        pt_social_t s7;
        pt_social_init(&s7, 5, 1);
        pt_events_t q7;
        pt_events_init(&q7);
        pt_state_t a7 = adult_state(DAY1_M, 1);
        pt_social_on_minute(&s7, &a7, &q7);
        s7.cand[0].bond = 80;
        int acc_s = 0, acc_d = 0;
        for (int i = 0; i < 400; i += 1) {
            pt_social_t a = s7;
            a.rng_state = (uint32_t) (i * 2654435761u + 1);
            pt_events_t qa;
            pt_events_init(&qa);
            uint32_t ca = 100000;
            if (pt_social_propose(&a, 0, PT_RING_SIMPLE, DAY1_M, &ca, &qa)
                == PT_SOC_OK) acc_s += 1;
            pt_social_t b = s7;
            b.rng_state = (uint32_t) (i * 2654435761u + 1);
            pt_events_t qb;
            pt_events_init(&qb);
            uint32_t cb = 100000;
            if (pt_social_propose(&b, 0, PT_RING_DIAMOND, DAY1_M, &cb, &qb)
                == PT_SOC_OK) acc_d += 1;
        }
        // bond=80：普通 40%、钻石 65%；宽松带防 flake。
        assert(acc_s > 100 && acc_s < 250);
        assert(acc_d > acc_s + 50);
    }

    // ---- P2-S3b：世代交替 / 育儿 / 家谱 ----
    {
        pt_social_t s8;
        pt_social_init(&s8, 123, 1);
        assert(pt_social_generation(&s8) == 0);
        assert(!pt_social_care_active(&s8));
        assert(pt_social_hall_count(&s8) == 0);

        // 非 EGG_READY 不能开蛋。
        pt_state_t par = adult_state(DAY1_M, 3);
        assert(!pt_social_begin_generation(&s8, &par));

        // 手工构造 EGG_READY（模拟同住育种结果）。
        s8.phase = PT_SOC_EGG_READY;
        s8.married_at = DAY1_M - PT_CFG_SOC_COHABIT_MIN;
        pt_genome_roll(&s8.spouse, 0xFF, 0, &s8.rng_state);
        s8.child_genome.body = 3;
        s8.child_anc[0] = par.genome;
        s8.child_anc[4] = s8.spouse;
        s8.child_anc_present = (uint8_t) ((1u << 0) | (1u << 4));
        // 一个候选应在世代交替时清空。
        s8.cand[2].valid = true;
        s8.cand[2].bond = 40;

        assert(pt_social_begin_generation(&s8, &par));
        assert(s8.phase == PT_SOC_SINGLE);
        assert(pt_social_generation(&s8) == 1);
        assert(pt_social_care_active(&s8));
        assert(s8.active_anc_present == ((1u << 0) | (1u << 4)));
        assert(s8.active_anc[0].body == par.genome.body);
        // 候选清空且 last_roll_day 复位（子代成年后重新推荐）。
        assert(pt_social_candidate_count(&s8) == 0);
        assert(s8.pending_valid);
        assert(s8.pending.generation == 0);
        assert(s8.pending.pet.body == par.genome.body);

        // 育儿期：未成年阶段不结档。
        pt_events_t q8;
        pt_events_init(&q8);
        pt_state_t kid;
        memset(&kid, 0, sizeof(kid));
        kid.stage = PT_STAGE_BABY;
        kid.minute = DAY1_M + 10;
        pt_social_on_minute(&s8, &kid, &q8);
        assert(pt_social_care_active(&s8));
        assert(pt_social_hall_count(&s8) == 0);

        // 进入少年：父母离队、入堂、事件一次。
        kid.stage = PT_STAGE_TEEN;
        kid.minute = DAY1_M + 20;
        pt_social_on_minute(&s8, &kid, &q8);
        assert(!pt_social_care_active(&s8));
        assert(pt_social_hall_count(&s8) == 1);
        assert(pop_kind(&q8, PT_EV_SOC_PARENTS_LEAVE) == 1);
        const pt_soc_hall_t *h = pt_social_hall_entry(&s8, 0);
        assert(h != NULL && h->generation == 0);
        // 只推一次。
        kid.minute += 10;
        pt_social_on_minute(&s8, &kid, &q8);
        assert(pop_kind(&q8, PT_EV_SOC_PARENTS_LEAVE) == 0);

        // 名人堂 FIFO 淘汰（直接灌 PT_SOC_HALL_MAX+1 条）。
        for (uint8_t i = 0; i < PT_SOC_HALL_MAX; i += 1) {
            memset(&s8.pending, 0, sizeof(s8.pending));
            s8.pending.generation = 100u + i;
            s8.pending_valid = 1;
            s8.care_active = 1;
            int32_t mm = DAY1_M + 30 + i;
            pt_social_end_parenting(&s8, mm, &q8);
        }
        assert(pt_social_hall_count(&s8) == PT_SOC_HALL_MAX);
        // 最旧一条（gen=0）已淘汰，堂内最旧是 gen=100，最新 gen=100+9。
        assert(pt_social_hall_entry(&s8, 0)->generation == 100);
        assert(pt_social_hall_entry(&s8, PT_SOC_HALL_MAX - 1)->generation
               == 100u + PT_SOC_HALL_MAX - 1u);
    }

    // ---- 高世代稀有加权（08 §3.1）：gen=10 时高档稀有占比显著上升 ----
    {
        int hi0 = 0, hi10 = 0;
        uint32_t r0 = 11, r10 = 11;
        for (int i = 0; i < 2000; i += 1) {
            pt_genome_t g;
            pt_genome_roll(&g, 0xFF, 0, &r0);
            if (pt_part_rarity(g.body) >= PT_RAR_R) hi0 += 1;
            pt_genome_roll(&g, 0xFF, 10, &r10);
            if (pt_part_rarity(g.body) >= PT_RAR_R) hi10 += 1;
        }
        // 20% 升档机会主要把 U 推到 R：期望增量约 0.2*P(U)（数百分点），
        // 用宽松下界（+3%）防 flake，上界保证 gen=10 仍非必出高档。
        assert(hi10 > hi0 + 60);
        assert(hi10 < 1200);
    }

    printf("test_pt_social OK\n");
    return 0;
}