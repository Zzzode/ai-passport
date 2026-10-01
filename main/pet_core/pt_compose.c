// pet_core/pt_compose.c —— 见 pt_compose.h。纯 C，无 LVGL/ESP-IDF 依赖。
#include "pt_compose.h"

#include <stddef.h>
#include <string.h>

#define GRAY_N (PT_COMPOSE_W * PT_COMPOSE_H / 4)   // 64x64 源帧

// 叠放顺序（05 §5.1：背饰在最底，头饰在最上；PALETTE 无图形）。
static const int DRAW_ORDER[5] = {
    PT_GENE_SLOT_BACK,
    PT_GENE_SLOT_BODY,
    PT_GENE_SLOT_FACE,
    PT_GENE_SLOT_EYES,
    PT_GENE_SLOT_HEAD,
};

uint16_t pt_rgb565_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t) (((uint16_t) (r >> 3) << 11)
                       | ((uint16_t) (g >> 2) << 5)
                       | (uint16_t) (b >> 3));
}

uint16_t pt_rgb565_u32(uint32_t rgb)
{
    return pt_rgb565_rgb((uint8_t) (rgb >> 16), (uint8_t) (rgb >> 8),
                         (uint8_t) rgb);
}

static uint8_t mix8(uint8_t a, uint8_t b, uint8_t t8)
{
    // t8: 0..255，无除法（移位近似 1/255）。
    return (uint8_t) ((uint16_t) a + ((uint16_t) (b - a) * t8 >> 8));
}

void pt_compose_lut(const pt_palette_t *pal, uint16_t lut[256])
{
    if (pal == NULL) {
        memset(lut, 0, 256 * sizeof(uint16_t));
        return;
    }
    uint8_t er = (uint8_t) (pal->edge >> 16);
    uint8_t eg = (uint8_t) (pal->edge >> 8);
    uint8_t eb = (uint8_t) pal->edge;
    uint8_t mr = (uint8_t) (pal->main >> 16);
    uint8_t mg = (uint8_t) (pal->main >> 8);
    uint8_t mb = (uint8_t) pal->main;
    uint8_t hr = (uint8_t) (pal->belly >> 16);
    uint8_t hg = (uint8_t) (pal->belly >> 8);
    uint8_t hb = (uint8_t) pal->belly;

    lut[0] = 0;   // 透明哨兵（合成侧不查 LUT[0]）。
    for (uint16_t v = 1; v < 256; v += 1) {
        uint8_t r, g, b;
        if (v <= 48) {
            r = er; g = eg; b = eb;
        } else if (v <= 192) {
            // 49..192 → 0..255 在 edge→main 间插值。
            uint8_t t = (uint8_t) (((uint16_t) (v - 49) * 255u) / 143u);
            r = mix8(er, mr, t);
            g = mix8(eg, mg, t);
            b = mix8(eb, mb, t);
        } else {
            // 193..255 → main→belly 高光。
            uint8_t t = (uint8_t) (((uint16_t) (v - 193) * 255u) / 62u);
            r = mix8(mr, hr, t);
            g = mix8(mg, hg, t);
            b = mix8(mb, hb, t);
        }
        lut[v] = pt_rgb565_rgb(r, g, b);
    }
}

int pt_compose_decode_frame(const uint8_t *rle, uint16_t bytes,
                            uint8_t *out_gray)
{
    if (rle == NULL || out_gray == NULL) {
        return 0;
    }
    uint32_t got = 0;
    for (uint16_t i = 0; i + 1 < bytes; i += 2) {
        uint8_t count = rle[i];
        uint8_t value = rle[i + 1];
        if (count == 0 || got + count > GRAY_N) {
            return 0;
        }
        memset(out_gray + got, value, count);
        got += count;
    }
    return got == GRAY_N;
}

static uint8_t genome_slot_part(const pt_genome_t *g, int slot)
{
    switch (slot) {
    case PT_GENE_SLOT_BODY:    return g->body;
    case PT_GENE_SLOT_EYES:    return g->eyes;
    case PT_GENE_SLOT_FACE:    return g->face;
    case PT_GENE_SLOT_HEAD:    return g->head;
    case PT_GENE_SLOT_BACK:    return g->back;
    default:                   return 0;
    }
}

// 05 §5.2：无 PSRAM，合成严格单实例、缓冲复用。放静态区而非栈——启动期
// pet_ui_init 跑在主任务（栈仅 3.5 KB），4.6 KB 栈帧会溢出复位。仅 LVGL
// 持锁语境调用，无需可重入。
static uint16_t s_lut[256];
static uint8_t s_gray[GRAY_N];

void pt_compose_pet(const pt_genome_t *g, const uint8_t *slot_frame,
                    uint16_t bg565, uint16_t *out,
                    const pet_art_provider_t *art)
{
    if (g == NULL || out == NULL || art == NULL || art->frame_rle == NULL) {
        return;
    }
    for (uint32_t i = 0; i < PT_COMPOSE_PIXELS; i += 1) {
        out[i] = bg565;
    }

    const pt_palette_t *pal = pt_genome_palette(pt_part_index(g->palette));
    pt_compose_lut(pal, s_lut);

    for (uint8_t k = 0; k < 5; k += 1) {
        int slot = DRAW_ORDER[k];
        uint8_t part = genome_slot_part(g, slot);
        uint8_t index = pt_part_index(part);
        uint8_t frame = slot_frame != NULL ? slot_frame[slot] : 0;
        uint16_t bytes = 0;
        const uint8_t *rle = art->frame_rle(slot, index, frame, &bytes);
        if (rle == NULL) {
            // 帧越界退 0 帧；仍无数据则跳过该层（绝不越界写）。
            if (frame != 0) {
                rle = art->frame_rle(slot, index, 0, &bytes);
            }
            if (rle == NULL) {
                continue;
            }
        }
        if (!pt_compose_decode_frame(rle, bytes, s_gray)) {
            continue;
        }
        // 64x64 最近邻 x2 落到 128x128（无逐像素乘除）。
        for (uint8_t sy = 0; sy < 64; sy += 1) {
            uint8_t dy0 = sy * 2;
            for (uint8_t sx = 0; sx < 64; sx += 1) {
                uint8_t v = s_gray[sy * 64 + sx];
                if (v == 0) {
                    continue;
                }
                uint16_t color = s_lut[v];
                uint8_t dx0 = sx * 2;
                out[(dy0) * 128 + dx0] = color;
                out[(dy0) * 128 + dx0 + 1] = color;
                out[(dy0 + 1) * 128 + dx0] = color;
                out[(dy0 + 1) * 128 + dx0 + 1] = color;
            }
        }
    }
}
