// 主机测试：存档序列化往返、CRC 损坏检测、版本/魔数拒收（designs 11 §5）。
// v2 起经济状态并入同一 blob；v1 老存档必须平滑迁移。
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pet_decor.h"
#include "pet_econ.h"
#include "pet_jobs.h"
#include "pet_save.h"
#include "pt_config.h"
#include "pt_engine.h"
#include "pt_events.h"

#define START (10 * 1440 + 360)

static pt_jobs_t g_jobs;
static pt_decor_t g_decor;

// 与 pet_save.c 同一 CRC32（用于把 v2 blob 改写成 v1 后重算校验）。
static uint32_t crc32_blob(const uint8_t *p)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < PET_SAVE_BYTES - 4; i += 1) {
        crc ^= p[i];
        for (int b = 0; b < 8; b += 1) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t) (-(int32_t)(crc & 1)));
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

static void put_u32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; i += 1) {
        p[i] = (uint8_t) (v >> (8 * i));
    }
}

// v3 blob 中各新增字段的偏移（与 pet_save.c 编码顺序对应，仅本迁移测试使用）。
#define V3_OFF_SKILL        131   // skill[3]+skill_today[3]+games_today = 7B
#define V3_OFF_DIET_L2      117   // 第二个账本 diet[5]（u16×5 = 10B）
#define V3_OFF_DIET_L1      100
#define V3_OFF_GAME_PAYS    224   // 经济块尾部 1B
#define V4_OFF_JOBS         225   // v4 职业块 6B
#define V5_OFF_DECOR        231   // v5 装饰块 9B

static void finish_blob(uint8_t *buf, uint32_t version)
{
    put_u32(&buf[4], version);
    uint32_t crc = crc32_blob(buf);
    size_t crc_off = PET_SAVE_BYTES - 4;
    buf[crc_off] = (uint8_t) crc;
    buf[crc_off + 1] = (uint8_t) (crc >> 8);
    buf[crc_off + 2] = (uint8_t) (crc >> 16);
    buf[crc_off + 3] = (uint8_t) (crc >> 24);
}

// 从当前 blob 删去新增段（从高偏移向低删），重算 CRC，得到 v2 blob。
static void downgrade_v3_to_v2(uint8_t *buf)
{
    // v5 → v4：删装饰块 9B。
    memmove(buf + V5_OFF_DECOR, buf + V5_OFF_DECOR + 9,
            PET_SAVE_BYTES - V5_OFF_DECOR - 9);
    memset(buf + PET_SAVE_BYTES - 9, 0, 9);
    // v4 → v3：删职业块 6B（之后缓冲与 v3 同构，尾部为无害填充）。
    memmove(buf + V4_OFF_JOBS, buf + V4_OFF_JOBS + 6,
            PET_SAVE_BYTES - V4_OFF_JOBS - 6);
    memset(buf + PET_SAVE_BYTES - 6, 0, 6);
    // v3 → v2：游戏发奖计数、技能块、两段饮食账本。
    memmove(buf + V3_OFF_GAME_PAYS, buf + V3_OFF_GAME_PAYS + 1,
            PET_SAVE_BYTES - V3_OFF_GAME_PAYS - 1);
    buf[PET_SAVE_BYTES - 1] = 0;
    memmove(buf + V3_OFF_SKILL, buf + V3_OFF_SKILL + 7,
            PET_SAVE_BYTES - V3_OFF_SKILL - 7);
    memset(buf + PET_SAVE_BYTES - 8, 0, 8);
    memmove(buf + V3_OFF_DIET_L2, buf + V3_OFF_DIET_L2 + 10,
            PET_SAVE_BYTES - V3_OFF_DIET_L2 - 10);
    memset(buf + PET_SAVE_BYTES - 18, 0, 10);
    memmove(buf + V3_OFF_DIET_L1, buf + V3_OFF_DIET_L1 + 10,
            PET_SAVE_BYTES - V3_OFF_DIET_L1 - 10);
    memset(buf + PET_SAVE_BYTES - 28, 0, 10);
    finish_blob(buf, 2u);
}

