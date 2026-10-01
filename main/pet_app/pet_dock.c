#include "pet_dock.h"

#include <stddef.h>

// 图标按解锁顺序排列，索引即固定位置（designs 10 §3）。
static const pet_icon_t DOCK_ORDER[] = {
    PET_ICON_FEED,
    PET_ICON_CLEAN,
    PET_ICON_STATUS,
    PET_ICON_LIGHTS,
    PET_ICON_MED,
    PET_ICON_GAME,
    PET_ICON_PAT,
    PET_ICON_SHOP,
    PET_ICON_JOB,
    PET_ICON_MATE,
    PET_ICON_DEX,
    PET_ICON_SETTINGS,
};
#define DOCK_ORDER_COUNT (sizeof(DOCK_ORDER) / sizeof(DOCK_ORDER[0]))

// 图标的解锁阶段：低于该阶段不出现。
static pt_stage_t icon_min_stage(pet_icon_t icon)
{
    switch (icon) {
    case PET_ICON_FEED:
    case PET_ICON_CLEAN:
    case PET_ICON_STATUS:
    case PET_ICON_DEX:
    case PET_ICON_SETTINGS:
        return PT_STAGE_EGG;
    case PET_ICON_LIGHTS:
        return PT_STAGE_BABY;
    case PET_ICON_GAME:
    case PET_ICON_PAT:
    case PET_ICON_SHOP:
        return PT_STAGE_CHILD;
    case PET_ICON_JOB:
    case PET_ICON_MATE:
        return PT_STAGE_ADULT;
    case PET_ICON_MED:
        return PT_STAGE_COUNT;   // 由 med_unlocked 单独放行
    default:
        return PT_STAGE_COUNT;
    }
}

void pet_dock_build(pet_dock_t *dock, pt_stage_t stage, bool med_unlocked)
{
    dock->count = 0;
    dock->selected = 0;
    for (size_t i = 0; i < DOCK_ORDER_COUNT && dock->count < PET_DOCK_MAX; i += 1) {
        pet_icon_t icon = DOCK_ORDER[i];
        bool unlocked = (icon == PET_ICON_MED)
            ? med_unlocked
            : (stage >= icon_min_stage(icon) && stage != PT_STAGE_DEAD);
        if (unlocked) {
            dock->icons[dock->count] = icon;
            dock->count += 1;
        }
    }
    if (dock->count == 0) {
        dock->icons[0] = PET_ICON_STATUS;
        dock->count = 1;
    }
    if (dock->selected >= dock->count) {
        dock->selected = 0;
    }
}

void pet_dock_move(pet_dock_t *dock, int delta)
{
    if (dock->count == 0) {
        return;
    }
    int next = (int) dock->selected + delta;
    int count = (int) dock->count;
    next %= count;
    if (next < 0) {
        next += count;
    }
    dock->selected = (uint8_t) next;
}

pet_icon_t pet_dock_selected(const pet_dock_t *dock)
{
    return dock->count == 0 ? PET_ICON_NONE : dock->icons[dock->selected];
}

bool pet_dock_focus_call(pet_dock_t *dock, pt_call_kind_t call)
{
    pet_icon_t target = PET_ICON_NONE;
    switch (call) {
    case PT_CALL_HUNGRY:
        target = PET_ICON_FEED;
        break;
    case PT_CALL_SAD:
        target = PET_ICON_PAT;
        break;
    case PT_CALL_LIGHTS:
        target = PET_ICON_LIGHTS;
        break;
    case PT_CALL_NONE:
    default:
        return false;
    }
    for (uint8_t i = 0; i < dock->count; i += 1) {
        if (dock->icons[i] == target) {
            dock->selected = i;
            return true;
        }
    }
    return false;
}

bool pet_dock_icon_enabled(const pet_dock_t *dock, pet_icon_t icon)
{
    (void) dock;
    // 蛋期不能喂食/游戏/摸头（蛋还没有这些交互）；其余情况图标出现即可用。
    return icon != PET_ICON_NONE;
}
