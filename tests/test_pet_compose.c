// 主机测试：P2-S2 部件合成（designs 05 §5/§9）。
// 纯逻辑，无 ESP-IDF/LVGL 依赖；RLE 解码/LUT 用桩，合成用管线真实生成的部件表。
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pt_compose.h"
#include "pet_parts_data.h"
#include "pt_genome.h"

#define N (PT_COMPOSE_PIXELS)

static const uint8_t *real_provider(int slot, uint8_t index, uint8_t frame,
                                    uint16_t *bytes)
{
    const pet_part_art_t *a = pet_parts_art(slot, index);
    if (a == NULL || frame >= a->frame_count) {
        return NULL;
    }
    *bytes = a->bytes[frame];
    return a->rle[frame];
}

static const pet_art_provider_t REAL = { real_provider };

// ---- 测试用小工具 ----
static void rle_put(uint8_t **p, uint8_t count, uint8_t value)
{
    *(*p)++ = count;
    *(*p)++ = value;
}

static void test_decode_roundtrip_and_guard(void)
{
    uint8_t gray[PET_PARTS_FRAME_BYTES];
    for (uint32_t i = 0; i < PET_PARTS_FRAME_BYTES; i += 1) {
        gray[i] = (uint8_t) ((i * 7 + (i >> 4)) & 0xFF);
    }
    // 测试内编码器：逐字节对（最坏 2x）。
    uint8_t rle[PET_PARTS_FRAME_BYTES * 2];
    uint8_t *w = rle;
    for (uint32_t i = 0; i < PET_PARTS_FRAME_BYTES; i += 1) {
        rle_put(&w, 1, gray[i]);
    }
    uint8_t out[PET_PARTS_FRAME_BYTES];
    assert(pt_compose_decode_frame(rle, (uint16_t) (w - rle), out));
    assert(memcmp(out, gray, sizeof(gray)) == 0);

    // 超长 run 拒绝。
    uint8_t bad1[] = { 0xFF, 12, 1, 1 };
    assert(!pt_compose_decode_frame(bad1, sizeof(bad1), out));
    // 截断（总长不足 4096）拒绝。
    uint8_t bad2[] = { 10, 5, 10, 6 };
    assert(!pt_compose_decode_frame(bad2, sizeof(bad2), out));
    // count=0 拒绝。
    uint8_t bad3[] = { 0, 5 };
    assert(!pt_compose_decode_frame(bad3, sizeof(bad3), out));
    assert(!pt_compose_decode_frame(NULL, 0, out));
}

static void test_lut_bands(void)
{
    // 用高对比配色，三个波段必须严格落色：main 黑、edge 白、belly 红。
    pt_palette_t pal = { 0x000000, 0xFFFFFF, 0xFF0000 };
    uint16_t lut[256];
    pt_compose_lut(&pal, lut);
    assert(lut[24] == pt_rgb565_u32(0xFFFFFF));       // 轮廓带 = edge
    assert(lut[48] == pt_rgb565_u32(0xFFFFFF));
    // 49→192 白→黑，亮度单调不增（565 量化容差）。
    for (int v = 50; v < 192; v += 1) {
        assert(lut[v + 1] <= lut[v] + 2);
    }
    assert(lut[255] == pt_rgb565_u32(0xFF0000));      // 高光 = belly
    pt_compose_lut(NULL, lut);
    assert(lut[200] == 0);
}

static pt_genome_t genome_with_palette(uint8_t pal_index)
{
    pt_genome_t g;
    memset(&g, 0, sizeof(g));
    g.body = pt_part_make(0, PT_RAR_C);
    g.eyes = pt_part_make(0, PT_RAR_C);
    g.face = pt_part_make(0, PT_RAR_C);
    g.head = pt_part_make(0, PT_RAR_C);
    g.back = pt_part_make(0, PT_RAR_C);
    g.palette = pt_part_make(pal_index, PT_RAR_C);
    g.personality = PT_PERS_TIMID;
    return g;
}