static void test_v3_fields_roundtrip(void)
{
    pt_state_t s;
    pt_econ_t e;
    pt_state_new(&s, 4242u, START);
    pt_econ_init(&e, 9u);

    s.skill[PT_SKILL_MIND] = 40;
    s.skill[PT_SKILL_BODY] = 25;
    s.skill[PT_SKILL_ART] = 9;
    s.skill_today[PT_SKILL_MIND] = 7;
    s.games_today = 6;
    s.ledger.diet[PT_FOOD_TAG_SWEET] = 3;
    s.prev_ledger.diet[PT_FOOD_TAG_MEAT] = 60000u;
    (void) pt_econ_game_payout(&e, PT_GAME_GRADE_PERFECT);
    assert(e.game_pays_today == 1);

    uint8_t buf[PET_SAVE_BYTES];
    assert(pet_save_encode(&s, &e, &g_jobs, &g_decor, buf, sizeof(buf)) == PET_SAVE_BYTES);

    pt_state_t sr;
    pt_econ_t er;
    assert(pet_save_decode(&sr, &er, NULL, NULL, buf, sizeof(buf)));
    assert(memcmp(&s, &sr, sizeof(s)) == 0);
    assert(memcmp(&e, &er, sizeof(e)) == 0);
    assert(er.game_pays_today == 1 && er.coins == 30);
}

static void test_v2_save_migrates_zeros(void)
{
    pt_state_t s;
    pt_econ_t e;
    pt_state_new(&s, 8889u, START);
    pt_econ_init(&e, 123u);
    e.coins = 250;
    assert(pt_econ_add(&e, PT_ITEM_BISCUIT, 2) == PT_ECON_OK);
    s.snacks_today = 2;

    uint8_t buf[PET_SAVE_BYTES];
    assert(pet_save_encode(&s, &e, &g_jobs, &g_decor, buf, sizeof(buf)) == PET_SAVE_BYTES);
    downgrade_v3_to_v2(buf);

    pt_state_t sr;
    pt_econ_t er;
    memset(&sr, 0x33, sizeof(sr));
    memset(&er, 0x44, sizeof(er));
    assert(pet_save_decode(&sr, &er, NULL, NULL, buf, sizeof(buf)));

    // v2 老字段保留：钱、饼干库存、日配额都在。
    assert(er.coins == 250);
    assert(pt_econ_count(&er, PT_ITEM_BISCUIT) == 2);
    assert(sr.snacks_today == 2);
    // v3 新增字段全部按零值迁移。
    assert(sr.skill[PT_SKILL_MIND] == 0 && sr.skill[PT_SKILL_BODY] == 0
           && sr.skill[PT_SKILL_ART] == 0);
    assert(sr.games_today == 0);
    assert(sr.ledger.diet[PT_FOOD_TAG_SWEET] == 0);
    assert(er.game_pays_today == 0);
}

static void test_v4_jobs_roundtrip(void)
{
    pt_state_t s;
    pt_econ_t e;
    pt_jobs_t j;
    pt_state_new(&s, 4244u, START);
    pt_econ_init(&e, 9u);
    pt_jobs_init(&j);
    assert(pt_jobs_switch(&j, PT_JOB_LIBRARIAN,
                          (uint8_t[PT_SKILL_COUNT]) { 40, 0, 0 }, 31));
    // 两班 PERFECT + 一班 GREAT：工资入账由装配层负责，这里只验持久化。
    (void) pt_jobs_work(&j, PT_JOB_LIBRARIAN, PT_GAME_GRADE_PERFECT);
    (void) pt_jobs_work(&j, PT_JOB_LIBRARIAN, PT_GAME_GRADE_PERFECT);
    (void) pt_jobs_work(&j, PT_JOB_LIBRARIAN, PT_GAME_GRADE_GREAT);
    pt_jobs_on_day(&j);
    assert(j.shifts_today == 0 && j.perfect_run == 0);
    (void) pt_jobs_work(&j, PT_JOB_LIBRARIAN, PT_GAME_GRADE_PERFECT);
    pt_jobs_on_day(&j);
    (void) pt_jobs_work(&j, PT_JOB_LIBRARIAN, PT_GAME_GRADE_PERFECT);
    pt_jobs_on_day(&j);
    (void) pt_jobs_work(&j, PT_JOB_LIBRARIAN, PT_GAME_GRADE_PERFECT);
    pt_jobs_on_day(&j);
    assert(j.worker_sticker == 1);   // 连续 3 个全班完美工作日

    uint8_t buf[PET_SAVE_BYTES];
    assert(pet_save_encode(&s, &e, &j, &g_decor, buf, sizeof(buf)) == PET_SAVE_BYTES);

    pt_jobs_t jr;
    memset(&jr, 0x55, sizeof(jr));
    assert(pet_save_decode(&s, &e, &jr, NULL, buf, sizeof(buf)));
    assert(jr.job == PT_JOB_LIBRARIAN);
    assert(jr.worker_sticker == 1);
    assert(jr.perfect_run == 3);
}

