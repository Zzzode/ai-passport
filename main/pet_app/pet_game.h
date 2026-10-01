// pet_app/pet_game.h —— G1「猜大小」纯逻辑（designs 07 / 13 P0-C）。
// 不依赖 ESP-IDF/LVGL，可在主机单测；RNG 自带 xorshift32，不碰引擎 rng_state。
// 玩法：1..9 两张牌，玩家猜第二张比第一张大（HIGH）或小（LOW）；
// 相等为 TIE，本局不计胜负并重开一张牌；退出时按累计胜场给引擎评级。
#pragma once

#include <stdint.h>

typedef enum {
    PET_GAME_HIGH = 0,
    PET_GAME_LOW,
} pet_game_choice_t;

typedef enum {
    PET_GAME_TIE = 0,
    PET_GAME_WIN,
    PET_GAME_LOSE,
} pet_game_result_t;

typedef struct {
    uint32_t rng;       // 自带 PRNG 状态
    uint8_t first;      // 当前第一张牌 1..9
    uint8_t second;     // 最近一次开出的第二张牌（0=尚未开出）
    uint8_t wins;       // 本次游戏累计胜场
    uint8_t played;     // 已分出胜负的局数
} pet_game_t;

// 用给定种子开新一局，并发第一张牌。
void pet_game_init(pet_game_t *g, uint32_t seed);

// 当前第一张牌（1..9）。
uint8_t pet_game_first_card(const pet_game_t *g);

// 最近一次开出的第二张牌；结果为 TIE 时第一张牌已重发。
uint8_t pet_game_second_card(const pet_game_t *g);

// 玩家做出 HIGH/LOW 选择，开第二张牌并判定。
pet_game_result_t pet_game_choose(pet_game_t *g, pet_game_choice_t choice);

// 结果确认后开下一局的第一张牌。
void pet_game_next_round(pet_game_t *g);

uint8_t pet_game_wins(const pet_game_t *g);

// 退出游戏时 UI 按胜场换算原始评级（pet_ui 再按体力经 pet_grade_from_score 统一结算）：
// 1 胜=GOOD(0)，2 胜=GREAT(1)，3 胜及以上=PERFECT(2)。
uint8_t pet_game_rating(const pet_game_t *g);
