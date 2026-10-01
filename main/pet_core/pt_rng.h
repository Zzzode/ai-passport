// pet_core/pt_rng.h —— 确定性随机：xorshift32 + 哈希派生。
// 规则：未来事件（排便抖动/例行病/过食掷骰等）一律由确定性哈希派生，
// 保证同一存档无论逐分钟还是离线大跨度回放，结果一致。
#pragma once

#include <stdbool.h>
#include <stdint.h>

void pt_rng_seed(uint32_t *state, uint32_t seed);
uint32_t pt_rng_next(uint32_t *state);

// 返回 [0, range)；range 为 0 时返回 0。
uint32_t pt_rng_below(uint32_t *state, uint32_t range);

// 任意数据的确定性 32 位哈希（FNV-1a 变体）。
uint32_t pt_hash32(const void *data, uint32_t len);

// 便捷派生：把若干盐值与存档代次混合成"当天当槽位"的随机结果。
uint32_t pt_salted(uint32_t base, uint32_t day, uint32_t slot);

// permille 概率判定：返回 true 的概率为 p/1000。
bool pt_rng_chance_permille(uint32_t *state, uint32_t p);
