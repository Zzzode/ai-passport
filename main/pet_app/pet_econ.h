// pet_app/pet_econ.h —— L3 经济底座（纯逻辑，可主机单测，designs/Tomagotchi/06）。
// 钱包（G/贝壳券）、每日签到、商品目录、库存、每日三刷货架。
//
// 架构红线（06 §4 / 11 §2）：经济层永远不直接改生命状态。吃东西/玩玩具只做库存
// 校验与扣减，再由上层把 pt_intent_kind_t 投给 pet_core 引擎执行；引擎拒绝时
// 上层用 pt_econ_add / pt_econ_refund_toy_use 把库存/次数补回。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pt_types.h"

#define PT_ECON_COIN_CAP          99999u   // G 币余额上限（06 §1）
#define PT_ECON_SHELL_CAP         999u     // 贝壳券上限
#define PT_ECON_CHECKIN_COINS     20       // 每日签到
#define PT_ECON_CHECKIN_DAY7      100      // 连续第 7 天
#define PT_ECON_CHECKIN_DAY7_SHELL 1

// S5：月初津贴与软税（06 §2/§5）。
#define PT_ECON_STIPEND_DAY       5        // 每月 5 号（月历日，day_id%30+1）
#define PT_ECON_STIPEND_COINS     500
#define PT_ECON_SOFT_TAX_AT       10000u   // 持有 >10,000G 后
#define PT_ECON_SOFT_TAX_PCT      20       // 工资/游戏收益 −20%
#define PT_ECON_INV_SLOTS         20       // 食物库存格上限（06 §4）
#define PT_ECON_SHOP_RANDOM       6        // 随机栏位数（06 §3.1）
#define PT_ECON_TOY_USES_DAY      3        // 每个玩具每日次数

// 小游戏 G 币产出（07 §4 / 12 §9）：前 5 局按评级，之后保底 5G。
#define PT_ECON_GAME_PAY_GOOD     10
#define PT_ECON_GAME_PAY_GREAT    20
#define PT_ECON_GAME_PAY_PERFECT  30
#define PT_ECON_GAME_FULL_N       5
#define PT_ECON_GAME_FLOOR        5

// 货架刷新时刻（本地时钟，不联网）：08:00 / 14:00 / 20:00。
#define PT_ECON_REFRESH_1_MIN     (8 * 60)
#define PT_ECON_REFRESH_2_MIN     (14 * 60)
#define PT_ECON_REFRESH_3_MIN     (20 * 60)

typedef enum {
    PT_ITEM_NONE = 0,
    // 常备食物（幼儿期常驻）
    PT_ITEM_RICEBALL,    // 5G 正餐
    PT_ITEM_BISCUIT,     // 8G 零食
    // 随机食物池（少年期起）
    PT_ITEM_BENTO,       // 12G 正餐
    PT_ITEM_PUDDING,     // 14G 零食
    PT_ITEM_CAKE,        // 20G 零食
    PT_ITEM_CREPE,       // 25G 特色食物（DIET，甜食）
    PT_ITEM_WATERMELON,  // 30G 特色食物（DIET，果蔬/正餐类）
    // 玩具（幼儿期 3 件常驻，少年期起也可被随机刷新）
    PT_ITEM_BALL,        // 100G
    PT_ITEM_MUSICBOX,    // 300G
    PT_ITEM_BUBBLES,     // 500G
    PT_ITEM_COUNT,
} pt_item_t;

typedef enum { PT_ITEM_KIND_FOOD = 0, PT_ITEM_KIND_TOY } pt_item_kind_t;
typedef enum { PT_FOOD_MEAL = 0, PT_FOOD_SNACK } pt_food_class_t;

typedef struct {
    const char *name;
    uint16_t price;        // v0.1 全部以 G 标价；贝壳券商品随后续期次
    uint8_t kind;          // pt_item_kind_t
    uint8_t food_class;    // pt_food_class_t（仅食物）：决定喂主食还是零食
    uint8_t food_tag;      // pt_food_tag_t（仅食物）：维度③饮食倾向计数
    bool diet;             // DIET 标签（保留旧字段：特色食物标记）
    uint8_t min_stage;     // 最早出现/可购阶段（pt_stage_t）
    bool staple;           // 常备栏：永远在售
} pt_item_def_t;

const pt_item_def_t *pt_item_def(pt_item_t id);
bool pt_item_is_toy(pt_item_t id);

// 食物 → L0 喂食意图（经济层不自己执行）。
pt_intent_kind_t pt_item_eat_intent(pt_item_t id);

typedef struct {
    uint16_t item;         // pt_item_t；PT_ITEM_NONE=空槽
    uint8_t qty;
} pt_inv_slot_t;