static void test_v5_decor_roundtrip(void)
{
    pt_state_t s;
    pt_econ_t e;
    pt_jobs_t j;
    pt_decor_t d;
    pt_state_new(&s, 4250u, START);
    pt_econ_init(&e, 9u);
    pt_jobs_init(&j);
    pt_decor_init(&d);
    e.coins = 5000;
    e.shells = 30;
    assert(pt_decor_buy_outfit(&d, &e, PT_STAGE_TEEN, 0,
                               PT_OUTFIT_CAP) == PT_DECOR_OK);
    assert(pt_decor_buy_outfit(&d, &e, PT_STAGE_TEEN, 4,
                               PT_OUTFIT_STARHAT) == PT_DECOR_OK);
    assert(pt_decor_equip(&d, PT_OUTFIT_STARHAT) == PT_DECOR_OK);
    assert(pt_decor_buy_furn(&d, &e, PT_STAGE_TEEN, 9,
                             PT_FURN_PLANT) == PT_DECOR_OK);
    assert(pt_decor_place(&d, PT_FURN_PLANT) == PT_DECOR_OK);
    assert(pt_decor_buy_theme(&d, &e, PT_THEME_SKY) == PT_DECOR_OK);
    assert(pt_decor_set_theme(&d, PT_THEME_SKY) == PT_DECOR_OK);
    // cap 200G 全价；盆栽 300G 在月历第 10 日（day_id=9）享家具 9 折=270G；
    // 星空帽走贝壳；晴空主题 400G。
    assert(e.coins == 5000 - 200 - 270 - 400);
    assert(e.shells == 30 - 8);

    uint8_t buf[PET_SAVE_BYTES];
    assert(pet_save_encode(&s, &e, &j, &d, buf, sizeof(buf))
           == PET_SAVE_BYTES);

    pt_decor_t dr;
    memset(&dr, 0x66, sizeof(dr));
    assert(pet_save_decode(&s, &e, NULL, &dr, buf, sizeof(buf)));
    assert(memcmp(&d, &dr, sizeof(d)) == 0);
    assert(dr.theme == PT_THEME_SKY);
    assert(pt_decor_worn(&dr, PT_SLOT_HAT) == PT_OUTFIT_STARHAT);
    assert(pt_decor_is_placed(&dr, PT_FURN_PLANT));
    assert(pt_decor_happy_pct(&dr) == PT_DECOR_PLANT_HAPPY_PCT);
    assert(pt_decor_meal_bonus(&dr) == 0);
    assert(pt_decor_energy_pct(&dr) == 100);
}

static void test_v5_dirty_decor_rejected(void)
{
    pt_state_t s;
    pt_econ_t e;
    pt_decor_t d;
    pt_state_new(&s, 4251u, START);
    pt_econ_init(&e, 1u);
    pt_decor_init(&d);
    uint8_t buf[PET_SAVE_BYTES];
    assert(pet_save_encode(&s, &e, &g_jobs, &d, buf, sizeof(buf))
           == PET_SAVE_BYTES);

    // 装饰块偏移 231：worn[4] 231-234 / outfits 235 / furniture 236 /
    // placed 237 / themes 238 / theme 239。摆放未拥有家具属脏写。
    buf[237] = 0xFF;
    finish_blob(buf, PET_SAVE_VERSION);
    pt_state_t sr;
    pt_econ_t er;
    pt_decor_t dr;
    assert(!pet_save_decode(&sr, &er, NULL, &dr, buf, sizeof(buf)));
}

