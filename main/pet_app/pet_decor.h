// pet_app/pet_decor.h —— 换装 / 功能家具 / 房间主题（designs/Tomagotchi/05 §7, 06 §3）。
// 纯逻辑，不依赖 ESP-IDF/LVGL；所有权、穿戴、摆放、购买扣款均可主机单测。
//
// 与生命内核的关系：装饰不直接改 pt_state。功能家具的增益（厨房正餐 +3、
// 盆栽心情回复、健身角体力/表现）由上层从摆放位算出后，通过
// pt_engine_set_room_buffs() 注入引擎（运行期参数，不进存档、不进内核状态）；
// 引擎拒绝语义在此不适用——购买/穿戴/摆放均为本地收藏操作，无动作回滚。
//
// 收藏是跨世代财产：宠物离队（死亡重开蛋）后装饰收藏保留（05 §7.1）。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pet_econ.h"
#include "pt_types.h"

// ---- 服装：4 槽（05 §7.1），单件收藏，可穿/脱 --------------------------------
typedef enum {
    PT_SLOT_HAT = 0,    // 帽子
    PT_SLOT_FACE,       // 眼镜 / 脸饰
    PT_SLOT_COLLAR,     // 围巾 / 项圈
    PT_SLOT_HELD,       // 手持物
    PT_SLOT_COUNT,
} pt_slot_t;

typedef enum {
    PT_OUTFIT_NONE = 0,
    PT_OUTFIT_CAP,       // 200G 帽子
    PT_OUTFIT_TOPHAT,    // 600G 帽子
    PT_OUTFIT_STARHAT,   // 8 贝壳券 帽子（稀有）
    PT_OUTFIT_GLASSES,   // 250G 眼镜
    PT_OUTFIT_SCARF,     // 300G 围巾
    PT_OUTFIT_BALLOON,   // 350G 手持
    PT_OUTFIT_COUNT,
} pt_outfit_t;

// ---- 功能家具：同房最多摆 3 件（05 §7.2），增益全部 ≤10% ----------------------
typedef enum {
    PT_FURN_PLANT = 0,   // 盆栽：居家心情回复 = 阶段心情衰减的 10%
    PT_FURN_GYM,         // 健身角：睡眠体力回复 +10%、游戏/打工得分 +10%
    PT_FURN_KITCHEN,     // 厨房摆件：正餐额外饱腹 +3
    PT_FURN_COUNT,
} pt_furn_t;

#define PT_DECOR_PLACED_MAX       3
#define PT_DECOR_MEAL_BONUS       3     // 厨房：正餐额外饱腹（05 §7.2）
#define PT_DECOR_PLANT_HAPPY_PCT  10    // 盆栽：心情回复占阶段衰减比例
#define PT_DECOR_GYM_ENERGY_PCT   110   // 健身角：睡眠体力回复 ×110/100
#define PT_DECOR_GYM_SCORE_PCT    10    // 健身角：小游戏/打工原始分 +10%

// ---- 房间主题：壁纸一套成套更换（05 §7.2） ------------------------------------
typedef enum {
    PT_THEME_COZY = 0,    // 默认暖绿（初始拥有）
    PT_THEME_SKY,         // 晴空蓝 400G
    PT_THEME_BERRY,       // 莓果粉 400G
    PT_THEME_STARRY,      // 星空 12 贝壳券（稀有）
    PT_THEME_COUNT,
} pt_theme_t;

typedef struct {
    const char *name;
    uint8_t slot;          // pt_slot_t
    uint16_t price;        // G 价；0=贝壳券商品
    uint8_t shells;        // 贝壳券价；0=G 商品
    uint8_t min_stage;     // 最早可购阶段（pt_stage_t）：少年起
} pt_outfit_def_t;

typedef struct {
    const char *name;
    uint16_t price;        // G 价（家具参与家具折扣日）
    uint8_t min_stage;
} pt_furn_def_t;

typedef struct {
    const char *name;
    uint16_t price;        // G 价；0=贝壳券商品
    uint8_t shells;
    uint32_t wall;         // 壁纸色（LVGL 24bit hex，UI 层使用）
} pt_theme_def_t;

