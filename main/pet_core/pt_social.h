// pet_core/pt_social.h —— P2-S3a 离线社交状态机（designs 08 §1–§4）。
// 纯逻辑：不碰 NVS/LVGL/经济结构体；G 币通过 *coins 形参进出，便于主机测试。
// 链路：媒婆每日推荐 → 名单（≤3）→ 打招呼/送礼积累关系 → 热恋+戒指求婚
//       → 婚礼 → 同住 24h → 育种蛋就绪（基因组+祖辈环备好，S3b 接走世代交替）。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pt_events.h"
#include "pt_genome.h"
#include "pt_types.h"

#define PT_SOC_CANDIDATES 3
#define PT_SOC_HALL_MAX  10   // 家族名人堂代数上限（满后最旧一代淘汰，08 §5）

// 家族名人堂一条（一代玩家宠 + 其配偶），24B。
typedef struct {
    pt_genome_t pet;        // 该代玩家宠
    pt_genome_t mate;       // 配偶（未婚离代时为零基因）
    uint32_t generation;    // 玩家宠代次（首发蛋=0）
    uint8_t species;        // 成年定型 pt_species_t
    uint8_t care;           // pt_care_tier_t
    uint16_t adult_age_days;// 成家时年龄（游戏天）
} pt_soc_hall_t;            // 编码 24B（名人堂 10 条 = 240B）

// 关系阶段（08 §2 阈值）。
typedef enum {
    PT_REL_MET = 0,     // 相识
    PT_REL_FRIEND,      // 好感 25
    PT_REL_CRUSH,       // 暧昧 50
    PT_REL_LOVE,        // 热恋 80（可求婚）
} pt_rel_stage_t;

typedef enum {
    PT_SOC_SINGLE = 0,    // 候选名单经营期
    PT_SOC_MARRIED,       // 已婚同住（蛋出现前）
    PT_SOC_EGG_READY,     // 同住满 24h，子代蛋已育种（S3b 接管）
} pt_soc_phase_t;

typedef enum {
    PT_RING_SIMPLE = 0,   // 普通戒
    PT_RING_DIAMOND,      // 钻戒（接受率/对象质量加成）
} pt_ring_t;

typedef enum {
    PT_SOC_OK = 0,
    PT_SOC_BAD_STATE,     // 阶段/相位不对
    PT_SOC_BAD_SLOT,      // 候选槽无效
    PT_SOC_CAPPED,        // 今日互动已达 3 次
    PT_SOC_COOLDOWN,      // 两次互动需隔 2 小时
    PT_SOC_POOR,          // G 币不足
    PT_SOC_NOT_LOVE,      // 关系未到热恋 80
    PT_SOC_LOCKED,        // 被拒后 48h 冷静期
    PT_SOC_REJECTED,      // 求婚被拒（不扣关系/钱）
} pt_soc_rv_t;

typedef struct {
    pt_genome_t genome;       // 名片基因型（决定剪影/性格倾向）
    uint8_t bond;             // 关系值 0..100
    uint8_t interacts_today;  // 今日已互动次数
    int32_t last_interact;    // 最近互动绝对分钟；-1=从未
    int32_t last_day;         // 最近互动所在清醒日（衰减用）
    int32_t propose_lock;     // 被拒后可再求的绝对分钟；0=无锁
    bool valid;
} pt_soc_cand_t;

