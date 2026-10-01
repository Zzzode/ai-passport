#include "pet_audio.h"

#include <stdint.h>

#include "bsp_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define RATE 16000
#define CHUNK 256                 // 每次喂给 I2S 的采样数
#define QUEUE_DEPTH 8
#define AMPLITUDE 8000            // 方波幅度（无 PSRAM，短音效不做复杂合成）

// 一个音符：f0→f1 在 ms 内线性滑音（f0==f1 即纯音）；freq=0 表示休止。
typedef struct {
    int16_t f0;
    int16_t f1;
    uint16_t ms;
} snd_note_t;

static const snd_note_t SND_CALL_SEQ[] = {
    { 988, 988, 90 }, { 0, 0, 60 }, { 1319, 1319, 150 },
};
static const snd_note_t SND_CONFIRM_SEQ[] = {
    { 1200, 1200, 60 },
};
static const snd_note_t SND_CANCEL_SEQ[] = {
    { 392, 392, 110 },
};
static const snd_note_t SND_EAT_SEQ[] = {
    { 330, 330, 45 }, { 0, 0, 30 }, { 330, 330, 45 },
    { 0, 0, 30 }, { 300, 300, 45 },
};
static const snd_note_t SND_HAPPY_SEQ[] = {
    { 659, 659, 90 }, { 0, 0, 40 }, { 880, 880, 90 },
    { 0, 0, 40 }, { 1175, 1175, 160 },
};
static const snd_note_t SND_SAD_SEQ[] = {
    { 622, 622, 150 }, { 0, 0, 80 }, { 466, 466, 220 },
};
static const snd_note_t SND_CLEAN_SEQ[] = {
    { 880, 294, 280 },
};
static const snd_note_t SND_EVOLVE_SEQ[] = {
    { 523, 523, 420 }, { 0, 0, 60 }, { 659, 659, 420 },
    { 0, 0, 60 }, { 784, 784, 420 }, { 0, 0, 60 },
    { 1047, 1047, 420 }, { 0, 0, 60 }, { 1319, 1319, 900 },
};
static const snd_note_t SND_SICK_SEQ[] = {
    { 311, 311, 180 }, { 0, 0, 90 }, { 247, 247, 260 },
};
static const snd_note_t SND_DEATH_SEQ[] = {
    { 523, 523, 700 }, { 0, 0, 150 }, { 440, 440, 700 },
    { 0, 0, 150 }, { 392, 392, 700 }, { 0, 0, 150 },
    { 330, 330, 1200 },
};

typedef struct {
    const snd_note_t *notes;
    uint16_t count;
} snd_track_t;

static const snd_track_t TRACKS[SND_COUNT] = {
    [SND_CALL] = { SND_CALL_SEQ, sizeof(SND_CALL_SEQ) / sizeof(snd_note_t) },
    [SND_CONFIRM] = { SND_CONFIRM_SEQ, 1 },
    [SND_CANCEL] = { SND_CANCEL_SEQ, 1 },
    [SND_EAT] = { SND_EAT_SEQ, sizeof(SND_EAT_SEQ) / sizeof(snd_note_t) },
    [SND_HAPPY] = { SND_HAPPY_SEQ, sizeof(SND_HAPPY_SEQ) / sizeof(snd_note_t) },
    [SND_SAD] = { SND_SAD_SEQ, sizeof(SND_SAD_SEQ) / sizeof(snd_note_t) },
    [SND_CLEAN] = { SND_CLEAN_SEQ, 1 },
    [SND_EVOLVE] = { SND_EVOLVE_SEQ, sizeof(SND_EVOLVE_SEQ) / sizeof(snd_note_t) },
    [SND_SICK] = { SND_SICK_SEQ, sizeof(SND_SICK_SEQ) / sizeof(snd_note_t) },
    [SND_DEATH] = { SND_DEATH_SEQ, sizeof(SND_DEATH_SEQ) / sizeof(snd_note_t) },
};

// 0=Mute / 1=Low / 2=Mid / 3=High，对应 codec 音量百分比。
static const uint8_t VOL_PERCENT[4] = { 0, 25, 55, 85 };

static QueueHandle_t s_queue;
static volatile uint8_t s_volume = 2;

// 单音合成：方波 + 首尾 5ms 线性包络去咔哒声；支持滑音。
static void play_note(const snd_note_t *note)
{
    if (note->f0 <= 0) {
        vTaskDelay(pdMS_TO_TICKS(note->ms));
        return;
    }

    int32_t total = RATE * (int32_t) note->ms / 1000;
    int32_t done = 0;
    uint32_t phase = 0;

    while (done < total) {
        int16_t pcm[CHUNK];
        int n = total - done;
        if (n > CHUNK) {
            n = CHUNK;
        }
        for (int i = 0; i < n; i += 1) {
            int32_t idx = done + i;
            int32_t freq = note->f0
                + (int32_t) (note->f1 - note->f0) * idx / (total > 0 ? total : 1);
            if (freq < 1) {
                freq = 1;
            }
            phase += (uint32_t) freq;
            int32_t sample = (phase % (uint32_t) RATE < (uint32_t) (RATE / 2))
                ? AMPLITUDE
                : -AMPLITUDE;

            // 包络：前 5ms 渐强、后 5ms 渐弱。
            int32_t attack = RATE * 5 / 1000;
            int32_t gain = 100;
            if (idx < attack) {
                gain = 100 * idx / attack;
            } else if (idx > total - attack) {
                gain = 100 * (total - idx) / attack;
                if (gain < 0) {
                    gain = 0;
                }
            }
            pcm[i] = (int16_t) (sample * gain / 100);
        }
        bsp_audio_write(pcm, (size_t) n * sizeof(int16_t));
        done += n;
    }
}

static void audio_task(void *arg)
{
    (void) arg;
    bsp_audio_set_format(RATE, 16, 1);
    bsp_audio_set_volume(VOL_PERCENT[s_volume]);

    uint8_t id;
    for (;;) {
        if (xQueueReceive(s_queue, &id, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (id >= SND_COUNT || s_volume == 0) {
            continue;
        }
        bsp_audio_set_volume(VOL_PERCENT[s_volume]);
        const snd_track_t *track = &TRACKS[id];
        for (uint16_t i = 0; i < track->count; i += 1) {
            play_note(&track->notes[i]);
        }
    }
}

void pet_audio_start(bool available)
{
    if (!available) {
        return;
    }
    s_queue = xQueueCreate(QUEUE_DEPTH, sizeof(uint8_t));
    if (s_queue == NULL) {
        return;
    }
    xTaskCreate(audio_task, "pet_audio", 4096, NULL, 3, NULL);
}

void pet_audio_play(pet_snd_t snd)
{
    if (s_queue == NULL || s_volume == 0) {
        return;
    }
    uint8_t id = (uint8_t) snd;
    (void) xQueueSend(s_queue, &id, 0);
}

void pet_audio_set_volume(uint8_t level)
{
    s_volume = level > 3 ? 2 : level;
}

uint8_t pet_audio_get_volume(void)
{
    return s_volume;
}
