// pet_app/pet_dex_save.h —— 图鉴独立存档（第三个 NVS blob，PET3）。
// 主存档 256B 已满、社交 blob 512B（PET2）；图鉴走自己的 magic/version/CRC
// 定长缓冲，由 pet_store 的第三组双 bank（da/db/dc）承载。
// v1=256B（S4：物种三级点亮 + 64 部件三标记 + 最长寿 + 里程碑发放位）。
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "pt_dex.h"

#define PET_DEX_SAVE_MAGIC    0x50455433u   // "PET3"
#define PET_DEX_SAVE_VERSION  1u
#define PET_DEX_SAVE_BYTES    256

size_t pet_dex_save_encode(const pt_dex_t *d, uint8_t *out, size_t cap);

// 仅接受 v1=256B 且 CRC/字段合法；否则返回 false（调用方按新档初始化）。
bool pet_dex_save_decode(pt_dex_t *d, const uint8_t *in, size_t len);
