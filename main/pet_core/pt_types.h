// pet_core/pt_types.h —— PasPet 核心类型：阶段、物种、需求 FSM、事件、意图。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pt_genome.h"

// 注意：阶段索引同时用于配置数组（衰减/窗口/便便间隔），顺序勿改。
typedef enum {
    PT_STAGE_EGG = 0,
    PT_STAGE_BABY,
    PT_STAGE_CHILD,
    PT_STAGE_TEEN,
    PT_STAGE_ADULT,
    PT_STAGE_SENIOR,
    PT_STAGE_DEAD,
    PT_STAGE_COUNT,
} pt_stage_t;

// 物种模板 ID（P0：固定链 + 四档成人；命名为原创占位名，见 designs 04 §8）。
typedef enum {
    PT_SP_NONE = 0,
    PT_SP_EGG,
    PT_SP_BABY,            // 圆宝
    PT_SP_CHILD,           // 豆丁
    PT_SP_TEEN_A,          // 糯糯
    PT_SP_TEEN_B,          // 咕咕
    PT_SP_TEEN_C,          // 毛毛
    PT_SP_ADULT_PERFECT,   // 雪团
    PT_SP_ADULT_GREAT,     // 棉花糖
    PT_SP_ADULT_NORMAL,    // 饭团
    PT_SP_ADULT_NEGLECT,   // 煤球
    PT_SP_ADULT_MOON,      // 月影（隐藏角色，见 pt_evolve）
} pt_species_t;

// 照顾质量档（进化维度①）
typedef enum {
    PT_CARE_PERFECT = 0,
    PT_CARE_GREAT,
    PT_CARE_NORMAL,
    PT_CARE_NEGLECT,
} pt_care_tier_t;

typedef enum {
    PT_CALL_NONE = 0,
    PT_CALL_HUNGRY,
    PT_CALL_SAD,
    PT_CALL_LIGHTS,
} pt_call_kind_t;

typedef enum {
    PT_NEED_FULL = 0,
    PT_NEED_HAPPY,
    PT_NEED_COUNT,
} pt_need_t;

// 维度②技能三维（07 §1 / 04 §3 维度②），顺序即配置表索引，勿改。
typedef enum {
    PT_SKILL_MIND = 0,
    PT_SKILL_BODY,
    PT_SKILL_ART,
    PT_SKILL_COUNT,
} pt_skill_t;

// 六个微游戏 id（07 §3）；纯逻辑/UI/经济共用同一编号。
typedef enum {
    PT_GAME_G1_HILO = 0,   // 猜大小   MIND
    PT_GAME_G2_RHYTHM,     // 节奏敲击 ART
    PT_GAME_G3_CATCH,      // 接掉落   BODY
    PT_GAME_G4_MEMORY,     // 记忆序列 MIND
    PT_GAME_G5_WALK,       // 障碍散步 BODY
    PT_GAME_G6_PREFER,     // 投其所好 无技能（亲密度）
    PT_GAME_COUNT,
} pt_game_id_t;

// 维度③饮食标签（04 §3 维度③）：阶段内成功喂食按标签计数。
typedef enum {
    PT_FOOD_TAG_MEAL = 0,   // 主食
    PT_FOOD_TAG_SWEET,      // 甜食
    PT_FOOD_TAG_SAVORY,     // 咸食
    PT_FOOD_TAG_VEG,        // 果蔬
    PT_FOOD_TAG_MEAT,       // 肉鱼
    PT_FOOD_TAG_COUNT,
} pt_food_tag_t;

#define PT_GAME_GRADE_GOOD     0
#define PT_GAME_GRADE_GREAT    1
#define PT_GAME_GRADE_PERFECT  2
// GAME_RESULT 参数打包：game_id*4 + grade（0/1/2）。
#define PT_GAME_PACK(gid, grade) (uint8_t) ((gid) * 4u + (grade))
#define PT_GAME_UNPACK_ID(p)      ((p) / 4u)
#define PT_GAME_UNPACK_GRADE(p)   ((p) % 4u)

// 单个需求的呼叫/疏忽状态机。
typedef enum {
    PT_NEED_ARMED = 0,    // 正常：跌破阈值即呼叫
    PT_NEED_NEGLECT,      // 呼叫已超时（已记小失误），继续观察是否归零
    PT_NEED_COOLDOWN,     // 已记大失误，等回升后重新武装
} pt_need_state_t;

// 意图：UI/小游戏/网络层请求执行的动作；引擎是唯一写状态者。
typedef enum {
    PT_INTENT_FEED_MEAL = 1,
    PT_INTENT_FEED_SNACK,
    PT_INTENT_FEED_BOTTLE,
    PT_INTENT_PAT,
    PT_INTENT_CLEAN,
    PT_INTENT_MEDICINE,
    PT_INTENT_LIGHTS_OFF,
    PT_INTENT_LIGHTS_ON,
    PT_INTENT_GAME_RESULT,  // 任一小游戏完成；参数=PT_GAME_PACK(game_id, grade)
    PT_INTENT_PLAY_TOY,     // 玩已拥有的玩具（L3 经济层请求）
    PT_INTENT_JOB_SHIFT,    // 上一个班；参数=本班体力消耗（pt_cfg_game_energy）
} pt_intent_kind_t;

