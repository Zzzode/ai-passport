// pet_app/pet_store.h —— 存档的 NVS 持久化（designs 11 §5.2）。
// pet_save 负责字节编解码，本文件负责双 bank 交替写、读回校验、掉电安全：
// 永远先写非当前 bank，读回校验通过后才切换"当前"指针，掉电至少留一份完好。
// 另外保存两个字节级设置：药图标是否点亮、音量档。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pet_decor.h"
#include "pet_econ.h"
#include "pet_jobs.h"
#include "pt_types.h"

// 初始化 NVS（不擦除；失败只记录日志，由调用方决定降级）。
void pet_store_init(void);

// 读取存档：优先当前 bank，损坏自动回退另一份。
// 返回 true 时 *out/*econ/*jobs/*decor 已填充（v1–v4 老存档自动迁移默认值）；
// med/vol 非 NULL 时同时读出（缺省 false / 2）。
bool pet_store_load(pt_state_t *out, pt_econ_t *econ, pt_jobs_t *jobs,
                    pt_decor_t *decor, bool *med_unlocked, uint8_t *volume);

// 双 bank 原子切换保存；写完读回校验失败则保留旧 bank 并返回 false。
bool pet_store_save(const pt_state_t *state, const pt_econ_t *econ,
                    const pt_jobs_t *jobs, const pt_decor_t *decor);

// 罕见设置变更，独立小键即时提交。
void pet_store_set_med(bool unlocked);
void pet_store_set_volume(uint8_t volume);

// 免打扰时段（22:00–08:00）开关；NVS 缺省按 true 处理（designs 03 §4）。
bool pet_store_quiet(void);
void pet_store_set_quiet(bool enabled);

// P2-S3a 社交存档：独立第二组双 bank（键 sa/sb/sc），原始定长 blob，
// 编解码由 pet_social_save 负责。无记录返回 false（调用方按新档初始化）。
bool pet_store_social_load(uint8_t *buf, size_t len);
bool pet_store_social_save(const uint8_t *buf, size_t len);

// P2-S4 图鉴存档：独立第三组双 bank（键 da/db/dc），原始定长 blob，
// 编解码由 pet_dex_save 负责。无记录返回 false（调用方按新档初始化）。
bool pet_store_dex_load(uint8_t *buf, size_t len);
bool pet_store_dex_save(const uint8_t *buf, size_t len);
