// 主机测试：G1 猜大小纯逻辑（固定种子可复现；TIE 不计胜负；退出评级）。
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "pet_game.h"

static void test_fixed_seed_matches(void)
{
    pet_game_t g;
    pet_game_init(&g, 42);
    assert(g.first >= 1 && g.first <= 9);
    uint8_t card0 = g.first;

    // 同种子轨迹必须一致（种子 0 也不能退化成全零状态）。
    pet_game_t h;
    pet_game_init(&h, 42);
    assert(h.first == card0);

    pet_game_result_t rg = pet_game_choose(&g, PET_GAME_HIGH);
    pet_game_result_t rh = pet_game_choose(&h, PET_GAME_HIGH);
    assert(rg == rh);
    assert(g.second == h.second);

    pet_game_t z;
    pet_game_init(&z, 0);
    assert(z.rng != 0);
    assert(z.first >= 1 && z.first <= 9);
}

static void test_tie_automatically_redraws(void)
{
    // 找一个会开出与第一张相等的种子，验证 TIE 重发且不计胜负。
    bool found = false;
    for (uint32_t seed = 1; seed < 200000; seed += 1) {
        pet_game_t g;
        pet_game_init(&g, seed);
        uint8_t first = g.first;
        pet_game_result_t r = pet_game_choose(&g, PET_GAME_HIGH);
        if (r == PET_GAME_TIE) {
            assert(g.second == first);
            assert(g.wins == 0 && g.played == 0);
            assert(g.first >= 1 && g.first <= 9);
            found = true;
            break;
        }
    }
    assert(found);
}

static void test_win_lose_accounting(void)
{
    // 穷举若干种子打完 20 局：wins + loses == played，牌面与判定一致。
    for (uint32_t seed = 1; seed <= 500; seed += 1) {
        pet_game_t g;
        pet_game_init(&g, seed);
        for (int round = 0; round < 20; round += 1) {
            uint8_t first = g.first;
            pet_game_choice_t pick = (round + seed) % 2 ? PET_GAME_HIGH
                                                        : PET_GAME_LOW;
            pet_game_result_t r = pet_game_choose(&g, pick);
            if (r == PET_GAME_TIE) {
                // TIE 不计局数、不增胜场，且第一张牌已重发。
                assert(g.second == first);
                continue;
            }
            bool expect_win = (pick == PET_GAME_HIGH)
                ? (g.second > first)
                : (g.second < first);
            assert((r == PET_GAME_WIN) == expect_win);
            pet_game_next_round(&g);
        }
        assert((uint8_t) (g.played - g.wins) <= g.played);
    }
}

static void test_rating_table(void)
{
    pet_game_t g;
    pet_game_init(&g, 123);
    g.wins = 0;
    assert(pet_game_rating(&g) == 0);
    g.wins = 1;
    assert(pet_game_rating(&g) == 0);
    g.wins = 2;
    assert(pet_game_rating(&g) == 1);
    g.wins = 3;
    assert(pet_game_rating(&g) == 2);
    g.wins = 255;
    assert(pet_game_rating(&g) == 2);
}

int main(void)
{
    test_fixed_seed_matches();
    test_tie_automatically_redraws();
    test_win_lose_accounting();
    test_rating_table();
    printf("test_pet_game: PASS\n");
    return 0;
}