typedef struct {
    pt_soc_phase_t phase;
    uint32_t rng_state;
    int32_t day_id;           // 已结算到的清醒日（配额重置/衰减/每日推荐）
    int32_t last_roll_day;    // 媒婆上次推荐的清醒日；INT32_MIN=从未
    pt_soc_cand_t cand[PT_SOC_CANDIDATES];

    // 已婚/蛋就绪
    pt_genome_t spouse;
    uint8_t spouse_personality;
    int32_t married_at;       // 婚礼绝对分钟

    // EGG_READY：S3b 开启子代新蛋时取用
    pt_genome_t child_genome;
    pt_genome_t child_anc[8];
    uint8_t child_anc_present;

    // P2-S3b 世代/育儿/家谱
    uint32_t generation;             // 当前主角代次（首发蛋 0）
    pt_genome_t active_anc[8];       // 当前主角祖辈环（供下一次育种）
    uint8_t active_anc_present;
    uint8_t care_active;             // 父母同住育儿期（到子代进入少年止）
    pt_soc_hall_t pending;           // 育儿期结束时入堂的父母一代
    uint8_t pending_valid;
    pt_soc_hall_t hall[PT_SOC_HALL_MAX];
    uint8_t hall_count;
} pt_social_t;

// 名字由名片基因确定性派生（2 音节、≤6 字符 ASCII），无需存档。
void pt_social_name(const pt_genome_t *g, char *out, uint8_t cap);

// 关系阶段（阈值 0/25/50/80）。
pt_rel_stage_t pt_social_rel_stage(uint8_t bond);
const char *pt_social_rel_name(pt_rel_stage_t st);
uint8_t pt_social_candidate_count(const pt_social_t *so);

// 新存档社交状态（新蛋时随生命种子一起初始化）。
void pt_social_init(pt_social_t *so, uint32_t seed, int32_t day_id);

// 引擎每分钟推进后调用：清醒日界处理（推荐名额/每日配额/衰减）、同住计时、
// 育种。事件推入 q（PT_EV_SOC_CANDIDATE/WEDDING/EGG_READY）。
void pt_social_on_minute(pt_social_t *so, const pt_state_t *s,
                         pt_events_t *q);

// 打招呼：每日前 3 次有效、单次冷却 120 分钟，每次 +5（08 §2）。
pt_soc_rv_t pt_social_greet(pt_social_t *so, uint8_t ci, int32_t minute);

// 送礼：80G；送对（性格相性）+15、送错 +3；同样受每日次数/冷却约束。
// 成功才扣钱。
pt_soc_rv_t pt_social_gift(pt_social_t *so, uint8_t ci, int32_t minute,
                           uint32_t *coins, uint8_t player_personality);

// 求婚：需热恋（≥80）；接受率 = 40%+(bond-80)*2% + 钻戒 25%（封顶 95%）。
// 接受：扣戒指钱、相位转 MARRIED、推婚礼事件、清空候选；
// 拒绝：不扣关系/钱，置 48h 冷静期，返回 PT_SOC_REJECTED。
pt_soc_rv_t pt_social_propose(pt_social_t *so, uint8_t ci, pt_ring_t ring,
                              int32_t minute, uint32_t *coins,
                              pt_events_t *q);

// 距离同住结束（蛋到来）剩余分钟；仅 MARRIED 有效，否则返回 -1。
int32_t pt_social_egg_minutes_left(const pt_social_t *so, int32_t minute);

// ---- P2-S3b 世代交替 / 育儿 / 家谱 ----
uint32_t pt_social_generation(const pt_social_t *so);
bool pt_social_care_active(const pt_social_t *so);
uint8_t pt_social_hall_count(const pt_social_t *so);
const pt_soc_hall_t *pt_social_hall_entry(const pt_social_t *so, uint8_t i);

// EGG_READY 状态下玩家迎接新蛋：归档父母待入堂数据、代次 +1、祖辈环过户、
// 社交回 SINGLE（候选清空，成年后重新推荐）、进入育儿照顾期。
// 返回 false 表示当前相位不允许。子代基因组由调用方配合 pt_state_new_egg_genome
// 取用（so->child_genome）。
bool pt_social_begin_generation(pt_social_t *so, const pt_state_t *parent);

// 育儿期结束（子代进入少年，由 on_minute 自动调用并推 PARENTS_LEAVE 事件）：
// 父母一代正式入家谱。返回 true 表示本次发生了离队。
bool pt_social_end_parenting(pt_social_t *so, int32_t minute, pt_events_t *q);