static uint32_t count_color(const uint16_t *img, uint16_t color)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < N; i += 1) {
        if (img[i] == color) {
            n += 1;
        }
    }
    return n;
}

static uint32_t count_nonbg(const uint16_t *img, uint16_t bg)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < N; i += 1) {
        if (img[i] != bg) {
            n += 1;
        }
    }
    return n;
}

static void test_compose_real_art(void)
{
    // 生成表与基因目录件数一致（管线/育种契约）。
    assert(pet_parts_slot_count(PT_GENE_SLOT_BODY)
           == pt_slot_part_count(PT_GENE_SLOT_BODY));
    assert(pet_parts_slot_count(PT_GENE_SLOT_EYES)
           == pt_slot_part_count(PT_GENE_SLOT_EYES));
    assert(pet_parts_slot_count(PT_GENE_SLOT_FACE)
           == pt_slot_part_count(PT_GENE_SLOT_FACE));
    assert(pet_parts_slot_count(PT_GENE_SLOT_HEAD)
           == pt_slot_part_count(PT_GENE_SLOT_HEAD));
    assert(pet_parts_slot_count(PT_GENE_SLOT_BACK)
           == pt_slot_part_count(PT_GENE_SLOT_BACK));
    assert(pet_parts_slot_count(PT_GENE_SLOT_PALETTE) == 0);
    assert(pet_parts_art(PT_GENE_SLOT_BODY, 8) == NULL);

    pt_genome_t g = genome_with_palette(12);   // 炭灰
    const pt_palette_t *pal = pt_genome_palette(12);
    uint16_t bg = pt_rgb565_u32(0xCFEFCB);
    static uint16_t img[N];
    pt_compose_pet(&g, NULL, bg, img, &REAL);

    uint32_t ink = count_nonbg(img, bg);
    assert(ink > 2000);                          // 占位宠不是空图
    assert(count_color(img, pt_rgb565_u32(pal->edge)) > 100);
    assert(count_color(img, pt_rgb565_u32(pal->main)) > 100);
    assert(count_color(img, pt_rgb565_u32(pal->belly)) > 20);
    // 边缘四角必须保持背景（部件内容不铺全幅、锚点统一 0,0 且留边）。
    assert(img[0] == bg && img[127] == bg
           && img[N - 128] == bg && img[N - 1] == bg);

    // 确定性：同基因两次合成逐像素一致。
    static uint16_t img2[N];
    pt_genome_t g2 = g;
    pt_compose_pet(&g2, NULL, bg, img2, &REAL);
    assert(memcmp(img, img2, sizeof(img)) == 0);

    // 换 PALETTE：同形不同色（染色管线真生效）。
    pt_genome_t gw = genome_with_palette(0);   // 雪团白
    static uint16_t imgw[N];
    pt_compose_pet(&gw, NULL, bg, imgw, &REAL);
    assert(memcmp(img, imgw, sizeof(img)) != 0);
    assert(count_color(imgw, pt_rgb565_u32(
               pt_genome_palette(0)->main)) > 100);
}

// 越界部件/损坏 RLE/帧越界都不得崩，其余层照常。
static int g_bad_slot;
static const uint8_t *bad_provider(int slot, uint8_t index, uint8_t frame,
                                   uint16_t *bytes)
{
    (void) index;
    (void) frame;
    if (slot == g_bad_slot) {
        if (g_bad_slot == PT_GENE_SLOT_FACE) {
            static const uint8_t junk[] = { 9, 9 };   // 解码长度不足
            *bytes = sizeof(junk);
            return junk;
        }
        return NULL;
    }
    return real_provider(slot, 0, 0, bytes);
}

