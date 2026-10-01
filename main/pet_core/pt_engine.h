// pet_core/pt_engine.h —— 唯一模拟入口：新生命初始化、意图处理、按分钟推进。
// 规则：所有状态变更都经过这里；在线逐分钟与离线大跨度回放走同一条路径，
// 结果只取决于存档状态与绝对分钟，不取决于推进速度。
#pragma once

#include "pt_events.h"
#include "pt_types.h"

// 新开一颗蛋：seed 决定排便抖动、例行病、掷骰等未来事件的确定性结果。
void pt_state_new(pt_state_t *s, uint32_t seed, int32_t start_minute);

// 处理一次玩家/小游戏意图（喂食、安抚、清理、喂药、关灯、游戏结果）。
void pt_handle_intent(pt_state_t *s, pt_events_t *q, pt_intent_kind_t kind,
                      uint8_t param);

// 推进到绝对分钟 target（含离线结算与 48h 封顶的温柔处理）。
void pt_advance_to(pt_state_t *s, pt_events_t *q, int32_t target_minute);

// 只读辅助：当前需求的呼叫是否仍在窗口内等。
bool pt_needs_call_active(const pt_state_t *s);

// 清醒日 id（06:00 日界）：与引擎内部 day_id 同口径，供社交等模块复用。
int32_t pt_day_index(int32_t minute);

// P2-S3b 育儿期父母同住增益（夜间衰减 ×90%、呼叫窗口 +15 分钟，08 §4）。
// 与房间摆件同为运行期参数、不属于 pt_state、不进主存档；社交状态载入/
// 世代交替时由 app 推送，主机测试用例间显式关闭。
void pt_engine_set_family_care(bool active);
bool pt_engine_family_care_active(void);

// P2-S3b 世代交替：用育种好的基因组开启子代新蛋（其余初始化同 pt_state_new）。
void pt_state_new_egg_genome(pt_state_t *s, uint32_t seed, int32_t start_minute,
                             const pt_genome_t *g);

// 房间环境增益（S4 功能家具，designs 05 §7.2）：运行期参数，不属于 pt_state、
// 不进存档；上层在开机载入后与家具摆放变化时按当前摆放位推送。
//   meal_bonus：每顿正餐额外饱腹点（厨房摆件，0=无）；
//   happy_pct ：居家清醒时心情回复占该阶段心情衰减的百分比（盆栽，0=无，≤10）；
//   energy_pct：睡眠体力回复百分比（健身角，100=无增益，110=+10%）。
// 同一进程内静态保持；主机测试用例间需显式复位为 (0,0,100)。
void pt_engine_set_room_buffs(uint8_t meal_bonus, uint8_t happy_pct,
                              uint8_t energy_pct);
