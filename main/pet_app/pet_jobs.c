#include "pet_jobs.h"

#include <stddef.h>

// 门槛顺序 MIND/BODY/ART（07 §5.1）；杂货帮工是 BALANCED 保底。
static const pt_job_def_t JOBS[PT_JOB_COUNT] = {
    [PT_JOB_SHOPHAND]  = { "Shop Hand",    PT_GAME_G1_HILO,
                            0,  0,  0, { 50,  80, 120}, true  },
    [PT_JOB_MUSICIAN]  = { "Musician",     PT_GAME_G2_RHYTHM,
                            0,  0, 40, { 50,  80, 120}, false },
    [PT_JOB_BAKER]     = { "Baker",        PT_GAME_G3_CATCH,
                           30,  0, 40, { 60,  90, 130}, false },
    [PT_JOB_COURIER]   = { "Courier",      PT_GAME_G5_WALK,
                            0, 40,  0, { 60,  90, 130}, false },
    [PT_JOB_TRAINER]   = { "Trainer",      PT_GAME_G3_CATCH,
                            0, 60, 30, { 70, 100, 140}, false },
    [PT_JOB_LIBRARIAN] = { "Librarian",    PT_GAME_G4_MEMORY,
                           40,  0,  0, { 50,  80, 120}, false },
    [PT_JOB_LECTURER]  = { "Lecturer",    PT_GAME_G1_HILO,
                           60,  0,  0, { 70, 100, 140}, false },
    [PT_JOB_FLORIST]   = { "Florist",      PT_GAME_G4_MEMORY,
                            0, 30, 30, { 60,  90, 130}, false },
    [PT_JOB_WEATHER]   = { "Weathercast",  PT_GAME_G5_WALK,
                           35, 35, 35, { 80, 110, 150}, false },
};

void pt_jobs_init(pt_jobs_t *j)
{
    j->job = PT_JOB_SHOPHAND;
    j->shifts_today = 0;
    j->perfect_today = 0;
    j->switch_period = 0xFFFFu;
    j->perfect_run = 0;
    j->worker_sticker = 0;
}

const pt_job_def_t *pt_job_def(pt_job_id_t job)
{
    if (job >= PT_JOB_COUNT) {
        return &JOBS[PT_JOB_SHOPHAND];
    }
    return &JOBS[job];
}

bool pt_job_eligible(pt_job_id_t job, const uint8_t skill[PT_SKILL_COUNT])
{
    if (job >= PT_JOB_COUNT) {
        return false;
    }
    const pt_job_def_t *d = &JOBS[job];
    if (d->always) {
        return true;
    }
    return skill[PT_SKILL_MIND] >= d->need_mind
        && skill[PT_SKILL_BODY] >= d->need_body
        && skill[PT_SKILL_ART] >= d->need_art;
}

uint8_t pt_jobs_list(const uint8_t skill[PT_SKILL_COUNT],
                     pt_job_id_t *out, uint8_t cap)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < PT_JOB_COUNT && n < cap; i += 1) {
        if (pt_job_eligible((pt_job_id_t) i, skill)) {
            out[n] = (pt_job_id_t) i;
            n += 1;
        }
    }
    return n;
}

uint16_t pt_job_wage(pt_job_id_t job, uint8_t grade)
{
    const pt_job_def_t *d = pt_job_def(job);
    if (grade > PT_GAME_GRADE_PERFECT) {
        grade = PT_GAME_GRADE_GOOD;
    }
    return d->wage[grade];
}

bool pt_jobs_can_work(const pt_jobs_t *j)
{
    return j->shifts_today < PT_JOB_SHIFTS_PER_DAY;
}

uint16_t pt_jobs_work(pt_jobs_t *j, pt_job_id_t job, uint8_t grade)
{
    if (!pt_jobs_can_work(j) || job >= PT_JOB_COUNT) {
        return 0;
    }
    j->shifts_today = (uint8_t) (j->shifts_today + 1);
    if (grade == PT_GAME_GRADE_PERFECT) {
        j->perfect_today = (uint8_t) (j->perfect_today + 1);
    }
    return pt_job_wage(job, grade);
}

bool pt_jobs_can_switch(const pt_jobs_t *j, int32_t day_id)
{
    if (day_id < 0) {
        return false;
    }
    uint16_t period = (uint16_t) (day_id / PT_JOB_SWITCH_PERIOD_D);
    return j->switch_period != period;
}

bool pt_jobs_switch(pt_jobs_t *j, pt_job_id_t job,
                    const uint8_t skill[PT_SKILL_COUNT], int32_t day_id)
{
    if (job >= PT_JOB_COUNT || job == j->job) {
        return false;
    }
    if (!pt_jobs_can_switch(j, day_id)
        || !pt_job_eligible(job, skill)) {
        return false;
    }
    j->job = (uint8_t) job;
    j->switch_period = (uint16_t) (day_id / PT_JOB_SWITCH_PERIOD_D);
    return true;
}

void pt_jobs_on_day(pt_jobs_t *j)
{
    // 当天至少上了 1 班且全部 PERFECT 才算一个完美工作日（07 §5.2）。
    if (j->shifts_today > 0
        && j->perfect_today == j->shifts_today) {
        if (j->perfect_run < 0xFFu) {
            j->perfect_run = (uint8_t) (j->perfect_run + 1);
        }
        if (j->perfect_run >= PT_JOB_STICKER_RUN) {
            j->worker_sticker = 1;
        }
    } else if (j->shifts_today > 0) {
        j->perfect_run = 0;
    }
    j->shifts_today = 0;
    j->perfect_today = 0;
}