typedef struct {
    uint8_t worn[PT_SLOT_COUNT];  // 各槽当前穿戴 pt_outfit_t；0=空
    uint8_t outfits;              // 服装所有权位掩码（bit = id-1）
    uint8_t furniture;            // 家具所有权位掩码（bit = pt_furn_t）
    uint8_t placed;               // 已摆放家具（furniture 的子集，≤3 件）
    uint8_t themes;               // 主题所有权位掩码（bit = pt_theme_t）
    uint8_t theme;                // 当前主题 pt_theme_t
} pt_decor_t;

void pt_decor_init(pt_decor_t *d);

const pt_outfit_def_t *pt_outfit_def(pt_outfit_t id);
const pt_furn_def_t *pt_furn_def(pt_furn_t id);
const pt_theme_def_t *pt_theme_def(pt_theme_t id);

bool pt_decor_owns_outfit(const pt_decor_t *d, pt_outfit_t id);
bool pt_decor_owns_furn(const pt_decor_t *d, pt_furn_t id);
bool pt_decor_owns_theme(const pt_decor_t *d, pt_theme_t id);
bool pt_decor_is_placed(const pt_decor_t *d, pt_furn_t id);
pt_outfit_t pt_decor_worn(const pt_decor_t *d, pt_slot_t slot);

// 折后 G 价（贝壳券商品返回 0，用 def->shells）。
uint32_t pt_decor_outfit_price_g(const pt_outfit_def_t *def_, int32_t day_id);
uint32_t pt_decor_furn_price_g(const pt_furn_def_t *def_, int32_t day_id);

typedef enum {
    PT_DECOR_OK = 0,
    PT_DECOR_BAD_ID,
    PT_DECOR_LOCKED,       // 阶段未到
    PT_DECOR_OWNED,
    PT_DECOR_NO_MONEY,
    PT_DECOR_PLACE_CAP,    // 已摆 3 件功能家具
    PT_DECOR_NOT_OWNED,    // 未拥有不能穿戴/摆放/启用
} pt_decor_rv_t;

// 购买：校验阶段/重复拥有，扣款与收藏在同一函数内完成（失败不动钱包）。
pt_decor_rv_t pt_decor_buy_outfit(pt_decor_t *d, pt_econ_t *e,
                                  pt_stage_t stage, int32_t day_id,
                                  pt_outfit_t id);
pt_decor_rv_t pt_decor_buy_furn(pt_decor_t *d, pt_econ_t *e,
                                pt_stage_t stage, int32_t day_id,
                                pt_furn_t id);
pt_decor_rv_t pt_decor_buy_theme(pt_decor_t *d, pt_econ_t *e,
                                 pt_theme_t id);

// 穿戴：再次穿戴同一件 = 脱下；非空 id 必须已拥有且槽位匹配。
pt_decor_rv_t pt_decor_equip(pt_decor_t *d, pt_outfit_t id);
// 脱下指定槽（空槽也安全）。
pt_decor_rv_t pt_decor_unequip_slot(pt_decor_t *d, pt_slot_t slot);

// 摆放 / 收起功能家具（拥有校验 + 3 件上限）。
pt_decor_rv_t pt_decor_place(pt_decor_t *d, pt_furn_t id);
pt_decor_rv_t pt_decor_unplace(pt_decor_t *d, pt_furn_t id);

// 成套更换房间主题（必须已拥有）。
pt_decor_rv_t pt_decor_set_theme(pt_decor_t *d, pt_theme_t id);

// 当前摆放位给引擎的增益（上层读取后 pt_engine_set_room_buffs）。
uint8_t pt_decor_meal_bonus(const pt_decor_t *d);
uint8_t pt_decor_happy_pct(const pt_decor_t *d);    // 0 或 PT_DECOR_PLANT_HAPPY_PCT
uint8_t pt_decor_energy_pct(const pt_decor_t *d);   // 100 或 110
uint8_t pt_decor_score_pct(const pt_decor_t *d);    // 0 或 10

// 存档校验：位掩码无杂位、摆放⊆拥有、穿戴/主题合法且已拥有。
bool pt_decor_validate(const pt_decor_t *d);
