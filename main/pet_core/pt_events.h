// pet_core/pt_events.h —— 有界环形事件队列（引擎产事件，UI/音频/存档消费）。
#pragma once

#include "pt_config.h"
#include "pt_types.h"

typedef struct {
    pt_event_t buf[PT_EVENTS_CAP];
    uint8_t head;
    uint8_t count;
} pt_events_t;

void pt_events_init(pt_events_t *q);
void pt_events_clear(pt_events_t *q);
bool pt_events_empty(const pt_events_t *q);
uint8_t pt_events_count(const pt_events_t *q);

// 入队；队列满时丢弃最旧事件并返回 false（关键事件不应遇到此情况，见引擎容量设计）。
bool pt_events_push(pt_events_t *q, pt_event_kind_t kind, int32_t minute,
                    uint8_t a, uint8_t b);

// 按 FIFO 取出一个事件；没有事件返回 false。
bool pt_events_take(pt_events_t *q, pt_event_t *out);
