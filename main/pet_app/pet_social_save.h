// pet_app/pet_social_save.h —— 社交状态独立存档（第二个 NVS blob）。
// 主存档 256B 已用满（v6），社交走自己的 magic/version/CRC 固定缓冲，
// 由 pet_store 的第二组双 bank承载。
// v1=256B（S3a：候选/婚姻/育种蛋）；v2=512B（S3b：+代次/祖辈环/育儿/名人堂）。
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "pt_social.h"

#define PET_SOC_SAVE_MAGIC    0x50455432u   // "PET2"
#define PET_SOC_SAVE_VERSION  2u
#define PET_SOC_SAVE_BYTES    512
#define PET_SOC_SAVE_BYTES_V1 256

size_t pet_soc_save_encode(const pt_social_t *so, uint8_t *out, size_t cap);

// 接受 v1（256B）/v2（512B）；其余长度或校验不符返回 false。
bool pet_soc_save_decode(pt_social_t *so, const uint8_t *in, size_t len);