static void test_v6_genome_roundtrip(void)
{
    pt_state_t s;
    pt_econ_t e;
    pt_jobs_t j;
    pt_decor_t d;
    pt_state_new(&s, 4260u, START);
    pt_econ_init(&e, 11u);
    pt_jobs_init(&j);
    pt_decor_init(&d);

    // 手工置一组含稀有件/纯血/突变/首发标记的合法基因。
    s.genome.body = pt_part_make(7, PT_RAR_R);
    s.genome.eyes = pt_part_make(11, PT_RAR_L);
    s.genome.face = pt_part_make(0, PT_RAR_C);
    s.genome.head = pt_part_make(8, PT_RAR_U);
    s.genome.palette = pt_part_make(15, PT_RAR_L);
    s.genome.back = pt_part_make(7, PT_RAR_L);
    s.genome.personality = PT_PERS_SASSY;
    s.genome.flags = PT_GF_PURE_EYES | PT_GF_MUTATED | PT_GF_FIRST_EGG;
    assert(pt_genome_validate(&s.genome));

    uint8_t buf[PET_SAVE_BYTES];
    assert(pet_save_encode(&s, &e, &j, &d, buf, sizeof(buf))
           == PET_SAVE_BYTES);
    // 基因块固定落在偏移 242..249（职业 7B + 装饰 11B 之后）。
    assert(buf[242] == s.genome.body && buf[249] == s.genome.flags);

    pt_state_t sr;
    memset(&sr, 0x55, sizeof(sr));
    assert(pet_save_decode(&sr, &e, NULL, NULL, buf, sizeof(buf)));
    assert(memcmp(&sr.genome, &s.genome, sizeof(pt_genome_t)) == 0);
}

static void test_v6_dirty_genome_rejected(void)
{
    pt_state_t s;
    pt_econ_t e;
    pt_decor_t d;
    pt_state_new(&s, 4261u, START);
    pt_econ_init(&e, 12u);
    pt_decor_init(&d);
    uint8_t buf[PET_SAVE_BYTES];
    assert(pet_save_encode(&s, &e, &g_jobs, &d, buf, sizeof(buf))
           == PET_SAVE_BYTES);

    // 偏移 243（eyes）写入目录内不存在的部件 → 脏数据拒收。
    buf[243] = 0xFF;
    finish_blob(buf, PET_SAVE_VERSION);
    pt_state_t sr;
    pt_econ_t er;
    assert(!pet_save_decode(&sr, &er, NULL, NULL, buf, sizeof(buf)));
}

static void test_v5_migrates_genome(void)
{
    pt_state_t s;
    pt_econ_t e;
    pt_decor_t d;
    pt_state_new(&s, 4262u, START);
    pt_econ_init(&e, 13u);
    pt_decor_init(&d);
    uint8_t buf[PET_SAVE_BYTES];
    assert(pet_save_encode(&s, &e, &g_jobs, &d, buf, sizeof(buf))
           == PET_SAVE_BYTES);

    // v6 → v5：删基因块 8B（偏移 242；尾部 CRC 随 memmove 移位后重算）。
    memmove(buf + 242, buf + 250, PET_SAVE_BYTES - 250);
    memset(buf + PET_SAVE_BYTES - 8, 0, 8);
    finish_blob(buf, 5u);

    pt_state_t a, b;
    assert(pet_save_decode(&a, &e, NULL, NULL, buf, sizeof(buf)));
    assert(pt_genome_validate(&a.genome));
    assert((a.genome.flags & PT_GF_FIRST_EGG) != 0);
    // 同一旧档迁移结果确定（不读 RNG 流、不随调用变化）。
    assert(pet_save_decode(&b, &e, NULL, NULL, buf, sizeof(buf)));
    assert(memcmp(&a.genome, &b.genome, sizeof(pt_genome_t)) == 0);
}

static void test_v3_dirty_skill_rejected(void)
{
    pt_state_t s;
    pt_econ_t e;
    pt_state_new(&s, 4243u, START);
    pt_econ_init(&e, 1u);
    uint8_t buf[PET_SAVE_BYTES];
    assert(pet_save_encode(&s, &e, &g_jobs, &g_decor, buf, sizeof(buf)) == PET_SAVE_BYTES);

    // 技能值超过 99 属脏写：CRC 修正后仍必须拒收。
    buf[V3_OFF_SKILL] = 100;
    finish_blob(buf, PET_SAVE_VERSION);
    pt_state_t sr;
    pt_econ_t er;
    assert(!pet_save_decode(&sr, &er, NULL, NULL, buf, sizeof(buf)));
}