typedef enum {
    PT_EV_NONE = 0,
    PT_EV_HATCH,
    PT_EV_STAGE_CHANGED,
    PT_EV_EVOLUTION,       // 晨间窗口真正演出的进化
    PT_EV_CALL_RAISED,
    PT_EV_CALL_RESOLVED,
    PT_EV_CALL_EXPIRED,
    PT_EV_MISTAKE_SMALL,
    PT_EV_MISTAKE_BIG,
    PT_EV_POOP,
    PT_EV_SICK,
    PT_EV_CURED,
    PT_EV_REFUSED,         // 宠物拒绝（如吃饱了）
    PT_EV_MEDICINE_FAILED,
    PT_EV_TOPUP,
    PT_EV_AGE_UP,
    PT_EV_DEATH,
    // P2-S3a 社交（08）
    PT_EV_SOC_CANDIDATE,   // 媒婆推荐新候选（a=候选槽位）
    PT_EV_SOC_WEDDING,     // 婚礼
    PT_EV_SOC_EGG_READY,   // 同住满 24h，育种蛋就绪
    PT_EV_SOC_PARENTS_LEAVE,  // 子代进入少年：父母离队（遗产/家谱揭晓）
} pt_event_kind_t;

typedef struct {
    pt_event_kind_t kind;
    int32_t at_minute;
    uint8_t a;   // 语义随事件：阶段/呼叫类型/需求/物种/年龄…
    uint8_t b;
} pt_event_t;

// 阶段判定账本（失误与顶满在进入阶段时清零）。
typedef struct {
    int16_t small;
    int16_t big;
    uint8_t full_topups;
    uint8_t happy_topups;
    uint8_t perfunctory;   // "敷衍"次数：求关注（SAD 呼叫）时塞零食/强行游戏。
                           // 不参与四档进化，只影响隐藏角色（designs 03 §管教失误）。
    uint16_t diet[PT_FOOD_TAG_COUNT];  // 维度③：阶段内成功喂食按标签计数
} pt_ledger_t;

// 需要持久化的模拟状态（存档核心，v1 先放逻辑需要的部分）。
typedef struct {
    int32_t minute;            // 已结算到的绝对分钟
    uint32_t rng_state;
    uint32_t boot_count;

    pt_stage_t stage;
    pt_species_t species;
    pt_species_t pending_species;   // 已冻结、等待晨间窗口演出的进化结果
    pt_genome_t genome;             // L2 外观基因型（8B，P2-S1；孵化时一次性掷定）
    bool waiting_evolve;            // 结果已冻结，等晨间窗口
    int32_t stage_started;
    int32_t day_id;
    uint16_t age_days;
    int32_t death_minute;           // 寿终绝对分钟；-1=未定

    uint8_t fullness;
    uint8_t happiness;
    uint8_t health;
    uint8_t energy;
    uint8_t bond;
    uint8_t weight;

    bool sleeping;
    bool lights_off;
    bool poor_sleep;
    bool sick;
    uint8_t poops;

    pt_call_kind_t active_call;
    int32_t call_started;

    // 每个需求的 FSM + 归零观察时刻（-1=未归零）
    pt_need_state_t need_state[PT_NEED_COUNT];
    int32_t zero_since[PT_NEED_COUNT];

    // 衰减/回复的定点累加器（每分钟累加速率，满 60 落地 1 点）
    int16_t acc_full;
    int16_t acc_happy;
    int16_t acc_energy;
    int16_t acc_health;

    int32_t next_poop;         // 下次排便的绝对分钟
    int32_t sick_scheduled;    // 本阶段例行病时刻（-1=无）
    int32_t poop_health_at;    // 上次卫生扣血时刻
    int32_t poop_sick_at;      // 上次满便生病掷骰时刻

    pt_ledger_t ledger;
    pt_ledger_t prev_ledger;
    bool ledger_frozen;

    // 每日配额（按清醒日界重置）
    uint8_t snacks_today;
    uint8_t cleans_bond_today;
    uint8_t game_weight_today;
    int32_t pat_window_start;
    uint8_t pats_in_window;

    // 维度②技能（P1）：终身累计 0–99；每日每类上限 9（07 §4）。
    uint8_t skill[PT_SKILL_COUNT];
    uint8_t skill_today[PT_SKILL_COUNT];
    uint8_t games_today;     // 今日完成局数（心情前 3 局全额的计数闸）
} pt_state_t;
