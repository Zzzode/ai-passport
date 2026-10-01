#include "pet_save.h"

#include <string.h>

#include "pet_decor.h"
#include "pet_econ.h"
#include "pet_jobs.h"
#include "pt_config.h"
#include "pt_rng.h"

// 显式小端读写：不依赖结构体布局，跨版本/跨平台稳定。
static void put_u8(uint8_t *p, size_t *off, uint8_t v)
{
    p[*off] = v;
    *off += 1;
}

static void put_u16(uint8_t *p, size_t *off, uint16_t v)
{
    p[*off] = (uint8_t) (v & 0xff);
    p[*off + 1] = (uint8_t) (v >> 8);
    *off += 2;
}

static void put_u32(uint8_t *p, size_t *off, uint32_t v)
{
    for (int i = 0; i < 4; i += 1) {
        p[*off + i] = (uint8_t) ((v >> (8 * i)) & 0xff);
    }
    *off += 4;
}

static void put_i32(uint8_t *p, size_t *off, int32_t v)
{
    put_u32(p, off, (uint32_t) v);
}

static uint8_t get_u8(const uint8_t *p, size_t *off)
{
    uint8_t v = p[*off];
    *off += 1;
    return v;
}

static uint16_t get_u16(const uint8_t *p, size_t *off)
{
    uint16_t v = (uint16_t) (p[*off] | ((uint16_t) p[*off + 1] << 8));
    *off += 2;
    return v;
}

static uint32_t get_u32(const uint8_t *p, size_t *off)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; i += 1) {
        v |= (uint32_t) p[*off + i] << (8 * i);
    }
    *off += 4;
    return v;
}

static int32_t get_i32(const uint8_t *p, size_t *off)
{
    return (int32_t) get_u32(p, off);
}

static void put_ledger(uint8_t *p, size_t *off, const pt_ledger_t *l)
{
    put_u16(p, off, (uint16_t) l->small);
    put_u16(p, off, (uint16_t) l->big);
    put_u8(p, off, l->full_topups);
    put_u8(p, off, l->happy_topups);
    put_u8(p, off, l->perfunctory);
    for (uint8_t i = 0; i < PT_FOOD_TAG_COUNT; i += 1) {
        put_u16(p, off, l->diet[i]);
    }
}

static void get_ledger(const uint8_t *p, size_t *off, pt_ledger_t *l)
{
    l->small = (int16_t) get_u16(p, off);
    l->big = (int16_t) get_u16(p, off);
    l->full_topups = get_u8(p, off);
    l->happy_topups = get_u8(p, off);
    l->perfunctory = get_u8(p, off);
    for (uint8_t i = 0; i < PT_FOOD_TAG_COUNT; i += 1) {
        l->diet[i] = get_u16(p, off);
    }
}

// v1/v2 老账本：无维度③饮食字段，读旧 7 字节，diet 保持零（tmp 已清零）。
static void get_ledger_v2(const uint8_t *p, size_t *off, pt_ledger_t *l)
{
    l->small = (int16_t) get_u16(p, off);
    l->big = (int16_t) get_u16(p, off);
    l->full_topups = get_u8(p, off);
    l->happy_topups = get_u8(p, off);
    l->perfunctory = get_u8(p, off);
}