static void test_roundtrip_preserves_state(void)
{
    pt_state_t s;
    pt_econ_t e;
    pt_events_t q;
    pt_events_init(&q);
    pt_state_new(&s, 1234u, START);
    pt_econ_init(&e, 777u);

    // 让状态带上各种非默认值：推进一段、喂食、排便、生病。
    pt_advance_to(&s, &q, s.minute + PT_CFG_EGG_MIN + PT_CFG_BABY_MIN + 300);
    pt_handle_intent(&s, &q, PT_INTENT_FEED_MEAL, 0);
    pt_handle_intent(&s, &q, PT_INTENT_PAT, 0);
    pt_events_clear(&q);

    uint8_t buf[PET_SAVE_BYTES];
    size_t n = pet_save_encode(&s, &e, &g_jobs, &g_decor, buf, sizeof(buf));
    assert(n > 0);
    assert(n <= PET_SAVE_BYTES);

    pt_state_t restored;
    pt_econ_t er;
    memset(&restored, 0xAA, sizeof(restored));
    memset(&er, 0xAA, sizeof(er));
    assert(pet_save_decode(&restored, &er, NULL, NULL, buf, n));
    assert(memcmp(&s, &restored, sizeof(s)) == 0);
    assert(memcmp(&e, &er, sizeof(e)) == 0);
}

static void test_economy_roundtrip(void)
{
    pt_state_t s;
    pt_econ_t e;
    pt_state_new(&s, 321u, START);
    pt_econ_init(&e, 31337u);
    e.coins = 42000;
    e.shells = 7;
    e.streak = 4;
    e.checkin_day = s.day_id - 1;
    assert(pt_econ_add(&e, PT_ITEM_RICEBALL, 3) == PT_ECON_OK);
    assert(pt_econ_add(&e, PT_ITEM_BALL, 1) == PT_ECON_OK);
    e.toy_uses[PT_ITEM_BALL] = 2;

    uint8_t buf[PET_SAVE_BYTES];
    assert(pet_save_encode(&s, &e, &g_jobs, &g_decor, buf, sizeof(buf)) == PET_SAVE_BYTES);

    pt_econ_t er;
    pt_state_t sr;
    assert(pet_save_decode(&sr, &er, NULL, NULL, buf, sizeof(buf)));
    assert(er.coins == 42000 && er.shells == 7 && er.streak == 4);
    assert(er.checkin_day == s.day_id - 1);
    assert(er.shelf_seed == 31337u);
    assert(pt_econ_count(&er, PT_ITEM_RICEBALL) == 3);
    assert(pt_econ_count(&er, PT_ITEM_BALL) == 1);
    assert(er.toy_uses[PT_ITEM_BALL] == 2);
}

static void test_v1_save_migrates_to_default_economy(void)
{
    pt_state_t s;
    pt_econ_t e;
    pt_state_new(&s, 8888u, START);
    pt_econ_init(&e, 1u);

    uint8_t buf[PET_SAVE_BYTES];
    assert(pet_save_encode(&s, &e, &g_jobs, &g_decor, buf, sizeof(buf)) == PET_SAVE_BYTES);

    // 改写为 v1（生命字段布局在 v1/v2 完全相同，尾部经济字节解码方应忽略）。
    put_u32(&buf[4], 1u);
    uint32_t crc = crc32_blob(buf);
    size_t crc_off = PET_SAVE_BYTES - 4;
    buf[crc_off] = (uint8_t) crc;
    buf[crc_off + 1] = (uint8_t) (crc >> 8);
    buf[crc_off + 2] = (uint8_t) (crc >> 16);
    buf[crc_off + 3] = (uint8_t) (crc >> 24);

    pt_state_t sr;
    pt_econ_t er;
    memset(&er, 0x55, sizeof(er));
    assert(pet_save_decode(&sr, &er, NULL, NULL, buf, sizeof(buf)));
    // v1 无基因块：迁移补发确定性首发基因（不与内存新蛋逐字节相同），
    // 其余生命字段必须逐字节一致。
    pt_genome_t migrated = sr.genome;
    assert(pt_genome_validate(&migrated));
    assert((migrated.flags & PT_GF_FIRST_EGG) != 0);
    memset(&s.genome, 0, sizeof(s.genome));
    memset(&sr.genome, 0, sizeof(sr.genome));
    assert(memcmp(&s, &sr, sizeof(s)) == 0);
    // 同一 v1 档两次迁移基因一致。
    pt_state_t sr2;
    assert(pet_save_decode(&sr2, &er, NULL, NULL, buf, sizeof(buf)));
    assert(memcmp(&migrated, &sr2.genome, sizeof(migrated)) == 0);
    assert(er.coins == 0 && er.shells == 0);
    assert(er.checkin_day == -1);
    assert(er.shelf_seed != 0);
    assert(pt_econ_count(&er, PT_ITEM_RICEBALL) == 0);
}

