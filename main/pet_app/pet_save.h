// pet_app/pet_save.h —— 存档序列化（纯逻辑，可主机单测）。
// 只负责 pt_state_t ↔ 字节缓冲的编码/解码与 CRC；NVS 读写由 pet_store.c 负责。
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "pet_decor.h"
#include "pet_econ.h"
#include "pet_jobs.h"
#include "pt_types.h"

#define PET_SAVE_MAGIC   0x50455431u   // "PET1"
#define PET_SAVE_VERSION 6u

// 编码后的固定字节数（所有字段显式小端写，与结构体内存布局解耦）。
size_t pet_save_encode(const pt_state_t *s, const pt_econ_t *e,
                       const pt_jobs_t *j, const pt_decor_t *dcr,
                       uint8_t *out, size_t cap);

// 解码到 s/e/j/dcr；magic/CRC 不符返回 false 且不修改输出。
// v1（仅生命）/v2（+经济）/v3（+技能）/v4（+职业）可读：新增字段按默认值迁移。
bool pet_save_decode(pt_state_t *s, pt_econ_t *e, pt_jobs_t *j,
                     pt_decor_t *dcr, const uint8_t *in, size_t len);

// 供 NVS 层使用：编码长度常量。
#define PET_SAVE_BYTES 256
