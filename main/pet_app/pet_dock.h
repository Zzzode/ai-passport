// pet_app/pet_dock.h —— 图标坞纯逻辑：图标按阶段生长、光标循环、呼叫联动。
// 只产出"当前可用的图标 id 列表 + 选中项"，具体绘制留给 LVGL 层。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pt_types.h"

typedef enum {
    PET_ICON_NONE = 0,
    PET_ICON_FEED,     // 喂食（正餐/零食/奶瓶，按阶段给不同弹层）
    PET_ICON_CLEAN,    // 厕所
    PET_ICON_STATUS,   // 状态
    PET_ICON_LIGHTS,   // 灯（婴儿期第一次睡前出现）
    PET_ICON_MED,      // 药（首次生病时教学点亮）
    PET_ICON_GAME,     // 游戏（幼儿期）
    PET_ICON_PAT,      // 摸头/互动（幼儿期）
    PET_ICON_SHOP,     // 商店（幼儿期，P1 经济）
    PET_ICON_JOB,      // 打工（成年期，P1 职业）
    PET_ICON_MATE,     // 婚介/家庭（成年期，P2 社交）
    PET_ICON_DEX,      // 图鉴（物种/部件收藏，P2-S4，常驻）
    PET_ICON_SETTINGS, // 设置（常驻，最右）
    PET_ICON_COUNT,
} pet_icon_t;

#define PET_DOCK_MAX 12

typedef struct {
    pet_icon_t icons[PET_DOCK_MAX];
    uint8_t count;
    uint8_t selected;
} pet_dock_t;

// 依据阶段构建图标坞：解锁的图标固定顺序出现，不随阶段重排（保护肌肉记忆）。
void pet_dock_build(pet_dock_t *dock, pt_stage_t stage, bool med_unlocked);

// 光标左/右循环移动（UP=左移、DOWN=右移）。
void pet_dock_move(pet_dock_t *dock, int delta);

// 当前选中的图标。
pet_icon_t pet_dock_selected(const pet_dock_t *dock);

// 呼叫把光标吸到对应图标（无呼叫返回 false，不改动）。
bool pet_dock_focus_call(pet_dock_t *dock, pt_call_kind_t call);

// 该图标在当前阶段是否可选（喂食在蛋期不可选等）。
bool pet_dock_icon_enabled(const pet_dock_t *dock, pet_icon_t icon);
