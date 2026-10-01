// pet_app/pet_clock.h —— 游戏分钟时钟（designs 11 §3）。
// 引擎只认"绝对分钟"，由本层把板级时间换算成分钟喂给 pt_advance_to：
//
//   game_minute = anchored_minute + (uptime_now - anchor_uptime) / 60s
//
// P0 硬件没有电池备份 RTC，也不联网对时：冷启动（断电/模拟器重载）无法知道
// 关机期间流逝的真实时间，离线差值按 0 处理——存档 minute 即恢复点。
// 设备保持供电（含模拟器标签页常开）时，时间随 uptime 连续推进。
// 后续接入 SNTP 或电池 RTC 时，只需替换本层的 uptime 源。
#pragma once

#include <stdint.h>

// 以当前 uptime 锚定游戏分钟（开机载入存档 / 新蛋后调用）。
void pet_clock_anchor(int32_t game_minute);

// 当前游戏绝对分钟。
int32_t pet_clock_now_minute(void);

// 单调 uptime（毫秒），供 UI 动画/消息计时与小游戏掷种子使用。
int64_t pet_clock_uptime_ms(void);
