// 主机测试：G2–G6 纯逻辑（固定输入 → 固定分数/评级；边界；07 §7）。
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "pet_games2.h"

static void test_grade_thresholds_and_energy(void)
{
    assert(pet_grade_from_score(0, 100) == PT_GAME_GRADE_GOOD);
    assert(pet_grade_from_score(33, 100) == PT_GAME_GRADE_GOOD);
    assert(pet_grade_from_score(34, 100) == PT_GAME_GRADE_GOOD);
    assert(pet_grade_from_score(59, 100) == PT_GAME_GRADE_GOOD);
    assert(pet_grade_from_score(60, 100) == PT_GAME_GRADE_GREAT);
    assert(pet_grade_from_score(84, 100) == PT_GAME_GRADE_GREAT);
    assert(pet_grade_from_score(85, 100) == PT_GAME_GRADE_PERFECT);
    assert(pet_grade_from_score(100, 100) == PT_GAME_GRADE_PERFECT);

    // 体力 <25：得分上限 ×80%。PERFECT 卷面也只能 GREAT；恰好压线的降级。
    assert(pet_grade_from_score(100, 24) == PT_GAME_GRADE_GREAT);  // 80
    assert(pet_grade_from_score(75, 24) == PT_GAME_GRADE_GREAT);   // 60
    assert(pet_grade_from_score(74, 24) == PT_GAME_GRADE_GOOD);    // 59
    assert(pet_grade_from_score(85, 25) == PT_GAME_GRADE_PERFECT); // 临界不罚
    assert(pet_grade_from_score(9, 0) == PT_GAME_GRADE_GOOD);      // 保底 GOOD
}

static void test_g2_rhythm(void)
{
    pet_g2_t g;
    pet_g2_init(&g);
    // 初始 pos=0 即中心，连续 5 拍都在 tick 偶数序列回到近心点不可能；
    // 直接验证：起点按下是 PERFECT，5 次判定后结束，均分=100。
    for (uint8_t i = 0; i < PET_G2_BEATS; i += 1) {
        assert(!g.done);
        pet_g2_press(&g);
    }
    assert(g.done);
    assert(pet_g2_score(&g) == 100);

    // 全部漏拍 0 分；tick 结束后不再计入。
    pet_g2_init(&g);
    for (uint8_t i = 0; i < PET_G2_BEATS; i += 1) {
        pet_g2_miss(&g);
    }
    assert(g.done && pet_g2_score(&g) == 0);
    uint16_t before = g.points;
    pet_g2_press(&g);
    assert(g.points == before);

    // tick 往返不越界 [-100,100]。
    pet_g2_init(&g);
    for (int i = 0; i < 500; i += 1) {
        pet_g2_tick(&g);
        assert(g.pos >= -100 && g.pos <= 100);
    }
}

static void test_g3_catch(void)
{
    // 固定种子下完整跑 300 tick 不崩，结果在 0..100，车道循环合法。
    for (uint32_t seed = 1; seed <= 30; seed += 1) {
        pet_g3_t g;
        pet_g3_init(&g, seed);
        assert(g.player == 1);
        pet_g3_move(&g, -1);
        assert(g.player == 0);
        pet_g3_move(&g, -1);
        assert(g.player == 2);      // 循环
        pet_g3_move(&g, 1);
        assert(g.player == 0);
        while (!pet_g3_done(&g)) {
            pet_g3_tick(&g);
        }
        assert(pet_g3_score(&g) <= 100);
    }

    // 全程守中车道：确定性种子下结果可复现。
    pet_g3_t a, b;
    pet_g3_init(&a, 1234u);
    pet_g3_init(&b, 1234u);
    while (!pet_g3_done(&a)) {
        pet_g3_tick(&a);
        pet_g3_tick(&b);
    }
    assert(pet_g3_score(&a) == pet_g3_score(&b));
}

static void test_g4_memory(void)
{
    // 完美复现 3..7 → 100 分 PERFECT（用内核自己的序列回放）。
    pet_g4_t g;
    pet_g4_init(&g, 777u);
    while (g.phase != PET_G4_DONE) {
        if (g.phase == PET_G4_SHOW) {
            uint8_t shown[7];
            uint8_t n = 0;
            for (;;) {
                uint8_t pad = pet_g4_next_pad(&g);
                if (pad == 0xFFu) {
                    break;
                }
                shown[n++] = pad;
            }
            assert(n == g.level);
            pet_g4_start_input(&g);
            for (uint8_t i = 0; i < n; i += 1) {
                assert(pet_g4_input(&g, shown[i]));
            }
        }
    }
    assert(g.best == 7 && pet_g4_score(&g) == 100);

    // 演示阶段拒绝取灯越界；首轮第一次就错 → best=0，保底 10 分。
    pet_g4_init(&g, 777u);
    uint8_t first = pet_g4_next_pad(&g);
    while (pet_g4_next_pad(&g) != 0xFFu) { }
    pet_g4_start_input(&g);
    assert(!pet_g4_input(&g, (uint8_t) (first ^ 1u)));
    assert(g.phase == PET_G4_DONE && g.best == 0);
    assert(pet_g4_score(&g) == 10);
}