size_t pet_save_encode(const pt_state_t *s, const pt_econ_t *e,
                       const pt_jobs_t *j, const pt_decor_t *dcr,
                       uint8_t *out, size_t cap)
{
    if (cap < PET_SAVE_BYTES) {
        return 0;
    }
    memset(out, 0, PET_SAVE_BYTES);
    size_t off = 0;

    put_u32(out, &off, PET_SAVE_MAGIC);
    put_u32(out, &off, PET_SAVE_VERSION);
    put_u32(out, &off, s->boot_count);
    put_u32(out, &off, s->rng_state);

    put_i32(out, &off, s->minute);
    put_i32(out, &off, s->stage_started);
    put_i32(out, &off, s->day_id);
    put_i32(out, &off, s->death_minute);
    put_i32(out, &off, s->call_started);
    put_i32(out, &off, s->next_poop);
    put_i32(out, &off, s->sick_scheduled);
    put_i32(out, &off, s->poop_health_at);
    put_i32(out, &off, s->poop_sick_at);
    put_i32(out, &off, s->pat_window_start);
    put_i32(out, &off, s->zero_since[PT_NEED_FULL]);
    put_i32(out, &off, s->zero_since[PT_NEED_HAPPY]);

    put_u16(out, &off, s->age_days);
    put_u8(out, &off, (uint8_t) s->stage);
    put_u8(out, &off, (uint8_t) s->species);
    put_u8(out, &off, (uint8_t) s->pending_species);
    put_u8(out, &off, s->waiting_evolve ? 1 : 0);
    put_u8(out, &off, s->ledger_frozen ? 1 : 0);

    put_u8(out, &off, s->fullness);
    put_u8(out, &off, s->happiness);
    put_u8(out, &off, s->health);
    put_u8(out, &off, s->energy);
    put_u8(out, &off, s->bond);
    put_u8(out, &off, s->weight);
    put_u8(out, &off, s->poops);

    put_u8(out, &off, s->sleeping ? 1 : 0);
    put_u8(out, &off, s->lights_off ? 1 : 0);
    put_u8(out, &off, s->poor_sleep ? 1 : 0);
    put_u8(out, &off, s->sick ? 1 : 0);
    put_u8(out, &off, (uint8_t) s->active_call);
    put_u8(out, &off, (uint8_t) s->need_state[PT_NEED_FULL]);
    put_u8(out, &off, (uint8_t) s->need_state[PT_NEED_HAPPY]);

    put_u16(out, &off, (uint16_t) s->acc_full);
    put_u16(out, &off, (uint16_t) s->acc_happy);
    put_u16(out, &off, (uint16_t) s->acc_energy);
    put_u16(out, &off, (uint16_t) s->acc_health);

    put_ledger(out, &off, &s->ledger);
    put_ledger(out, &off, &s->prev_ledger);

    put_u8(out, &off, s->snacks_today);
    put_u8(out, &off, s->cleans_bond_today);
    put_u8(out, &off, s->game_weight_today);
    put_u8(out, &off, s->pats_in_window);

    // ---- v3：维度②技能与每日游戏计数
    for (uint8_t i = 0; i < PT_SKILL_COUNT; i += 1) {
        put_u8(out, &off, s->skill[i]);
    }
    for (uint8_t i = 0; i < PT_SKILL_COUNT; i += 1) {
        put_u8(out, &off, s->skill_today[i]);
    }
    put_u8(out, &off, s->games_today);

    // ---- v2：经济状态（L3）；经济模块与生命内核解耦，但共用双 bank blob ----
    put_u32(out, &off, e->coins);
    put_u16(out, &off, e->shells);
    put_u16(out, &off, e->streak);
    put_i32(out, &off, e->checkin_day);
    put_u32(out, &off, e->shelf_seed);
    for (uint8_t i = 0; i < PT_ECON_INV_SLOTS; i += 1) {
        put_u16(out, &off, e->inv[i].item);
        put_u8(out, &off, e->inv[i].qty);
    }
    for (uint8_t i = 0; i < PT_ITEM_COUNT; i += 1) {
        put_u8(out, &off, e->toy_uses[i]);
    }
    put_u8(out, &off, e->game_pays_today);

    // ---- v4：职业（L3）
    put_u8(out, &off, j->job);
    put_u8(out, &off, j->shifts_today);
    put_u8(out, &off, j->perfect_today);
    put_u16(out, &off, j->switch_period);
    put_u8(out, &off, j->perfect_run);
    put_u8(out, &off, j->worker_sticker);

    // ---- v5：换装 / 家具 / 房间主题（L3，跨世代保留的收藏财产）
    for (uint8_t i = 0; i < PT_SLOT_COUNT; i += 1) {
        put_u8(out, &off, dcr->worn[i]);
    }
    put_u8(out, &off, dcr->outfits);
    put_u8(out, &off, dcr->furniture);
    put_u8(out, &off, dcr->placed);
    put_u8(out, &off, dcr->themes);
    put_u8(out, &off, dcr->theme);

    // ---- v6：L2 基因型（8B，偏移 242）
    put_u8(out, &off, s->genome.body);
    put_u8(out, &off, s->genome.eyes);
    put_u8(out, &off, s->genome.face);
    put_u8(out, &off, s->genome.head);
    put_u8(out, &off, s->genome.palette);
    put_u8(out, &off, s->genome.back);
    put_u8(out, &off, s->genome.personality);
    put_u8(out, &off, s->genome.flags);

    // CRC 覆盖整个前 PET_SAVE_BYTES-4 字节（未写到的尾部区为零填充，
    // 编解码两侧口径一致），固定写在缓冲区末尾，解码方无需知道有效长度。
    size_t crc_off = PET_SAVE_BYTES - 4;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < crc_off; i += 1) {
        crc ^= out[i];
        for (int b = 0; b < 8; b += 1) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t) (-(int32_t)(crc & 1)));
        }
    }
    crc ^= 0xFFFFFFFFu;
    out[crc_off] = (uint8_t) (crc & 0xff);
    out[crc_off + 1] = (uint8_t) ((crc >> 8) & 0xff);
    out[crc_off + 2] = (uint8_t) ((crc >> 16) & 0xff);
    out[crc_off + 3] = (uint8_t) ((crc >> 24) & 0xff);

    return PET_SAVE_BYTES;
}

