#include "pet_social_save.h"

#include <assert.h>
#include <string.h>

static void put_u8(uint8_t *p, size_t *o, uint8_t v) { p[(*o)++] = v; }
static void put_u32(uint8_t *p, size_t *o, uint32_t v)
{
    for (int i = 0; i < 4; i += 1) {
        p[*o + i] = (uint8_t) (v >> (8 * i));
    }
    *o += 4;
}
static void put_i32(uint8_t *p, size_t *o, int32_t v) { put_u32(p, o, (uint32_t) v); }

static uint8_t get_u8(const uint8_t *p, size_t *o) { return p[(*o)++]; }
static uint32_t get_u32(const uint8_t *p, size_t *o)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; i += 1) {
        v |= (uint32_t) p[*o + i] << (8 * i);
    }
    *o += 4;
    return v;
}
static int32_t get_i32(const uint8_t *p, size_t *o) { return (int32_t) get_u32(p, o); }

static void put_genome(uint8_t *p, size_t *o, const pt_genome_t *g)
{
    put_u8(p, o, g->body);
    put_u8(p, o, g->eyes);
    put_u8(p, o, g->face);
    put_u8(p, o, g->head);
    put_u8(p, o, g->palette);
    put_u8(p, o, g->back);
    put_u8(p, o, g->personality);
    put_u8(p, o, g->flags);
}

static void get_genome(const uint8_t *p, size_t *o, pt_genome_t *g)
{
    g->body = get_u8(p, o);
    g->eyes = get_u8(p, o);
    g->face = get_u8(p, o);
    g->head = get_u8(p, o);
    g->palette = get_u8(p, o);
    g->back = get_u8(p, o);
    g->personality = get_u8(p, o);
    g->flags = get_u8(p, o);
}

static void put_hall(uint8_t *p, size_t *o, const pt_soc_hall_t *h)
{
    put_genome(p, o, &h->pet);
    put_genome(p, o, &h->mate);
    put_u32(p, o, h->generation);
    put_u8(p, o, h->species);
    put_u8(p, o, h->care);
    put_u8(p, o, h->adult_age_days > 255 ? 255
                                        : (uint8_t) h->adult_age_days);
}

