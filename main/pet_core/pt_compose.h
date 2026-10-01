// pet_core/pt_compose.h —— L2 外观运行时合成（designs 05 §5）。
// 纯 C、无 LVGL 依赖：灰度部件 + 256 项调色板 LUT → 128x128 RGB565，
// 单帧耗时预算在真机以 30 fps 50% 余量校核（模拟器仅功能参考）。
#pragma once

#include <stdint.h>

#include "pt_genome.h"

#define PT_COMPOSE_W 128
#define PT_COMPOSE_H 128
#define PT_COMPOSE_PIXELS (PT_COMPOSE_W * PT_COMPOSE_H)

// 部件帧数据提供者（固件侧接生成表 pet_parts_data，测试侧可注入桩）。
typedef struct {
    // 返回某槽某部件某帧的 RLE 流与字节数；不存在返回 NULL。
    const uint8_t *(*frame_rle)(int slot, uint8_t index, uint8_t frame,
                                uint16_t *bytes);
} pet_art_provider_t;

uint16_t pt_rgb565_u32(uint32_t rgb);
uint16_t pt_rgb565_rgb(uint8_t r, uint8_t g, uint8_t b);

// 由调色板方案预计算 256 项 LUT：0 保留透明；1..48 轮廓色；49..192 轮廓→主色；
// 193..255 主色→腹白。调用方保证每只宠物一块可复用 LUT（512 B）。
void pt_compose_lut(const pt_palette_t *pal, uint16_t lut[256]);

// RLE 解码一帧（count,value 对，输出固定 PET_PARTS_FRAME_BYTES=4096 灰度字节）。
// 成功返回 true；流损坏/长度不符返回 false。
int pt_compose_decode_frame(const uint8_t *rle, uint16_t bytes,
                            uint8_t *out_gray);

// 把基因型对应的 5 个有形槽（PALETTE 只染色）按 BACK/BODY/FACE/EYES/HEAD
// 顺序叠加到 RGB565 缓冲；透明像素不写，非透明区域外填 bg565（RGB565 无
// alpha 通道，调用方传当前房间墙色实现视觉融合）。
// slot_frame 以 PT_GENE_SLOT_* 为下标给出每槽动画帧（05 §5.3），可为 NULL
// （全 0 帧）；帧号 >= 该部件帧数时自动退回 0 帧。
void pt_compose_pet(const pt_genome_t *g, const uint8_t *slot_frame,
                    uint16_t bg565, uint16_t *out,
                    const pet_art_provider_t *art);
