// pet_app/pet_games2.h —— G2–G6 纯逻辑内核（designs/Tomagotchi/07 §3）。
// 规则：不依赖 ESP-IDF/LVGL；随机用自带 xorshift（不碰存档 rng_state）；
// 时序游戏由 UI 按固定步长 tick 推进，判定只取决于 tick 数与输入序列，可主机单测。
// 评级口径统一：原始分 0..100；体力 <25 时得分上限 ×80%（07 §4）。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pt_types.h"

#define PET_G2_BEATS        5     // 连续 5 拍
#define PET_G3_TICKS        300   // 30 秒 × UI 10tps
#define PET_G3_LANES        3
#define PET_G5_OBSTACLES    6
#define PET_G6_QUESTIONS    5

#define PET_GRADE_SCORE_GOOD     34
#define PET_GRADE_SCORE_GREAT    60
#define PET_GRADE_SCORE_PERFECT  85

// 原始分（0..100）+ 当前体力 → 评级 0/1/2（GOOD/GREAT/PERFECT）。
uint8_t pet_grade_from_score(uint8_t score, uint8_t energy);

// 打工评级：老年期阈值再放宽 10%（07 §5.2"经验丰富"）。
uint8_t pet_grade_for_job(uint8_t score, uint8_t energy, bool senior);

// ---------------------------------------------------------------- G2 节奏

typedef struct {
    int8_t pos;          // 游标位置 -100..100，在两端往返
    int8_t dir;          // +1 / -1
    uint8_t speed;       // 每 tick 移动量
    uint8_t beat;        // 已判定拍数 0..5
    uint16_t points;     // 累计判定分
    bool done;
} pet_g2_t;

void pet_g2_init(pet_g2_t *g);
void pet_g2_tick(pet_g2_t *g);          // UI 每个节拍 tick（约 50ms）
void pet_g2_press(pet_g2_t *g);         // 本拍在中心区按 OK
void pet_g2_miss(pet_g2_t *g);          // 本拍超时未按
uint8_t pet_g2_score(const pet_g2_t *g);

// ---------------------------------------------------------------- G3 接掉落

typedef struct {
    int8_t lane;        // 0/1/2；-1=空
    bool bad;
    uint8_t row;        // 距底部的步进位置
} pet_g3_drop_t;

typedef struct {
    uint32_t rng;
    int8_t player;
    uint16_t tick;
    uint8_t spawn_in;   // 距下次生成的 tick
    int16_t score;
    pet_g3_drop_t drops[5];
} pet_g3_t;

void pet_g3_init(pet_g3_t *g, uint32_t seed);
void pet_g3_move(pet_g3_t *g, int delta);   // UP=-1 / DOWN=+1，三车道循环
void pet_g3_tick(pet_g3_t *g);
bool pet_g3_done(const pet_g3_t *g);
uint8_t pet_g3_score(const pet_g3_t *g);    // 0..100

// ---------------------------------------------------------------- G4 记忆

typedef enum {
    PET_G4_SHOW = 0,
    PET_G4_INPUT,
    PET_G4_DONE,
} pet_g4_phase_t;

typedef struct {
    uint32_t rng;
    uint8_t seq[7];          // 长度 3..7 全部预生成
    pet_g4_phase_t phase;
    uint8_t level;           // 当前长度 3..7
    uint8_t show_idx;
    uint8_t input_idx;
    uint8_t best;            // 已完整通过的最长序列
} pet_g4_t;

void pet_g4_init(pet_g4_t *g, uint32_t seed);
uint8_t pet_g4_next_pad(pet_g4_t *g);   // 演示阶段取当前灯位并推进；非演示返回 0xFF
void pet_g4_start_input(pet_g4_t *g);
bool pet_g4_input(pet_g4_t *g, uint8_t pad);  // 返回本键是否正确；错误即结束
uint8_t pet_g4_score(const pet_g4_t *g);

// ---------------------------------------------------------------- G5 障碍

typedef enum {
    PET_G5_UP = 0,    // 跳
    PET_G5_DOWN,      // 蹲
} pet_g5_action_t;

typedef struct {
    uint8_t action;    // pet_g5_action_t
    int16_t arrive;    // 到达判定线的 tick
    bool judged;
    bool cleared;
} pet_g5_obs_t;

typedef struct {
    uint32_t rng;
    int16_t tick;
    uint8_t combo;
    uint8_t cleared;
    uint8_t next_obs;
    pet_g5_obs_t obs[PET_G5_OBSTACLES];
} pet_g5_t;

void pet_g5_init(pet_g5_t *g, uint32_t seed);
void pet_g5_tick(pet_g5_t *g);
// 在判定窗内按键：返回 1=通过 / 0=失误（动作错或不在窗口）。
uint8_t pet_g5_act(pet_g5_t *g, pet_g5_action_t act);
bool pet_g5_done(const pet_g5_t *g);
uint8_t pet_g5_score(const pet_g5_t *g);

// ---------------------------------------------------------------- G6 投其所好

typedef enum {
    PET_G6_FOOD = 0,
    PET_G6_TOY,
    PET_G6_HUG,
} pet_g6_choice_t;

// 表情/符号暗示：答案由暗示确定性决定（性格系统 P2 上线后再加喜好修正）。
typedef enum {
    PET_G6_H_HUNGRY = 0,   // 饿 → 食物
    PET_G6_H_PLAYFUL,      // 无聊 → 玩具
    PET_G6_H_CUDDLY,       // 撒娇 → 抱抱
} pet_g6_hint_t;

typedef struct {
    uint32_t rng;
    uint8_t question;      // 0..5
    uint8_t correct_n;     // 答对数
    uint8_t last_tag;      // 最近一次喂食标签（pt_food_tag_t；本期不改变答案）
    pet_g6_hint_t hint;
} pet_g6_t;

void pet_g6_init(pet_g6_t *g, uint32_t seed, uint8_t last_food_tag);
pet_g6_hint_t pet_g6_current_hint(const pet_g6_t *g);
pet_g6_choice_t pet_g6_correct_answer(pet_g6_hint_t hint, uint8_t last_tag);
// 作答：推进到下一题，返回是否答对；5 题后 done。
bool pet_g6_answer(pet_g6_t *g, pet_g6_choice_t choice);
bool pet_g6_done(const pet_g6_t *g);
uint8_t pet_g6_score(const pet_g6_t *g);
