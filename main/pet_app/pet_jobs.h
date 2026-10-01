// pet_app/pet_jobs.h —— 成年职业系统纯逻辑（designs/Tomagotchi/07 §5）。
// 不依赖 ESP-IDF/LVGL；职业门槛/工资/班次/月限切换均可主机单测。
// 职业只换游戏的数据与皮：每个职业绑定一个 pt_game_id_t 的缩短版短关。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pt_types.h"

typedef enum {
    PT_JOB_SHOPHAND = 0,   // 杂货帮工（BALANCED 保底，永可做）
    PT_JOB_MUSICIAN,       // 街头乐手
    PT_JOB_BAKER,          // 面包师
    PT_JOB_COURIER,        // 快递员
    PT_JOB_TRAINER,        // 健身教练
    PT_JOB_LIBRARIAN,      // 图书管理员
    PT_JOB_LECTURER,       // 学者/讲师
    PT_JOB_FLORIST,        // 花艺师
    PT_JOB_WEATHER,        // 气象播报
    PT_JOB_COUNT,
} pt_job_id_t;

#define PT_JOB_SHIFTS_PER_DAY   3
#define PT_JOB_MIN_ENERGY       PT_CFG_JOB_MIN_ENERGY
#define PT_JOB_SWITCH_PERIOD_D  30   // 每 30 个游戏日限切换 1 次
#define PT_JOB_STICKER_RUN      3    // 连续 3 个全完美工作日发劳模贴纸

typedef struct {
    const char *name;
    uint8_t game;          // pt_game_id_t（短关玩法）
    uint8_t need_mind;
    uint8_t need_body;
    uint8_t need_art;
    uint16_t wage[3];      // GOOD / GREAT / PERFECT
    bool always;           // 无条件解锁（杂货帮工）
} pt_job_def_t;

typedef struct {
    uint8_t job;              // 当前职业 pt_job_id_t
    uint8_t shifts_today;     // 今日已上班次
    uint8_t perfect_today;    // 今日 PERFECT 班次数
    uint16_t switch_period;   // 上次切换职业所在的 day_id/30 周期；0xFFFF=从未
    uint8_t perfect_run;      // 连续"全班次完美且至少 1 班"的工作日数
    uint8_t worker_sticker;   // 劳模贴纸：0=无 1=有
} pt_jobs_t;

void pt_jobs_init(pt_jobs_t *j);

const pt_job_def_t *pt_job_def(pt_job_id_t job);

// 技能是否达到该职业门槛（杂货帮工永远可做）。
bool pt_job_eligible(pt_job_id_t job, const uint8_t skill[PT_SKILL_COUNT]);

// 列出当前可做职业（含杂货帮工），返回写入 out 的数量（恒 ≥1）。
uint8_t pt_jobs_list(const uint8_t skill[PT_SKILL_COUNT],
                     pt_job_id_t *out, uint8_t cap);

uint16_t pt_job_wage(pt_job_id_t job, uint8_t grade);

// 今天还能不能上班（班次闸；体力/疾病由引擎裁决）。
bool pt_jobs_can_work(const pt_jobs_t *j);

// 记一个班次并返回工资；班次用尽返回 0 且不改状态。
uint16_t pt_jobs_work(pt_jobs_t *j, pt_job_id_t job, uint8_t grade);

// 周期内是否还能切换职业。
bool pt_jobs_can_switch(const pt_jobs_t *j, int32_t day_id);

// 切换职业：门槛/月限不符返回 false。
bool pt_jobs_switch(pt_jobs_t *j, pt_job_id_t job,
                    const uint8_t skill[PT_SKILL_COUNT], int32_t day_id);

// 清醒日界：结算连续完美工作日/贴纸，重置当日班次。
void pt_jobs_on_day(pt_jobs_t *j);
