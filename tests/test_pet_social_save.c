// 主机测试：P2-S3a/b 社交存档编解码（magic/version/CRC/v1→v2 迁移/往返）。
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pt_config.h"
#include "pt_engine.h"
#include "pt_events.h"
#include "pt_social.h"
#include "pet_social_save.h"
#include "pt_types.h"

#define DAY1_M (1440 + 8 * 60)

// 手工编码一个最小 v1（256B）blob，验证老存档迁移。
static size_t put32(uint8_t *p, size_t o, uint32_t v)
{
    for (int i = 0; i < 4; i += 1) {
        p[o + i] = (uint8_t) (v >> (8 * i));
    }
    return o + 4;
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

static void make_v1(uint8_t out[PET_SOC_SAVE_BYTES_V1])
{
    memset(out, 0, PET_SOC_SAVE_BYTES_V1);
    size_t o = 0;
    o = put32(out, o, 0x50455432u);   // magic
    out[o++] = 1u;                    // version
    out[o++] = (uint8_t) PT_SOC_MARRIED;
    o = put32(out, o, 0x12345678u);   // rng
    o = put32(out, o, 55u);           // day_id
    o = put32(out, o, 54u);           // last_roll_day
    // 3 个候选（全空：valid=0 + 字段零），每条 23B
    o += (size_t) PT_SOC_CANDIDATES * 23u;
    o += 8;                           // spouse genome 全零
    out[o++] = 2u;                    // spouse_personality
    o = put32(out, o, 9000u);         // married_at
    o += 8;                           // child_genome
    o += 64;                          // child_anc[8]
    out[o++] = 0x11u;                 // child_anc_present
    assert(o == 173);
    uint32_t crc = crc32_body(out, PET_SOC_SAVE_BYTES_V1 - 4);
    out[PET_SOC_SAVE_BYTES_V1 - 4] = (uint8_t) crc;
    out[PET_SOC_SAVE_BYTES_V1 - 3] = (uint8_t) (crc >> 8);
    out[PET_SOC_SAVE_BYTES_V1 - 2] = (uint8_t) (crc >> 16);
    out[PET_SOC_SAVE_BYTES_V1 - 1] = (uint8_t) (crc >> 24);
}

int main(void)
{
    // 1) v2 复杂状态往返：已婚 + 候选 + 互动计数 + 世代/家谱。
    pt_social_t so;
    pt_social_init(&so, 777, 1);
    pt_events_t q;
    pt_events_init(&q);
    pt_state_t s;
    memset(&s, 0, sizeof(s));
    s.stage = PT_STAGE_ADULT;
    s.age_days = 1;
    s.minute = DAY1_M;
    pt_genome_init_first(&s.genome, (uint32_t[1]){ 42u });
    pt_social_on_minute(&so, &s, &q);
    pt_social_greet(&so, 0, DAY1_M);
    so.phase = PT_SOC_MARRIED;
    so.married_at = DAY1_M;
    so.spouse = so.cand[0].genome;
    so.spouse_personality = 3;
    so.generation = 2;
    so.active_anc[3].body = 5;
    so.active_anc_present = (uint8_t) (1u << 3);
    so.care_active = 1;
    so.hall_count = 2;
    so.hall[0].generation = 0;
    so.hall[0].care = PT_CARE_PERFECT;
    so.hall[0].pet = s.genome;
    so.hall[1].generation = 1;
    so.hall[1].care = PT_CARE_GREAT;
    so.hall[1].mate = so.spouse;

    uint8_t buf[PET_SOC_SAVE_BYTES];
    assert(pet_soc_save_encode(&so, buf, sizeof(buf)) == PET_SOC_SAVE_BYTES);
    assert(buf[0] == '2' && buf[1] == 'T' && buf[2] == 'E' && buf[3] == 'P');

    pt_social_t got;
    assert(pet_soc_save_decode(&got, buf, sizeof(buf)));
    assert(memcmp(&got, &so, sizeof(got)) == 0);

    // 2) CRC 覆盖任意字节翻转
    uint8_t bad[PET_SOC_SAVE_BYTES];
    memcpy(bad, buf, sizeof(bad));
    bad[60] ^= 0x5A;
    assert(!pet_soc_save_decode(&got, bad, sizeof(bad)));

    // 3) 长度不符
    assert(!pet_soc_save_decode(&got, buf, 100));
    assert(pet_soc_save_encode(&so, buf, 100) == 0);

    // 4) 全零/垃圾 magic
    memset(bad, 0, sizeof(bad));
    assert(!pet_soc_save_decode(&got, bad, sizeof(bad)));

    // 5) EGG_READY（含祖辈环）往返
    so.phase = PT_SOC_EGG_READY;
    so.child_genome.body = 7;
    so.child_anc[0] = s.genome;
    so.child_anc[4] = so.spouse;
    so.child_anc_present = (uint8_t) ((1u << 0) | (1u << 4));
    assert(pet_soc_save_encode(&so, buf, sizeof(buf)) == PET_SOC_SAVE_BYTES);
    assert(pet_soc_save_decode(&got, buf, sizeof(buf)));
    assert(got.phase == PT_SOC_EGG_READY);
    assert(got.child_genome.body == 7);
    assert(got.child_anc_present == ((1u << 0) | (1u << 4)));

    // 6) v1（256B）老存档迁移：公共字段读出，S3b 字段全部安全置零。
    uint8_t v1[PET_SOC_SAVE_BYTES_V1];
    make_v1(v1);
    pt_social_t mig;
    assert(pet_soc_save_decode(&mig, v1, sizeof(v1)));
    assert(mig.phase == PT_SOC_MARRIED);
    assert(mig.rng_state == 0x12345678u);
    assert(mig.day_id == 55);
    assert(mig.married_at == 9000);
    assert(mig.spouse_personality == 2);
    assert(mig.child_anc_present == 0x11u);
    assert(mig.generation == 0);
    assert(mig.care_active == 0);
    assert(pt_social_hall_count(&mig) == 0);

    // 7) v1 blob 的 CRC 损坏必须拒绝。
    v1[100] ^= 0x01;
    assert(!pet_soc_save_decode(&mig, v1, sizeof(v1)));

    // 8) v2 内容塞进 256B 缓冲（长度不符）拒绝。
    assert(!pet_soc_save_decode(&mig, buf, PET_SOC_SAVE_BYTES_V1));

    printf("test_pet_social_save OK\n");
    return 0;
}
