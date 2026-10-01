// 主机测试：P2-S4 PET3 图鉴 blob（256B 定长、magic/version/CRC、字段合法性）。
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pet_dex_save.h"
#include "pt_dex.h"

static pt_genome_t genome_at(uint8_t idx)
{
    pt_genome_t g;
    memset(&g, 0, sizeof(g));
    g.body = pt_part_make(idx, pt_catalog_rarity(PT_GENE_SLOT_BODY, idx));
    g.eyes = pt_part_make(idx, pt_catalog_rarity(PT_GENE_SLOT_EYES, idx));
    g.face = pt_part_make(idx, pt_catalog_rarity(PT_GENE_SLOT_FACE, idx));
    g.head = pt_part_make(idx, pt_catalog_rarity(PT_GENE_SLOT_HEAD, idx));
    g.palette = pt_part_make(idx, pt_catalog_rarity(PT_GENE_SLOT_PALETTE,
                                                    idx));
    g.back = pt_part_make(idx, pt_catalog_rarity(PT_GENE_SLOT_BACK, idx));
    return g;
}

int main(void)
{
    // ---- 往返：全字段填充 ----
    pt_dex_t d;
    pt_dex_init(&d);
    pt_dex_observe_species(&d, PT_SP_TEEN_A, PT_DEX_LV_MASTERED);
    pt_dex_raise_species(&d, PT_SP_ADULT_PERFECT, PT_CARE_PERFECT);
    pt_dex_raise_species(&d, PT_SP_ADULT_PERFECT, PT_CARE_GREAT);
    pt_genome_t g0 = genome_at(0);
    pt_genome_t g5 = genome_at(5);
    pt_dex_observe_genome(&d, &g0, PT_DEX_PART_OWNED);
    pt_dex_observe_genome(&d, &g5, PT_DEX_PART_BRED);
    d.oldest_days = 31200;
    uint16_t amount = 0;
    (void) pt_dex_poll_rewards(&d, &amount);  // claimed 非零
    assert(d.claimed != 0);

    uint8_t blob[PET_DEX_SAVE_BYTES];
    assert(pet_dex_save_encode(&d, blob, sizeof(blob)) == PET_DEX_SAVE_BYTES);
    // 头 4 字节 "PET3"，第 5 字节版本 1。
    assert(blob[0] == '3' && blob[1] == 'T' && blob[2] == 'E'
           && blob[3] == 'P');
    assert(blob[4] == PET_DEX_SAVE_VERSION);

    pt_dex_t out;
    assert(pet_dex_save_decode(&out, blob, sizeof(blob)));
    assert(out.level[0] == PT_DEX_LV_MASTERED);
    assert(out.raised[3] == 2);
    assert(out.best_care[3] == PT_CARE_PERFECT);
    assert(out.oldest_days == 31200);
    assert(out.claimed == d.claimed);
    assert(pt_dex_part_has(&out, PT_DEX_PART_OWNED, PT_GENE_SLOT_BODY, 0));
    assert(pt_dex_part_has(&out, PT_DEX_PART_BRED, PT_GENE_SLOT_BACK, 5));
    assert(pt_dex_parts_count(&out, PT_DEX_PART_SEEN) == 12);
    for (uint8_t i = 0; i < PT_DEX_SPECIES; i += 1) {
        assert(out.best_care[i] == 0xFF || out.best_care[i] < PT_DEX_CARE_COUNT);
    }

    // 空册也往返（best_care 全 0xFF）。
    pt_dex_t empty;
    pt_dex_init(&empty);
    assert(pet_dex_save_encode(&empty, blob, sizeof(blob))
           == PET_DEX_SAVE_BYTES);
    pt_dex_t empty_out;
    assert(pet_dex_save_decode(&empty_out, blob, sizeof(blob)));
    assert(empty_out.oldest_days == 0 && empty_out.claimed == 0);
    for (uint8_t i = 0; i < PT_DEX_SPECIES; i += 1) {
        assert(empty_out.best_care[i] == 0xFF);
    }

    // ---- 拒绝：长度不符 ----
    assert(!pet_dex_save_decode(&out, blob, 128));
    uint8_t big[512];
    assert(!pet_dex_save_decode(&out, big, sizeof(big)));
    // cap 不符编码失败。
    assert(pet_dex_save_encode(&d, blob, 128) == 0);

    // ---- 拒绝：magic / version 错 ----
    uint8_t bad[PET_DEX_SAVE_BYTES];
    memcpy(bad, blob, sizeof(bad));
    bad[0] = 'X';
    assert(!pet_dex_save_decode(&out, bad, sizeof(bad)));
    memcpy(bad, blob, sizeof(bad));
    bad[4] = 9;
    assert(!pet_dex_save_decode(&out, bad, sizeof(bad)));

    // ---- 拒绝：CRC 损坏（body / crc 各试一字节）----
    memcpy(bad, blob, sizeof(bad));
    bad[20] ^= 0x01;
    assert(!pet_dex_save_decode(&out, bad, sizeof(bad)));
    memcpy(bad, blob, sizeof(bad));
    bad[PET_DEX_SAVE_BYTES - 1] ^= 0x80;
    assert(!pet_dex_save_decode(&out, bad, sizeof(bad)));

    // ---- 拒绝：非法枚举字段 ----
    memcpy(bad, blob, sizeof(bad));
    bad[6] = 9;   // level 越界（level[0] 在偏移 6）
    assert(!pet_dex_save_decode(&out, bad, sizeof(bad)));
    memcpy(bad, blob, sizeof(bad));
    bad[6 + 8 + 8] = 7;   // best_care 非法（非 0xFF/0..3）
    assert(!pet_dex_save_decode(&out, bad, sizeof(bad)));

    // ---- 拒绝：OWNED 不是 SEEN 子集 / raised>0 但等级不足 ----
    pt_dex_t bogus;
    pt_dex_init(&bogus);
    bogus.parts_owned[0] = 0x01;   // seen 全空
    assert(pet_dex_save_encode(&bogus, bad, sizeof(bad))
           == PET_DEX_SAVE_BYTES);
    assert(!pet_dex_save_decode(&out, bad, sizeof(bad)));

    pt_dex_t bogus2;
    pt_dex_init(&bogus2);
    bogus2.raised[2] = 1;          // level[2] 仍为 NONE
    assert(pet_dex_save_encode(&bogus2, bad, sizeof(bad))
           == PET_DEX_SAVE_BYTES);
    assert(!pet_dex_save_decode(&out, bad, sizeof(bad)));

    // 未知里程碑位（高位）不拒绝，读出时被掩码丢弃，CRC 需重算。
    pt_dex_t fut;
    pt_dex_init(&fut);
    assert(pet_dex_save_encode(&fut, bad, sizeof(bad))
           == PET_DEX_SAVE_BYTES);
    size_t claimed_off = 6u + 8u + 8u + 8u + 8u + 8u + 8u + 2u;
    bad[claimed_off] = 0xFE;   // 含 1 个已知位 + 6 个未知位
    // 重算 CRC。
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < PET_DEX_SAVE_BYTES - 4; i += 1) {
        crc ^= bad[i];
        for (int b = 0; b < 8; b += 1) {
            crc = (crc >> 1)
                  ^ (0xEDB88320u & (uint32_t) (-(int32_t) (crc & 1)));
        }
    }
    crc ^= 0xFFFFFFFFu;
    bad[PET_DEX_SAVE_BYTES - 4] = (uint8_t) (crc & 0xff);
    bad[PET_DEX_SAVE_BYTES - 3] = (uint8_t) ((crc >> 8) & 0xff);
    bad[PET_DEX_SAVE_BYTES - 2] = (uint8_t) ((crc >> 16) & 0xff);
    bad[PET_DEX_SAVE_BYTES - 1] = (uint8_t) ((crc >> 24) & 0xff);
    assert(pet_dex_save_decode(&out, bad, sizeof(bad)));
    assert(out.claimed == 0x7E);  // 最高未知位也被保留到 0x7F 掩码内（7 位）

    printf("pet_dex_save: all tests passed\n");
    return 0;
}
