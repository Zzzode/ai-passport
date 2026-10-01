// 主机测试：确定性随机（种子复现、范围、概率边界、盐值区分）。
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "pt_rng.h"

static void test_seed_is_reproducible(void)
{
    uint32_t a, b;
    pt_rng_seed(&a, 12345u);
    pt_rng_seed(&b, 12345u);
    for (int i = 0; i < 100; i += 1) {
        assert(pt_rng_next(&a) == pt_rng_next(&b));
    }

    uint32_t c;
    pt_rng_seed(&c, 12346u);
    pt_rng_seed(&a, 12345u);
    assert(pt_rng_next(&a) != pt_rng_next(&c));
}

static void test_seed_zero_is_not_stuck(void)
{
    uint32_t s;
    pt_rng_seed(&s, 0u);
    uint32_t first = pt_rng_next(&s);
    uint32_t second = pt_rng_next(&s);
    assert(first != 0u);
    assert(first != second);
}

static void test_below_stays_in_range(void)
{
    uint32_t s;
    pt_rng_seed(&s, 777u);
    for (int i = 0; i < 5000; i += 1) {
        uint32_t v = pt_rng_below(&s, 7u);
        assert(v < 7u);
    }
    assert(pt_rng_below(&s, 0u) == 0u);
    assert(pt_rng_below(&s, 1u) == 0u);
}

static void test_chance_boundaries(void)
{
    uint32_t s;
    pt_rng_seed(&s, 99u);
    assert(!pt_rng_chance_permille(&s, 0u));
    assert(pt_rng_chance_permille(&s, 1000u));
    assert(pt_rng_chance_permille(&s, 2000u));   // 溢出上限按 100% 处理

    // 50% 在固定种子下应有稳定的近似频率（容差放宽以免脆测）。
    pt_rng_seed(&s, 4242u);
    int hits = 0;
    for (int i = 0; i < 4000; i += 1) {
        if (pt_rng_chance_permille(&s, 500u)) {
            hits += 1;
        }
    }
    assert(hits > 1700 && hits < 2300);
}

static void test_salted_depends_on_inputs(void)
{
    uint32_t base = pt_salted(1u, 2u, 3u);
    assert(base == pt_salted(1u, 2u, 3u));          // 同输入同结果
    assert(base != pt_salted(1u, 2u, 4u));
    assert(base != pt_salted(1u, 3u, 3u));
    assert(base != pt_salted(2u, 2u, 3u));
}

static void test_hash_matches_known_value(void)
{
    // FNV-1a 偏移基：空输入返回偏移量本身，保证哈希不是常数。
    assert(pt_hash32("", 0u) == 0x811c9dc5u);
    const char *text = "paspet";
    assert(pt_hash32(text, (uint32_t) strlen(text)) != 0x811c9dc5u);
}

int main(void)
{
    test_seed_is_reproducible();
    test_seed_zero_is_not_stuck();
    test_below_stays_in_range();
    test_chance_boundaries();
    test_salted_depends_on_inputs();
    test_hash_matches_known_value();
    return 0;
}