static void test_compose_resilience(void)
{
    static uint16_t img[N];
    uint16_t bg = pt_rgb565_u32(0x0102);
    pt_genome_t g = genome_with_palette(3);

    g_bad_slot = PT_GENE_SLOT_BODY;
    const pet_art_provider_t bad = { bad_provider };
    pt_compose_pet(&g, NULL, bg, img, &bad);   // NULL 层跳过
    assert(count_nonbg(img, bg) > 200);     // 头/眼/背饰仍在

    g_bad_slot = PT_GENE_SLOT_FACE;
    pt_compose_pet(&g, NULL, bg, img, &bad);   // 坏 RLE 跳过
    assert(count_nonbg(img, bg) > 200);

    // frame 越界：真实表情帧以外的帧号应自动退回 0 帧而非空白。
    uint8_t wild[PT_GENE_SLOT_COUNT] = { 99, 99, 99, 99, 0, 99 };
    pt_compose_pet(&g, wild, bg, img, &REAL);
    assert(count_nonbg(img, bg) > 2000);

    // 多帧 pose：呼吸/跳跃身体帧与静止帧不同；病眼帧与正常帧不同。
    static uint16_t imgA[N], imgB[N];
    uint8_t breath[PT_GENE_SLOT_COUNT] = {1, 0, 0, 0, 0, 0};
    uint8_t jump[PT_GENE_SLOT_COUNT]   = {2, 0, 0, 0, 0, 0};
    pt_compose_pet(&g, breath, bg, imgA, &REAL);
    pt_compose_pet(&g, jump, bg, imgB, &REAL);
    assert(memcmp(imgA, img, sizeof(img)) != 0);
    assert(memcmp(imgB, img, sizeof(img)) != 0);
    uint8_t sickeye[PT_GENE_SLOT_COUNT] = {0, 4, 0, 0, 0, 0};
    static uint16_t imgS[N];
    pt_compose_pet(&g, sickeye, bg, imgS, &REAL);
    assert(memcmp(imgS, img, sizeof(img)) != 0);

    // 空入参安全。
    pt_compose_pet(NULL, NULL, bg, img, &REAL);
    pt_compose_pet(&g, NULL, bg, NULL, &REAL);
    pet_art_provider_t none = { NULL };
    pt_compose_pet(&g, NULL, bg, img, &none);
}

// 穷举 48 个真实部件：每个都能解码、非全透明、非全幅铺满。
static void test_all_parts_legal(void)
{
    static uint16_t img[N];
    uint16_t bg = pt_rgb565_u32(0x1111);
    static const int slots[5] = {
        PT_GENE_SLOT_BODY, PT_GENE_SLOT_EYES, PT_GENE_SLOT_FACE,
        PT_GENE_SLOT_HEAD, PT_GENE_SLOT_BACK,
    };
    for (uint8_t si = 0; si < 5; si += 1) {
        int slot = slots[si];
        for (uint8_t idx = 0; idx < pet_parts_slot_count(slot); idx += 1) {
            pt_genome_t g = genome_with_palette(0);
            uint8_t *target[] = { &g.body, &g.eyes, &g.face, &g.head,
                                  &g.back };
            // 找到对应槽的字段写入。
            switch (slot) {
            case PT_GENE_SLOT_BODY: g.body = pt_part_make(idx, pt_catalog_rarity(slot, idx)); break;
            case PT_GENE_SLOT_EYES: g.eyes = pt_part_make(idx, pt_catalog_rarity(slot, idx)); break;
            case PT_GENE_SLOT_FACE: g.face = pt_part_make(idx, pt_catalog_rarity(slot, idx)); break;
            case PT_GENE_SLOT_HEAD: g.head = pt_part_make(idx, pt_catalog_rarity(slot, idx)); break;
            default:                g.back = pt_part_make(idx, pt_catalog_rarity(slot, idx)); break;
            }
            (void) target;
            pt_compose_pet(&g, NULL, bg, img, &REAL);
            uint32_t nonbg = count_nonbg(img, bg);
            assert(nonbg > 100);
            assert(nonbg < N);   // 没有部件吃掉四角背景
            assert(img[0] == bg && img[N - 1] == bg);
        }
    }
}

int main(void)
{
    test_decode_roundtrip_and_guard();
    test_lut_bands();
    test_compose_real_art();
    test_compose_resilience();
    test_all_parts_legal();
    printf("test_pet_compose: PASS\n");
    return 0;
}
