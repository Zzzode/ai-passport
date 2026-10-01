// 主机测试：成年职业系统（designs/Tomagotchi/07 §5）。
// 门槛/工资/班次闸/月限切换/连续完美工作日/劳模贴纸，纯逻辑无 ESP-IDF 依赖。
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "pet_jobs.h"

static const uint8_t SKILL_ZERO[PT_SKILL_COUNT] = { 0, 0, 0 };

static void test_eligibility(void)
{
    // 杂货帮工永远可做；零技能时其他职业全部锁定。
    assert(pt_job_eligible(PT_JOB_SHOPHAND, SKILL_ZERO));
    pt_job_id_t list[PT_JOB_COUNT];
    uint8_t n = pt_jobs_list(SKILL_ZERO, list, PT_JOB_COUNT);
    assert(n == 1 && list[0] == PT_JOB_SHOPHAND);

    // 面包师：ART40 + MIND30，BODY 不要求。
    uint8_t baker[PT_SKILL_COUNT] = { 30, 0, 40 };
    assert(pt_job_eligible(PT_JOB_BAKER, baker));
    assert(!pt_job_eligible(PT_JOB_BAKER,
             (uint8_t[PT_SKILL_COUNT]) { 29, 0, 40 }));
    // 健身教练：BODY60 + ART30（更难）。
    assert(!pt_job_eligible(PT_JOB_TRAINER, baker));
    uint8_t trainer[PT_SKILL_COUNT] = { 0, 60, 30 };
    assert(pt_job_eligible(PT_JOB_TRAINER, trainer));
    // 气象播报三项各 35。
    uint8_t weather[PT_SKILL_COUNT] = { 35, 35, 35 };
    assert(pt_job_eligible(PT_JOB_WEATHER, weather));
    assert(!pt_job_eligible(PT_JOB_WEATHER,
             (uint8_t[PT_SKILL_COUNT]) { 35, 34, 35 }));

    n = pt_jobs_list(trainer, list, PT_JOB_COUNT);
    bool has_courier = false, has_trainer = false, has_hand = false;
    for (uint8_t i = 0; i < n; i += 1) {
        has_courier = has_courier || list[i] == PT_JOB_COURIER;   // BODY40
        has_trainer = has_trainer || list[i] == PT_JOB_TRAINER;
        has_hand = has_hand || list[i] == PT_JOB_SHOPHAND;
    }
    assert(has_courier && has_trainer && has_hand);
}

static void test_wages(void)
{
    assert(pt_job_wage(PT_JOB_SHOPHAND, PT_GAME_GRADE_GOOD) == 50);
    assert(pt_job_wage(PT_JOB_SHOPHAND, PT_GAME_GRADE_GREAT) == 80);
    assert(pt_job_wage(PT_JOB_SHOPHAND, PT_GAME_GRADE_PERFECT) == 120);
    assert(pt_job_wage(PT_JOB_WEATHER, PT_GAME_GRADE_PERFECT) == 150);
    // 脏评级按 GOOD 处理，不越界。
    assert(pt_job_wage(PT_JOB_LECTURER, 9) == 70);
    // 工资远高于小游戏，是主要 G 币来源。
    assert(pt_job_wage(PT_JOB_BAKER, PT_GAME_GRADE_GOOD) == 60);
}

static void test_shifts_cap(void)
{
    pt_jobs_t j;
    pt_jobs_init(&j);
    assert(pt_jobs_can_work(&j));
    assert(pt_jobs_work(&j, PT_JOB_SHOPHAND, PT_GAME_GRADE_GOOD) == 50);
    assert(pt_jobs_work(&j, PT_JOB_SHOPHAND, PT_GAME_GRADE_GREAT) == 80);
    assert(pt_jobs_work(&j, PT_JOB_SHOPHAND, PT_GAME_GRADE_PERFECT) == 120);
    assert(!pt_jobs_can_work(&j));
    // 第 4 班：不记班、不发钱。
    assert(pt_jobs_work(&j, PT_JOB_SHOPHAND, PT_GAME_GRADE_PERFECT) == 0);
    assert(j.shifts_today == 3 && j.perfect_today == 1);
}

static void test_switch_rules(void)
{
    pt_jobs_t j;
    pt_jobs_init(&j);
    uint8_t librarian[PT_SKILL_COUNT] = { 40, 0, 0 };
    // 门槛不足不能切。
    assert(!pt_jobs_switch(&j, PT_JOB_LIBRARIAN, SKILL_ZERO, 5));
    // 首次切换成功。
    assert(pt_jobs_switch(&j, PT_JOB_LIBRARIAN, librarian, 5));
    assert(j.job == PT_JOB_LIBRARIAN);
    // 同一 30 日周期内不能再切（day 5/30 == 0；day 29 同周期）。
    assert(!pt_jobs_switch(&j, PT_JOB_SHOPHAND, SKILL_ZERO, 29));
    // 下一周期可切回；切成自己永远失败。
    assert(!pt_jobs_switch(&j, PT_JOB_LIBRARIAN, librarian, 30));
    assert(pt_jobs_switch(&j, PT_JOB_SHOPHAND, SKILL_ZERO, 30));
    assert(j.job == PT_JOB_SHOPHAND);
}

static void test_perfect_run_and_sticker(void)
{
    pt_jobs_t j;
    pt_jobs_init(&j);

    // 第 1 天：全班 PERFECT → run=1。
    (void) pt_jobs_work(&j, PT_JOB_SHOPHAND, PT_GAME_GRADE_PERFECT);
    (void) pt_jobs_work(&j, PT_JOB_SHOPHAND, PT_GAME_GRADE_PERFECT);
    pt_jobs_on_day(&j);
    assert(j.perfect_run == 1 && j.shifts_today == 0);

    // 第 2 天：只上 1 班但 PERFECT → run=2。
    (void) pt_jobs_work(&j, PT_JOB_SHOPHAND, PT_GAME_GRADE_PERFECT);
    pt_jobs_on_day(&j);
    assert(j.perfect_run == 2);

    // 第 3 天：3 班全 PERFECT → run=3，发劳模贴纸。
    for (int i = 0; i < 3; i += 1) {
        (void) pt_jobs_work(&j, PT_JOB_SHOPHAND, PT_GAME_GRADE_PERFECT);
    }
    pt_jobs_on_day(&j);
    assert(j.perfect_run == 3 && j.worker_sticker == 1);

    // 贴纸只发一次但保留；run 继续累加。
    pt_jobs_on_day(&j);   // 没上班的日子不清零连续记录。
    assert(j.perfect_run == 3);

    // 一个有瑕疵的工作日（有 GOOD 班）立即清零连续记录，贴纸不收回。
    (void) pt_jobs_work(&j, PT_JOB_SHOPHAND, PT_GAME_GRADE_PERFECT);
    (void) pt_jobs_work(&j, PT_JOB_SHOPHAND, PT_GAME_GRADE_GOOD);
    pt_jobs_on_day(&j);
    assert(j.perfect_run == 0 && j.worker_sticker == 1);
}

int main(void)
{
    test_eligibility();
    test_wages();
    test_shifts_cap();
    test_switch_rules();
    test_perfect_run_and_sticker();
    printf("test_pet_jobs: PASS\n");
    return 0;
}