static void get_hall(const uint8_t *p, size_t *o, pt_soc_hall_t *h)
{
    get_genome(p, o, &h->pet);
    get_genome(p, o, &h->mate);
    h->generation = get_u32(p, o);
    h->species = get_u8(p, o);
    h->care = get_u8(p, o);
    // 寿命上限 18 天，成家年龄 u8 足够（饱和存储）。
    h->adult_age_days = get_u8(p, o);
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

// 解析 magic/version 之后的 v1/v2 公共前缀（到 child_anc_present）。
static bool parse_common(const uint8_t *in, pt_social_t *tmp)
{
    size_t o = 0;
    if (get_u32(in, &o) != PET_SOC_SAVE_MAGIC) {
        return false;
    }
    uint8_t ver = get_u8(in, &o);
    if (ver != 1u && ver != PET_SOC_SAVE_VERSION) {
        return false;
    }
    uint8_t phase = get_u8(in, &o);
    if (phase > PT_SOC_EGG_READY) {
        return false;
    }
    tmp->phase = (pt_soc_phase_t) phase;
    tmp->rng_state = get_u32(in, &o);
    tmp->day_id = get_i32(in, &o);
    tmp->last_roll_day = get_i32(in, &o);
    for (uint8_t i = 0; i < PT_SOC_CANDIDATES; i += 1) {
        pt_soc_cand_t *c = &tmp->cand[i];
        c->valid = get_u8(in, &o) != 0;
        get_genome(in, &o, &c->genome);
        c->bond = get_u8(in, &o);
        c->interacts_today = get_u8(in, &o);
        c->last_interact = get_i32(in, &o);
        c->last_day = get_i32(in, &o);
        c->propose_lock = get_i32(in, &o);
    }
    get_genome(in, &o, &tmp->spouse);
    tmp->spouse_personality = get_u8(in, &o);
    tmp->married_at = get_i32(in, &o);
    get_genome(in, &o, &tmp->child_genome);
    for (uint8_t i = 0; i < 8; i += 1) {
        get_genome(in, &o, &tmp->child_anc[i]);
    }
    tmp->child_anc_present = get_u8(in, &o);
    return ver == PET_SOC_SAVE_VERSION;   // true=续读 v2 段；false=v1 后段按零默认
}

size_t pet_soc_save_encode(const pt_social_t *so, uint8_t *out, size_t cap)
{
    if (cap != PET_SOC_SAVE_BYTES) {
        return 0;
    }
    memset(out, 0, cap);
    size_t o = 0;
    put_u32(out, &o, PET_SOC_SAVE_MAGIC);
    put_u8(out, &o, PET_SOC_SAVE_VERSION);
    put_u8(out, &o, (uint8_t) so->phase);
    put_u32(out, &o, so->rng_state);
    put_i32(out, &o, so->day_id);
    put_i32(out, &o, so->last_roll_day);
    for (uint8_t i = 0; i < PT_SOC_CANDIDATES; i += 1) {
        const pt_soc_cand_t *c = &so->cand[i];
        put_u8(out, &o, c->valid ? 1 : 0);
        put_genome(out, &o, &c->genome);
        put_u8(out, &o, c->bond);
        put_u8(out, &o, c->interacts_today);
        put_i32(out, &o, c->last_interact);
        put_i32(out, &o, c->last_day);
        put_i32(out, &o, c->propose_lock);
    }
    put_genome(out, &o, &so->spouse);
    put_u8(out, &o, so->spouse_personality);
    put_i32(out, &o, so->married_at);
    put_genome(out, &o, &so->child_genome);
    for (uint8_t i = 0; i < 8; i += 1) {
        put_genome(out, &o, &so->child_anc[i]);
    }
    put_u8(out, &o, so->child_anc_present);

    // ---- v2 段：代次/祖辈环/育儿/名人堂 ----
    put_u32(out, &o, so->generation);
    for (uint8_t i = 0; i < 8; i += 1) {
        put_genome(out, &o, &so->active_anc[i]);
    }
    put_u8(out, &o, so->active_anc_present);
    put_u8(out, &o, so->care_active);
    put_u8(out, &o, so->pending_valid);
    put_hall(out, &o, &so->pending);
    put_u8(out, &o, so->hall_count);
    for (uint8_t i = 0; i < PT_SOC_HALL_MAX; i += 1) {
        put_hall(out, &o, &so->hall[i]);
    }

    size_t crc_off = PET_SOC_SAVE_BYTES - 4;
    if (o > crc_off) {
        return 0;
    }
    uint32_t crc = crc32_body(out, crc_off);
    out[crc_off] = (uint8_t) (crc & 0xff);
    out[crc_off + 1] = (uint8_t) ((crc >> 8) & 0xff);
    out[crc_off + 2] = (uint8_t) ((crc >> 16) & 0xff);
    out[crc_off + 3] = (uint8_t) ((crc >> 24) & 0xff);
    return PET_SOC_SAVE_BYTES;
}

bool pet_soc_save_decode(pt_social_t *so, const uint8_t *in, size_t len)
{
    if (len != PET_SOC_SAVE_BYTES && len != PET_SOC_SAVE_BYTES_V1) {
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

    pt_social_t tmp;
    memset(&tmp, 0, sizeof(tmp));
    bool is_v2 = parse_common(in, &tmp);
    if (!is_v2 && len != PET_SOC_SAVE_BYTES_V1) {
        return false;
    }
    if (is_v2 && len != PET_SOC_SAVE_BYTES) {
        return false;
    }

    if (is_v2) {
        // 公共段固定 173 字节：18 头 + 3×23 候选 + 8 配偶 + 1 性格 + 4 婚时
        //                  + 8 蛋基因 + 64 蛋祖辈环 + 1 环有效位。
        size_t o = 18u
            + (size_t) PT_SOC_CANDIDATES * (1u + 8u + 1u + 1u + 4u + 4u + 4u)
            + 8u + 1u + 4u + 8u + 8u * 8u + 1u;
        assert(o == 173u);
        tmp.generation = get_u32(in, &o);
        for (uint8_t i = 0; i < 8; i += 1) {
            get_genome(in, &o, &tmp.active_anc[i]);
        }
        tmp.active_anc_present = get_u8(in, &o);
        tmp.care_active = get_u8(in, &o);
        tmp.pending_valid = get_u8(in, &o);
        get_hall(in, &o, &tmp.pending);
        tmp.hall_count = get_u8(in, &o);
        if (tmp.hall_count > PT_SOC_HALL_MAX) {
            return false;
        }
        for (uint8_t i = 0; i < tmp.hall_count; i += 1) {
            get_hall(in, &o, &tmp.hall[i]);
        }
        if (o > crc_off) {
            return false;
        }
    }
    *so = tmp;
    return true;
}
