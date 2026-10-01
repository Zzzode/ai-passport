#include "pet_game.h"

#include <stdbool.h>

// 与 pet_core/pt_rng 同算法的独立实例：小游戏掷牌不消耗存档 rng_state，
// 游戏结果不参与确定性时间线回放。
static uint32_t next_rand(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static uint8_t draw_card(uint32_t *state)
{
    return (uint8_t) (next_rand(state) % 9u + 1u);
}

void pet_game_init(pet_game_t *g, uint32_t seed)
{
    g->rng = seed ? seed : 0x9e3779b9u;
    g->second = 0;
    g->wins = 0;
    g->played = 0;
    g->first = draw_card(&g->rng);
}

uint8_t pet_game_first_card(const pet_game_t *g)
{
    return g->first;
}

uint8_t pet_game_second_card(const pet_game_t *g)
{
    return g->second;
}

pet_game_result_t pet_game_choose(pet_game_t *g, pet_game_choice_t choice)
{
    g->second = draw_card(&g->rng);
    if (g->second == g->first) {
        // 和局：不计胜负，直接重发第一张牌。
        g->first = draw_card(&g->rng);
        return PET_GAME_TIE;
    }
    bool higher = g->second > g->first;
    bool win = (choice == PET_GAME_HIGH) ? higher : !higher;
    g->played += 1;
    if (win) {
        g->wins += 1;
        return PET_GAME_WIN;
    }
    return PET_GAME_LOSE;
}

void pet_game_next_round(pet_game_t *g)
{
    g->second = 0;
    g->first = draw_card(&g->rng);
}

uint8_t pet_game_wins(const pet_game_t *g)
{
    return g->wins;
}

uint8_t pet_game_rating(const pet_game_t *g)
{
    if (g->wins >= 3) {
        return 2;
    }
    if (g->wins == 2) {
        return 1;
    }
    return 0;
}
