#include "pt_rng.h"

void pt_rng_seed(uint32_t *state, uint32_t seed)
{
    // xorshift 不允许全零状态；用 FNV 偏移把任意种子映射为非零。
    *state = seed ^ 0x811c9dc5u;
    if (*state == 0) {
        *state = 0x811c9dc5u;
    }
}

uint32_t pt_rng_next(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

uint32_t pt_rng_below(uint32_t *state, uint32_t range)
{
    if (range == 0) {
        return 0;
    }
    // Lemire 无偏取模（本游戏 range 都很小，普通高位截取足够）。
    return (uint64_t) pt_rng_next(state) * range >> 32;
}

uint32_t pt_hash32(const void *data, uint32_t len)
{
    const uint8_t *bytes = data;
    uint32_t h = 0x811c9dc5u;
    for (uint32_t i = 0; i < len; i += 1) {
        h ^= bytes[i];
        h *= 0x01000193u;
    }
    return h;
}

uint32_t pt_salted(uint32_t base, uint32_t day, uint32_t slot)
{
    uint32_t words[3] = { base, day, slot };
    return pt_hash32(words, sizeof(words));
}

bool pt_rng_chance_permille(uint32_t *state, uint32_t p)
{
    if (p >= 1000) {
        return true;
    }
    if (p == 0) {
        return false;
    }
    return pt_rng_below(state, 1000) < p;
}