bool pet_save_decode(pt_state_t *s, pt_econ_t *e, pt_jobs_t *j,
                     pt_decor_t *dcr, const uint8_t *in, size_t len)
{
    if (len < PET_SAVE_BYTES) {
        return false;
    }
    size_t magic_off = 0;
    if (get_u32(in, &magic_off) != PET_SAVE_MAGIC) {
        return false;
    }
    size_t off = magic_off;
    uint32_t version = get_u32(in, &off);
    if (version != 1u && version != 2u && version != 3u && version != 4u
        && version != 5u && version != PET_SAVE_VERSION) {
        return false;
    }

    // 先校验 CRC（对除尾部 CRC 外的全部字节）。
    uint32_t stored_crc = 0;
    {
        size_t crc_off = PET_SAVE_BYTES - 4;
        stored_crc = (uint32_t) in[crc_off] | ((uint32_t) in[crc_off + 1] << 8)
            | ((uint32_t) in[crc_off + 2] << 16)
            | ((uint32_t) in[crc_off + 3] << 24);
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < crc_off; i += 1) {
            crc ^= in[i];
            for (int b = 0; b < 8; b += 1) {
                crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t) (-(int32_t)(crc & 1)));
            }
        }
        if ((crc ^ 0xFFFFFFFFu) != stored_crc) {
            return false;
        }
    }

    pt_state_t tmp;
    memset(&tmp, 0, sizeof(tmp));
    pt_econ_t etmp;
    memset(&etmp, 0, sizeof(etmp));
    // 其余字段按编码顺序读取。
    tmp.boot_count = get_u32(in, &off);
    tmp.rng_state = get_u32(in, &off);
    tmp.minute = get_i32(in, &off);
    tmp.stage_started = get_i32(in, &off);
    tmp.day_id = get_i32(in, &off);
    tmp.death_minute = get_i32(in, &off);
    tmp.call_started = get_i32(in, &off);
    tmp.next_poop = get_i32(in, &off);
    tmp.sick_scheduled = get_i32(in, &off);
    tmp.poop_health_at = get_i32(in, &off);
    tmp.poop_sick_at = get_i32(in, &off);
    tmp.pat_window_start = get_i32(in, &off);
    tmp.zero_since[PT_NEED_FULL] = get_i32(in, &off);
    tmp.zero_since[PT_NEED_HAPPY] = get_i32(in, &off);
    tmp.age_days = get_u16(in, &off);
    tmp.stage = (pt_stage_t) get_u8(in, &off);
    tmp.species = (pt_species_t) get_u8(in, &off);
    tmp.pending_species = (pt_species_t) get_u8(in, &off);
    tmp.waiting_evolve = get_u8(in, &off) != 0;
    tmp.ledger_frozen = get_u8(in, &off) != 0;
    tmp.fullness = get_u8(in, &off);
    tmp.happiness = get_u8(in, &off);
    tmp.health = get_u8(in, &off);
    tmp.energy = get_u8(in, &off);
    tmp.bond = get_u8(in, &off);
    tmp.weight = get_u8(in, &off);
    tmp.poops = get_u8(in, &off);
    tmp.sleeping = get_u8(in, &off) != 0;
    tmp.lights_off = get_u8(in, &off) != 0;
    tmp.poor_sleep = get_u8(in, &off) != 0;
    tmp.sick = get_u8(in, &off) != 0;
    tmp.active_call = (pt_call_kind_t) get_u8(in, &off);
    tmp.need_state[PT_NEED_FULL] = (pt_need_state_t) get_u8(in, &off);
    tmp.need_state[PT_NEED_HAPPY] = (pt_need_state_t) get_u8(in, &off);
    tmp.acc_full = (int16_t) get_u16(in, &off);
    tmp.acc_happy = (int16_t) get_u16(in, &off);
    tmp.acc_energy = (int16_t) get_u16(in, &off);
    tmp.acc_health = (int16_t) get_u16(in, &off);
    if (version >= 3u) {
        get_ledger(in, &off, &tmp.ledger);
        get_ledger(in, &off, &tmp.prev_ledger);
    } else {
        get_ledger_v2(in, &off, &tmp.ledger);
        get_ledger_v2(in, &off, &tmp.prev_ledger);
    }
    tmp.snacks_today = get_u8(in, &off);
    tmp.cleans_bond_today = get_u8(in, &off);
    tmp.game_weight_today = get_u8(in, &off);
    tmp.pats_in_window = get_u8(in, &off);

    if (version >= 3u) {
        for (uint8_t i = 0; i < PT_SKILL_COUNT; i += 1) {
            tmp.skill[i] = get_u8(in, &off);
            if (tmp.skill[i] > PT_CFG_SKILL_CAP) {
                return false;
            }
        }
        for (uint8_t i = 0; i < PT_SKILL_COUNT; i += 1) {
            tmp.skill_today[i] = get_u8(in, &off);
            if (tmp.skill_today[i] > PT_CFG_SKILL_DAILY_CAP) {
                return false;
            }
        }
        tmp.games_today = get_u8(in, &off);
    }

    if (version == 1u) {
        // v1 存档：P1 经济上线前的存档，钱包/库存按新存档默认值迁移；
        // 货架种子从生命 RNG 派生，同一存档迁移后货架仍逐日确定。
        uint32_t seed = pt_hash32(&tmp.rng_state, sizeof(tmp.rng_state));
        seed ^= (uint32_t) tmp.boot_count * 2654435761u;
        seed ^= 0x5A17E001u;
        pt_econ_init(&etmp, seed);
    } else {
        etmp.coins = get_u32(in, &off);
        etmp.shells = get_u16(in, &off);
        etmp.streak = get_u16(in, &off);
        etmp.checkin_day = (int32_t) get_u32(in, &off);
        etmp.shelf_seed = get_u32(in, &off);
        for (uint8_t i = 0; i < PT_ECON_INV_SLOTS; i += 1) {
            etmp.inv[i].item = get_u16(in, &off);
            etmp.inv[i].qty = get_u8(in, &off);
        }
        for (uint8_t i = 0; i < PT_ITEM_COUNT; i += 1) {
            etmp.toy_uses[i] = get_u8(in, &off);
        }
        if (version >= 3u) {
            etmp.game_pays_today = get_u8(in, &off);
        }
        // 模式约束：超范围 id/越界余额视为损坏（CRC 已挡随机损坏，这里挡脏写）。
        if (etmp.coins > PT_ECON_COIN_CAP || etmp.shells > PT_ECON_SHELL_CAP
            || etmp.streak > 7) {
            return false;
        }
        for (uint8_t i = 0; i < PT_ECON_INV_SLOTS; i += 1) {
            uint16_t id = etmp.inv[i].item;
            if (id >= PT_ITEM_COUNT
                || (id == PT_ITEM_NONE && etmp.inv[i].qty != 0)) {
                return false;
            }
        }
        for (uint8_t i = 0; i < PT_ITEM_COUNT; i += 1) {
            if (etmp.toy_uses[i] > PT_ECON_TOY_USES_DAY) {
                return false;
            }
        }
    }

    pt_jobs_t jtmp;
    pt_jobs_init(&jtmp);
    if (version >= 4u) {
        jtmp.job = get_u8(in, &off);
        jtmp.shifts_today = get_u8(in, &off);
        jtmp.perfect_today = get_u8(in, &off);
        jtmp.switch_period = get_u16(in, &off);
        jtmp.perfect_run = get_u8(in, &off);
        jtmp.worker_sticker = get_u8(in, &off);
        if (jtmp.job >= PT_JOB_COUNT
            || jtmp.shifts_today > PT_JOB_SHIFTS_PER_DAY
            || jtmp.perfect_today > jtmp.shifts_today
            || jtmp.worker_sticker > 1u) {
            return false;
        }
    }

    // v5：装饰收藏。v1–v4 存档迁移为默认值（只有默认主题）。
    pt_decor_t dtmp;
    pt_decor_init(&dtmp);
    if (version >= 5u) {
        for (uint8_t i = 0; i < PT_SLOT_COUNT; i += 1) {
            dtmp.worn[i] = get_u8(in, &off);
        }
        dtmp.outfits = get_u8(in, &off);
        dtmp.furniture = get_u8(in, &off);
        dtmp.placed = get_u8(in, &off);
        dtmp.themes = get_u8(in, &off);
        dtmp.theme = get_u8(in, &off);
        if (!pt_decor_validate(&dtmp)) {
            return false;
        }
    }

    // v6：L2 基因型。v1–v5 存档按确定性种子补发一组首发基因（同一存档迁移结果稳定）。
    if (version >= 6u) {
        tmp.genome.body = get_u8(in, &off);
        tmp.genome.eyes = get_u8(in, &off);
        tmp.genome.face = get_u8(in, &off);
        tmp.genome.head = get_u8(in, &off);
        tmp.genome.palette = get_u8(in, &off);
        tmp.genome.back = get_u8(in, &off);
        tmp.genome.personality = get_u8(in, &off);
        tmp.genome.flags = get_u8(in, &off);
        if (!pt_genome_validate(&tmp.genome)) {
            return false;
        }
    } else {
        uint32_t gseed = pt_hash32(&tmp.rng_state, sizeof(tmp.rng_state));
        gseed ^= tmp.boot_count * 2654435761u;
        gseed ^= 0x6E656E65u;   // "gene"
        pt_genome_init_first(&tmp.genome, &gseed);
    }

    *s = tmp;
    if (e != NULL) {
        *e = etmp;
    }
    if (j != NULL) {
        *j = jtmp;
    }
    if (dcr != NULL) {
        *dcr = dtmp;
    }
    return true;
}