typedef struct {
    uint32_t coins;
    uint16_t shells;
    uint16_t streak;            // 当前连续签到天数（第 7 天发奖后归零）
    int32_t checkin_day;        // 上次签到的清醒日 id；-1=从未
    uint32_t shelf_seed;        // 每存档固定的货架派生种子
    pt_inv_slot_t inv[PT_ECON_INV_SLOTS];
    uint8_t toy_uses[PT_ITEM_COUNT];   // 各玩具今日已用次数（清醒日界清零）
    uint8_t game_pays_today;           // 今日已全额/计次发游戏奖的局数（清醒日界清零）
} pt_econ_t;

// 新存档：空钱包、空库存；shelf_seed 由上层随生命种子一起掷定。
void pt_econ_init(pt_econ_t *e, uint32_t shelf_seed);

// 清醒日界推进：重置玩具每日次数（由引擎任务在 day_id 跳变时调用）。
void pt_econ_on_day(pt_econ_t *e);

typedef struct {
    uint16_t coins_gained;
    uint16_t shells_gained;
    bool week_bonus;
    uint16_t streak_now;
} pt_checkin_result_t;

bool pt_econ_can_checkin(const pt_econ_t *e, int32_t day_id);
bool pt_econ_checkin(pt_econ_t *e, int32_t day_id, pt_checkin_result_t *out);

// 生成当前货架：常备栏在前；幼儿=常备 2 + 固定玩具 3；少年起=常备 2 + 6 随机。
// 同一会话（两次刷新之间）对同一存档结果恒定，可随时重建。返回写入条数。
uint8_t pt_econ_shop_build(const pt_econ_t *e, pt_stage_t stage,
                           int32_t minute, uint16_t *out, uint8_t cap);

typedef enum {
    PT_ECON_OK = 0,
    PT_ECON_BAD_ITEM,
    PT_ECON_LOCKED,         // 阶段未到
    PT_ECON_NOT_ON_SHELF,   // 当前货架没有
    PT_ECON_NO_MONEY,
    PT_ECON_INV_FULL,       // 没有空库存格（目前目录下不可达，随目录扩充生效）
    PT_ECON_OWNED,          // 玩具已拥有（单件收藏）
    PT_ECON_NO_STOCK,       // 库存没有该物
    PT_ECON_USES_EXHAUSTED, // 玩具今日次数用完
} pt_econ_rv_t;

pt_econ_rv_t pt_econ_buy(pt_econ_t *e, pt_stage_t stage, int32_t minute,
                         pt_item_t item);

// 吃食物：校验库存并扣减；意图随后投引擎，拒绝时用 pt_econ_add 补回。
pt_econ_rv_t pt_econ_take_food(pt_econ_t *e, pt_item_t item);

// 玩玩具：校验每日次数并预扣；引擎拒绝时用 pt_econ_refund_toy_use 回滚。
pt_econ_rv_t pt_econ_use_toy(pt_econ_t *e, pt_item_t item);
void pt_econ_refund_toy_use(pt_econ_t *e, pt_item_t item);

// 库存加回（引擎拒绝喂食时的退款补偿）；堆叠到同类槽位。
pt_econ_rv_t pt_econ_add(pt_econ_t *e, pt_item_t item, uint8_t qty);

uint8_t pt_econ_count(const pt_econ_t *e, pt_item_t item);

// 小游戏结算发 G 币：前 5 局 GOOD/GREAT/PERFECT = 10/20/30，之后 5G 保底；
// 返回实际入账数（已含余额上限钳制）。调用方负责先确认引擎接受本局。
uint16_t pt_econ_game_payout(pt_econ_t *e, uint8_t grade);

// 通用 G 币入账（打工工资等）：返回实际入账数，超出 99,999 部分截断。
uint16_t pt_econ_add_coins(pt_econ_t *e, uint16_t amount);

// 通用扣款（S4 装饰店等）：余额不足返回 false 且不动钱包。
bool pt_econ_spend_coins(pt_econ_t *e, uint32_t amount);
bool pt_econ_spend_shells(pt_econ_t *e, uint16_t amount);
void pt_econ_grant_shells(pt_econ_t *e, uint16_t amount);

// 折扣日（06 §3.1，按游戏内"月历日"day_id%30+1）：
// 每月 5/15/25 全场 8 折返回 20；10/20/30 家具 9 折返回 10（furniture=true 时）；
// 两者重叠日全场折扣优先。其余返回 0。贝壳券商品不参与。
uint8_t pt_econ_sale_pct(int32_t day_id, bool furniture);

// S5：是否为月初津贴日（每月 5 号）。
bool pt_econ_is_stipend_day(int32_t day_id);

// S5：常备货架物品的当日成交价（食物/玩具只吃全场 8 折，不吃家具 9 折）。
uint32_t pt_econ_item_price_now(uint16_t price, int32_t day_id);

// S5：劳动收入入账（工资 / 游戏奖金）：持有 >10,000G 时 −20% 软边际（06 §5），
// 再做 99,999 上限钳制；返回实际入账数。签到/津贴不走此路径。
uint16_t pt_econ_credit_income(pt_econ_t *e, uint16_t amount);
