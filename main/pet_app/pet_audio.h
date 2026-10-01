// pet_app/pet_audio.h —— P0 十个芯片风音效（designs 10 §7.2）。
// 所有播放都在独立音频任务里合成方波 PCM 并经 bsp_audio_write 阻塞吐出；
// 按键回调、引擎任务、LVGL 锁内只允许调用 pet_audio_play()（仅入队，不阻塞）。
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SND_CALL = 0,    // 呼叫：两声短鸣
    SND_CONFIRM,     // 菜单确认
    SND_CANCEL,      // 取消 / 拒绝
    SND_EAT,         // 进食：三连咀嚼音
    SND_HAPPY,       // 心情大涨 / 游戏胜出 / 治愈
    SND_SAD,         // 失误 / 失败
    SND_CLEAN,       // 清理：下滑音
    SND_EVOLVE,      // 进化：约 4 秒小旋律
    SND_SICK,        // 生病：沉闷两音
    SND_DEATH,       // 告别：缓慢旋律
    SND_COUNT,
} pet_snd_t;

// available=false（bsp_audio_init 失败，如模拟器未模拟 codec）时不建任务，
// play() 全部静默降级；静音状态不影响呼叫记账与图标灯。
void pet_audio_start(bool available);

// 排队播放一个音效；队列满时丢弃新请求（呼叫由 UI 图标灯持续提示）。
void pet_audio_play(pet_snd_t snd);

// 音量档：0=静音，1=低，2=中，3=高。
void pet_audio_set_volume(uint8_t level);
uint8_t pet_audio_get_volume(void);
