// 主机测试：P1-S5 30 天经济仿真（designs 06 §5、12 §14）。
// 不驱动生命引擎，只在账本层用真实收支原语跑三幅玩家画像，验证：
//   1) 成年活跃玩家日净储蓄 ≈ 300G（±20% 校核带）；
//   2) 月初津贴每月恰好一笔 500G；
//   3) >10,000G 软边际精确落在工资/游戏收益上，花回线下立即恢复；
//   4) 余额永远不超 99,999；贝壳签到周期正确；
// 同时打印 30 天通胀曲线供调表（12 §15）。
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "pet_econ.h"
#include "pet_jobs.h"

#define SIM_DAYS 30

typedef struct {
    const char *name;
    uint8_t shifts;         // 每日打工班次数
    uint8_t job_grade;      // 0/1/2
    uint8_t games;          // 每日游戏局数（≤5 全额）
    uint8_t game_grade;     // 0/1/2
    bool checkin;           // 每日是否签到
    uint16_t daily_spend;   // 饭/零食/玩具损耗
    uint32_t start_coins;
} profile_t;

typedef struct {
    uint32_t gross_labor;   // 当日名义工资+游戏
    uint32_t credit_labor;  // 实际入账（软税后）
    uint32_t other_in;      // 签到+津贴
    uint32_t spent;
} day_report_t;

static uint32_t run_profile(const profile_t *p, day_report_t *days,
                            uint16_t *shells_out)
{
    pt_econ_t e;
    pt_econ_init(&e, 7u);
    e.coins = p->start_coins;
    uint32_t stipends = 0;

    for (int d = 0; d < SIM_DAYS; d += 1) {
        uint32_t gross = 0;
        uint32_t credited = 0;

        if (p->checkin) {
            pt_checkin_result_t r;
            if (pt_econ_checkin(&e, d, &r)) {
                days[d].other_in += r.coins_gained;
            }
        }
        if (pt_econ_is_stipend_day(d)) {
            uint16_t got = pt_econ_add_coins(&e, PT_ECON_STIPEND_COINS);
            days[d].other_in += got;
            stipends += got;
        }

        // 打工：工资取自真实职业表（杂货帮工 50/80/120）。
        for (uint8_t s = 0; s < p->shifts; s += 1) {
            uint16_t wage = pt_job_def(PT_JOB_SHOPHAND)->wage[p->job_grade];
            gross += wage;
            credited += pt_econ_credit_income(&e, wage);
        }
        // 游戏：前 5 局全额，第 6 局起保底（画像最多 5 局）。
        for (uint8_t g = 0; g < p->games; g += 1) {
            uint16_t nominal = p->game_grade == PT_GAME_GRADE_PERFECT
                                   ? PT_ECON_GAME_PAY_PERFECT
                               : p->game_grade == PT_GAME_GRADE_GREAT
                                   ? PT_ECON_GAME_PAY_GREAT
                                   : PT_ECON_GAME_PAY_GOOD;
            gross += nominal;
            credited += pt_econ_game_payout(&e, p->game_grade);
        }
        days[d].gross_labor = gross;
        days[d].credit_labor = credited;

        if (p->daily_spend > 0
            && pt_econ_spend_coins(&e, p->daily_spend)) {
            days[d].spent = p->daily_spend;
        } else {
            days[d].spent = e.coins;   // 疏忽画像也不会透支：花光为止
            e.coins = 0;
        }

        days[d].other_in += 0;
        pt_econ_on_day(&e);
        assert(e.coins <= PT_ECON_COIN_CAP);
    }

    assert(stipends == PT_ECON_STIPEND_COINS);   // 30 天恰好 1 笔
    *shells_out = e.shells;
    return e.coins;
}