static void test_corruption_is_rejected(void)
{
    pt_state_t s;
    pt_econ_t e;
    pt_state_new(&s, 5u, START);
    pt_econ_init(&e, 1u);

    uint8_t buf[PET_SAVE_BYTES];
    size_t n = pet_save_encode(&s, &e, &g_jobs, &g_decor, buf, sizeof(buf));
    assert(n > 0);

    // 篡改任意一个数据字节：CRC 必须发现。
    uint8_t tampered[PET_SAVE_BYTES];
    memcpy(tampered, buf, n);
    tampered[20] ^= 0x01;

    pt_state_t out;
    pt_econ_t eo;
    assert(!pet_save_decode(&out, &eo, NULL, NULL, tampered, n));
}

static void test_bad_magic_and_version_rejected(void)
{
    pt_state_t s;
    pt_econ_t e;
    pt_state_new(&s, 6u, START);
    pt_econ_init(&e, 1u);
    uint8_t buf[PET_SAVE_BYTES];
    size_t n = pet_save_encode(&s, &e, &g_jobs, &g_decor, buf, sizeof(buf));

    uint8_t bad[PET_SAVE_BYTES];
    pt_state_t out;
    pt_econ_t eo;
    memcpy(bad, buf, n);
    bad[0] ^= 0xFF;                       // 破坏魔数
    assert(!pet_save_decode(&out, &eo, NULL, NULL, bad, n));

    memcpy(bad, buf, n);
    bad[4] = 99;                          // 未来版本号
    assert(!pet_save_decode(&out, &eo, NULL, NULL, bad, n));

    // 太短的缓冲必须拒收而不是越界读。
    assert(!pet_save_decode(&out, &eo, NULL, NULL, buf, 8));
}

static void test_failed_decode_leaves_state_untouched(void)
{
    pt_state_t s;
    pt_econ_t e;
    pt_state_new(&s, 7u, START);
    pt_econ_init(&e, 1u);
    uint8_t buf[PET_SAVE_BYTES];
    size_t n = pet_save_encode(&s, &e, &g_jobs, &g_decor, buf, sizeof(buf));
    buf[30] ^= 0x01;                      // 弄坏

    pt_state_t sentinel;
    pt_econ_t es;
    pt_state_new(&sentinel, 8u, START + 100);
    pt_econ_init(&es, 9u);
    pt_state_t before = sentinel;
    pt_econ_t ebefore = es;
    assert(!pet_save_decode(&sentinel, &es, NULL, NULL, buf, n));
    assert(memcmp(&before, &sentinel, sizeof(sentinel)) == 0);
    assert(memcmp(&ebefore, &es, sizeof(es)) == 0);
}

// 存档往返后继续推进，两条时间线应保持一致（存档不丢时间语义）。
static void test_resume_matches_uninterrupted(void)
{
    pt_events_t q;
    pt_events_init(&q);

    pt_state_t a, b;
    pt_econ_t ea, eb;
    pt_state_new(&a, 99u, START);
    pt_state_new(&b, 99u, START);
    pt_econ_init(&ea, 9u);
    pt_econ_init(&eb, 9u);

    pt_advance_to(&a, &q, a.minute + 500);
    pt_advance_to(&b, &q, b.minute + 500);
    pt_events_clear(&q);

    // b 走"存盘 → 读回"这一步，a 不存。
    uint8_t buf[PET_SAVE_BYTES];
    size_t n = pet_save_encode(&b, &eb, &g_jobs, &g_decor, buf, sizeof(buf));
    assert(pet_save_decode(&b, &eb, NULL, NULL, buf, n));

    pt_advance_to(&a, &q, a.minute + 600);
    pt_advance_to(&b, &q, b.minute + 600);
    assert(memcmp(&a, &b, sizeof(a)) == 0);
}

int main(void)
{
    pt_jobs_init(&g_jobs);
    pt_decor_init(&g_decor);
    test_roundtrip_preserves_state();
    test_economy_roundtrip();
    test_v3_fields_roundtrip();
    test_v4_jobs_roundtrip();
    test_v5_decor_roundtrip();
    test_v5_dirty_decor_rejected();
    test_v6_genome_roundtrip();
    test_v6_dirty_genome_rejected();
    test_v5_migrates_genome();
    test_v1_save_migrates_to_default_economy();
    test_v2_save_migrates_zeros();
    test_v3_dirty_skill_rejected();
    test_corruption_is_rejected();
    test_bad_magic_and_version_rejected();
    test_failed_decode_leaves_state_untouched();
    test_resume_matches_uninterrupted();
    printf("test_pet_save: PASS\n");
    return 0;
}