static void test_g5_walk(void)
{
    // 全知玩家：在每个障碍到达 tick 按对应动作 → 全清 100。
    pet_g5_t g;
    pet_g5_init(&g, 42u);
    uint8_t i = 0;
    while (!pet_g5_done(&g)) {
        pet_g5_tick(&g);
        if (i < PET_G5_OBSTACLES && g.tick == g.obs[i].arrive) {
            assert(pet_g5_act(&g, (pet_g5_action_t) g.obs[i].action));
            i += 1;
        }
    }
    assert(i == PET_G5_OBSTACLES);
    assert(g.cleared == PET_G5_OBSTACLES);
    assert(pet_g5_score(&g) == 100);

    // 完全不按：超时全漏，0 分。
    pet_g5_init(&g, 42u);
    while (!pet_g5_done(&g)) {
        pet_g5_tick(&g);
    }
    assert(pet_g5_score(&g) == 0 && g.combo == 0);
}

static void test_g6_preference(void)
{
    // 暗示 → 正确答案矩阵（性格 P2 前 last_tag 不影响）。
    for (uint8_t tag = 0; tag < PT_FOOD_TAG_COUNT; tag += 1) {
        assert(pet_g6_correct_answer(PET_G6_H_HUNGRY, tag) == PET_G6_FOOD);
        assert(pet_g6_correct_answer(PET_G6_H_PLAYFUL, tag) == PET_G6_TOY);
        assert(pet_g6_correct_answer(PET_G6_H_CUDDLY, tag) == PET_G6_HUG);
    }

    // 全对：5 题后 done，100 分（逐题用当前暗示取答案）。
    pet_g6_t g;
    pet_g6_init(&g, 55u, PT_FOOD_TAG_SWEET);
    while (!pet_g6_done(&g)) {
        pet_g6_hint_t h = pet_g6_current_hint(&g);
        assert(pet_g6_answer(&g, pet_g6_correct_answer(h, g.last_tag)));
    }
    assert(g.correct_n == 5 && pet_g6_score(&g) == 100);

    // 全错 0 分；答完 5 题后再答无效。
    pet_g6_init(&g, 55u, PT_FOOD_TAG_MEAL);
    while (!pet_g6_done(&g)) {
        pet_g6_hint_t h = pet_g6_current_hint(&g);
        pet_g6_choice_t wrong = (pet_g6_choice_t)
            ((pet_g6_correct_answer(h, PT_FOOD_TAG_MEAL) + 1) % 3);
        assert(!pet_g6_answer(&g, wrong));
    }
    assert(g.correct_n == 0 && pet_g6_score(&g) == 0);
    assert(!pet_g6_answer(&g, PET_G6_FOOD));
}

// 打工评级：老年阈值放宽 10%（GREAT 54 / PERFECT 76），体力惩罚叠加。
static void test_job_grade_senior_relax(void)
{
    assert(pet_grade_for_job(59, 100, false) == PT_GAME_GRADE_GOOD);
    assert(pet_grade_for_job(54, 100, true) == PT_GAME_GRADE_GREAT);
    assert(pet_grade_for_job(53, 100, true) == PT_GAME_GRADE_GOOD);
    assert(pet_grade_for_job(77, 100, true) == PT_GAME_GRADE_PERFECT);
    assert(pet_grade_for_job(85, 100, false) == PT_GAME_GRADE_PERFECT);
    // 老年 + 低体力：77 → 先打八折 61.6，再比放宽线 54，仍 GREAT。
    assert(pet_grade_for_job(77, 24, true) == PT_GAME_GRADE_GREAT);
    assert(pet_grade_for_job(67, 24, true) == PT_GAME_GRADE_GOOD);  // 48.2
}

int main(void)
{
    test_grade_thresholds_and_energy();
    test_job_grade_senior_relax();
    test_g2_rhythm();
    test_g3_catch();
    test_g4_memory();
    test_g5_walk();
    test_g6_preference();
    printf("test_pet_games2: PASS\n");
    return 0;
}