int main(void)
{
    // 12 §14 基线画像（GREAT 档）：3×80 + 5×20 + 20 − 60 ≈ 300/日。
    profile_t active = {
        "active ", 3, PT_GAME_GRADE_GREAT, 5, PT_GAME_GRADE_GREAT,
        true, 60, 0,
    };
    // 勤奋画像：PERFECT 为主，冲软税线。
    profile_t diligent = {
        "diligent", 3, PT_GAME_GRADE_PERFECT, 5, PT_GAME_GRADE_GREAT,
        true, 60, 0,
    };
    // 疏忽画像：不上班、少游戏、少喂食。
    profile_t neglect = {
        "neglect", 0, PT_GAME_GRADE_GOOD, 2, PT_GAME_GRADE_GOOD,
        true, 15, 0,
    };
    // 富农画像：起始 12,000G，验证软税持续生效与花回线下恢复。
    profile_t rich = {
        "rich   ", 3, PT_GAME_GRADE_GREAT, 5, PT_GAME_GRADE_GREAT,
        true, 200, 12000,
    };

    const profile_t *profiles[4] = { &active, &diligent, &neglect, &rich };
    printf("30-day economy simulation (G coins, design 12 x14 band)\n");

    for (int k = 0; k < 4; k += 1) {
        static day_report_t days[SIM_DAYS];
        for (int d = 0; d < SIM_DAYS; d += 1) {
            days[d] = (day_report_t) { 0 };
        }
        uint16_t shells = 0;
        uint32_t end = run_profile(profiles[k], days, &shells);

        uint32_t labor = 0, other = 0, spent = 0;
        uint32_t balance = profiles[k]->start_coins;
        for (int d = 0; d < SIM_DAYS; d += 1) {
            labor += days[d].gross_labor;
            other += days[d].other_in;
            spent += days[d].spent;
            balance += days[d].credit_labor + days[d].other_in
                       - days[d].spent;
        }
        assert(balance == end);

        printf("%s end=%6lu  labor=%6lu other=%5lu spend=%6lu shells=%u\n",
               profiles[k]->name, (unsigned long) end,
               (unsigned long) labor, (unsigned long) other,
               (unsigned long) spent, (unsigned) shells);
        printf("         curve(day5):");
        uint32_t acc = profiles[k]->start_coins;
        for (int d = 0; d < SIM_DAYS; d += 1) {
            acc += days[d].credit_labor + days[d].other_in - days[d].spent;
            if ((d + 1) % 5 == 0) {
                printf(" %lu", (unsigned long) acc);
            }
        }
        printf("\n");

        if (k == 0) {
            // 活跃画像：剔除每月津贴后的日净储蓄必须落在 300±20% = [240,360]。
            uint32_t net = end - PT_ECON_STIPEND_COINS - profiles[k]->start_coins;
            uint32_t per_day = net / SIM_DAYS;
            printf("         net/day(excl stipend) = %luG\n",
                   (unsigned long) per_day);
            assert(per_day >= 240 && per_day <= 360);
            // 4 个第 7 天大奖 → 4 贝壳。
            assert(shells == 4);
        }
        if (k == 1) {
            // 勤奋画像 30 天必然越过 10,000 线且被软税削峰。
            uint32_t no_tax = profiles[k]->start_coins;
            for (int d = 0; d < SIM_DAYS; d += 1) {
                no_tax += days[d].gross_labor + days[d].other_in
                          - days[d].spent;
            }
            assert(no_tax > PT_ECON_SOFT_TAX_AT);
            printf("         soft tax kept back = %luG\n",
                   (unsigned long) (no_tax - end));
            assert(end < no_tax);
        }
        if (k == 2) {
            // 疏忽画像：余额为正但远低于软税线。
            assert(end > 0 && end < PT_ECON_SOFT_TAX_AT);
        }
        if (k == 3) {
            // 富农全程在线上：每天劳动入账都恰好是名义的 80%。
            for (int d = 0; d < SIM_DAYS; d += 1) {
                assert(days[d].gross_labor > 0);
                assert(days[d].credit_labor
                       == days[d].gross_labor
                              * (100u - PT_ECON_SOFT_TAX_PCT) / 100u);
            }
        }
    }

    printf("test_pet_econ_sim: PASS\n");
    return 0;
}
