// pet_app/pet_app.h —— PasPet 应用装配层（designs 11 §2 任务模型）。
// 引擎任务是唯一写 pt_state 的地方；UI 通过快照读状态、通过意图/命令改状态。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pet_input.h"
#include "pet_decor.h"
#include "pet_econ.h"
#include "pet_jobs.h"
#include "pt_dex.h"
#include "pt_social.h"
#include "pt_types.h"

// 音频设备是否可用（bsp_audio_init 的结果）由 main 告知；不可用时静默降级。
void pet_app_start(bool audio_available);

// 输入任务调用：把板级按键事件交给 UI（内部自行获取 LVGL 锁）。
void pet_app_button(pet_btn_t btn, pet_ev_t ev);

// UI 向引擎投递一个玩家意图（非阻塞，队列满返回 false）。
bool pet_app_send_intent(pt_intent_kind_t kind, uint8_t param);

// 死亡纪念页确认：开一颗新蛋（引擎任务内执行）。
void pet_app_new_game(void);

// UI 拷贝当前状态快照（短互斥；失败返回 false）。
bool pet_app_snapshot(pt_state_t *out);

// UI 取出引擎产出的事件（无事件返回 false）。
bool pet_app_take_event(pt_event_t *out);

// 设置：音量档 0..3 / 药图标是否已点亮 / 免打扰时段开关。
uint8_t pet_app_volume(void);
void pet_app_set_volume(uint8_t level);
bool pet_app_med_unlocked(void);
bool pet_app_quiet(void);
void pet_app_set_quiet(bool enabled);

// ---- L3 经济（P1）：钱包/签到/商店/背包的只读快照与动作投递，全部非阻塞 ----
bool pet_app_econ_snapshot(pt_econ_t *out);

// S5：取出引擎任务的一次性钱包提示（月初津贴等）；无提示返回 false。
bool pet_app_take_notice(char *out, uint8_t cap);
void pet_app_econ_checkin(void);
void pet_app_econ_buy(pt_item_t item);
void pet_app_econ_eat(pt_item_t item);
void pet_app_econ_play(pt_item_t item);

// 小游戏 G1–G6 完成结算（评级 0/1/2 = GOOD/GREAT/PERFECT）。
void pet_app_game_result(uint8_t game_id, uint8_t grade);

// ---- L3 职业（P1-S3）：成年打工，非阻塞 ----
// 上一个班（job=pt_job_id_t，grade=0/1/2）；班次/体力/阶段闸在引擎任务内裁决。
void pet_app_job_work(uint8_t job, uint8_t grade);
// 在职介所切换职业（门槛 + 30 日限频在引擎任务内裁决）。
void pet_app_job_switch(uint8_t job);
bool pet_app_jobs_snapshot(pt_jobs_t *out);

// ---- L3 换装 / 家具 / 主题（P1-S4）：本地收藏操作，非阻塞 ----
bool pet_app_decor_snapshot(pt_decor_t *out);
// kind: 0=服装 1=家具 2=主题；id 为对应枚举值。
void pet_app_decor_buy(uint8_t kind, uint8_t id);
void pet_app_decor_equip(uint8_t outfit);       // 再点同一件即脱下
void pet_app_decor_unequip(uint8_t slot);
void pet_app_decor_place(uint8_t furniture);
void pet_app_decor_unplace(uint8_t furniture);
void pet_app_decor_set_theme(uint8_t theme);

// ---- P2-S3a 社交（designs 08）：候选/关系/求婚快照与动作，非阻塞 ----
bool pet_app_social_snapshot(pt_social_t *out);
void pet_app_soc_greet(uint8_t candidate_index);
void pet_app_soc_gift(uint8_t candidate_index);
// ring：0=普通戒 500G，1=钻戒 5000G（扣款在引擎任务内裁决）。
void pet_app_soc_propose(uint8_t candidate_index, uint8_t ring);

// S3b：EGG_READY 面板确认迎接新蛋（世代交替）。
void pet_app_soc_start_egg(void);
// 取走上一个社交动作的裁决（pt_soc_rv_t），用于浮条；无结果返回 false。
bool pet_app_take_soc_result(uint8_t *rv);

// ---- P2-S4 图鉴（04 §8.3 / 05 §5.2/§8）：物种/部件/徽章页只读快照 ----
bool pet_app_dex_snapshot(pt_dex_t *out);
