#include "pt_config.h"

// 数组索引即 pt_stage_t（含 PT_STAGE_DEAD 的零值尾项）。
const uint8_t pt_cfg_dec_full[PT_STAGE_COUNT] = {
    0,    // EGG
    12,   // BABY
    7,    // CHILD
    5,    // TEEN
    4,    // ADULT
    5,    // SENIOR
    0,    // DEAD
};

const uint8_t pt_cfg_dec_happy[PT_STAGE_COUNT] = {
    0, 10, 7, 5, 4, 5, 0,
};

const uint8_t pt_cfg_dec_energy[PT_STAGE_COUNT] = {
    0, 14, 10, 8, 7, 9, 0,
};

const uint16_t pt_cfg_call_window[PT_STAGE_COUNT] = {
    0, 15, 30, 45, 60, 60, 0,
};

const uint8_t pt_cfg_bond_cap[PT_STAGE_COUNT] = {
    0, 40, 60, 80, 100, 100, 0,
};

const uint16_t pt_cfg_poop_interval[PT_STAGE_COUNT] = {
    0, 45, 120, 180, 180, 180, 0,
};

// 游戏 → 技能（07 §3）：G1/G4 MIND，G2 ART，G3/G5 BODY，G6 无技能。
const uint8_t pt_cfg_game_skill[PT_GAME_COUNT] = {
    PT_SKILL_MIND,                       // G1
    PT_SKILL_ART,                        // G2
    PT_SKILL_BODY,                       // G3
    PT_SKILL_MIND,                       // G4
    PT_SKILL_BODY,                       // G5
    (uint8_t) 0xFFu,                     // G6
};

// 体力消耗：节奏略高、记忆最低（07 §3 各游戏说明）。
const uint8_t pt_cfg_game_energy[PT_GAME_COUNT] = {
    5,    // G1
    10,   // G2
    8,    // G3
    5,    // G4
    8,    // G5
    5,    // G6
};
