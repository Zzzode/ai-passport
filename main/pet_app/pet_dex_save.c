// pet_app/pet_dex_save.c —— PET3 图鉴 blob 编解码（v1=256B）。
#include "pet_dex_save.h"

#include <string.h>

static void put_u8(uint8_t *p, size_t *o, uint8_t v) { p[(*o)++] = v; }
static void put_u16(uint8_t *p, size_t *o, uint16_t v)
{
    p[*o] = (uint8_t) (v & 0xff);
    p[*o + 1] = (uint8_t) (v >> 8);
    *o += 2;
}
static void put_u32(uint8_t *p, size_t *o, uint32_t v)
{
    for (int i = 0; i < 4; i += 1) {
        p[*o + i] = (uint8_t) (v >> (8 * i));
    }
    *o += 4;
}

static uint8_t get_u8(const uint8_t *p, size_t *o) { return p[(*o)++]; }
static uint16_t get_u16(const uint8_t *p, size_t *o)
{
    uint16_t v = (uint16_t) p[*o] | ((uint16_t) p[*o + 1] << 8);
    *o += 2;
    return v;
}
static uint32_t get_u32(const uint8_t *p, size_t *o)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; i += 1) {
        v |= (uint32_t) p[*o + i] << (8 * i);
    }
    *o += 4;
    return v;
}

static uint32_t crc32_body(const uint8_t *p, size_t n)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i += 1) {
        crc ^= p[i];
        for (int b = 0; b < 8; b += 1) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t) (-(int32_t) (crc & 1)));
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

size_t pet_dex_save_encode(const pt_dex_t *d, uint8_t *out, size_t cap)
{
    if (cap != PET_DEX_SAVE_BYTES) {
        return 0;
    }
    memset(out, 0, cap);
    size_t o = 0;
    put_u32(out, &o, PET_DEX_SAVE_MAGIC);
    put_u8(out, &o, PET_DEX_SAVE_VERSION);
    put_u8(out, &o, 0);   // 保留字节
    for (uint8_t i = 0; i < PT_DEX_SPECIES; i += 1) {
        put_u8(out, &o, d->level[i]);
    }
    for (uint8_t i = 0; i < PT_DEX_SPECIES; i += 1) {
        put_u8(out, &o, d->raised[i]);
    }
    for (uint8_t i = 0; i < PT_DEX_SPECIES; i += 1) {
        put_u8(out, &o, d->best_care[i]);
    }
    for (uint8_t i = 0; i < 8; i += 1) {
        put_u8(out, &o, d->parts_seen[i]);
    }
    for (uint8_t i = 0; i < 8; i += 1) {
        put_u8(out, &o, d->parts_owned[i]);
    }
    for (uint8_t i = 0; i < 8; i += 1) {
        put_u8(out, &o, d->parts_bred[i]);
    }
    put_u16(out, &o, d->oldest_days);
    put_u8(out, &o, d->claimed);

    size_t crc_off = PET_DEX_SAVE_BYTES - 4;
    if (o > crc_off) {
        return 0;
    }
    uint32_t crc = crc32_body(out, crc_off);
    out[crc_off] = (uint8_t) (crc & 0xff);
    out[crc_off + 1] = (uint8_t) ((crc >> 8) & 0xff);
    out[crc_off + 2] = (uint8_t) ((crc >> 16) & 0xff);
    out[crc_off + 3] = (uint8_t) ((crc >> 24) & 0xff);
    return PET_DEX_SAVE_BYTES;
}

bool pet_dex_save_decode(pt_dex_t *d, const uint8_t *in, size_t len)
{
    if (len != PET_DEX_SAVE_BYTES) {
        return false;
    }
    size_t crc_off = len - 4;
    uint32_t crc = crc32_body(in, crc_off);
    uint32_t got = (uint32_t) in[crc_off]
                   | ((uint32_t) in[crc_off + 1] << 8)
                   | ((uint32_t) in[crc_off + 2] << 16)
                   | ((uint32_t) in[crc_off + 3] << 24);
    if (crc != got) {
        return false;
    }

    pt_dex_t tmp;
    pt_dex_init(&tmp);
    size_t o = 0;
    if (get_u32(in, &o) != PET_DEX_SAVE_MAGIC) {
        return false;
    }
    if (get_u8(in, &o) != PET_DEX_SAVE_VERSION) {
        return false;
    }
    (void) get_u8(in, &o);   // 保留字节
    for (uint8_t i = 0; i < PT_DEX_SPECIES; i += 1) {
        tmp.level[i] = get_u8(in, &o);
        if (tmp.level[i] > PT_DEX_LV_MASTERED) {
            return false;
        }
    }
    for (uint8_t i = 0; i < PT_DEX_SPECIES; i += 1) {
        tmp.raised[i] = get_u8(in, &o);
    }
    for (uint8_t i = 0; i < PT_DEX_SPECIES; i += 1) {
        uint8_t care = get_u8(in, &o);
        if (care != 0xFFu && care >= PT_DEX_CARE_COUNT) {
            return false;
        }
        tmp.best_care[i] = care;
    }
    for (uint8_t i = 0; i < 8; i += 1) {
        tmp.parts_seen[i] = get_u8(in, &o);
    }
    for (uint8_t i = 0; i < 8; i += 1) {
        tmp.parts_owned[i] = get_u8(in, &o);
    }
    for (uint8_t i = 0; i < 8; i += 1) {
        tmp.parts_bred[i] = get_u8(in, &o);
    }
    tmp.oldest_days = get_u16(in, &o);
    tmp.claimed = get_u8(in, &o);
    // 未知里程碑位（未来版本）不拒绝，但只保留已知位；OWNED/BRED 必为 SEEN 子集。
    tmp.claimed &= 0x7Fu;
    for (uint8_t i = 0; i < 8; i += 1) {
        if ((tmp.parts_owned[i] & ~tmp.parts_seen[i]) != 0
            || (tmp.parts_bred[i] & ~tmp.parts_seen[i]) != 0) {
            return false;
        }
    }
    // 养育过的物种等级不得低于 RAISED。
    for (uint8_t i = 0; i < PT_DEX_SPECIES; i += 1) {
        if (tmp.raised[i] > 0 && tmp.level[i] < PT_DEX_LV_RAISED) {
            return false;
        }
    }
    if (o > crc_off) {
        return false;
    }
    *d = tmp;
    return true;
}
