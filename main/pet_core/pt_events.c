#include "pt_events.h"

#include <string.h>

void pt_events_init(pt_events_t *q)
{
    memset(q, 0, sizeof(*q));
}

void pt_events_clear(pt_events_t *q)
{
    q->head = 0;
    q->count = 0;
}

bool pt_events_empty(const pt_events_t *q)
{
    return q->count == 0;
}

uint8_t pt_events_count(const pt_events_t *q)
{
    return q->count;
}

bool pt_events_push(pt_events_t *q, pt_event_kind_t kind, int32_t minute,
                    uint8_t a, uint8_t b)
{
    if (q->count >= PT_EVENTS_CAP) {
        // 满队：丢弃最旧事件（环形推进），为关键事件腾位。
        q->head = (uint8_t) ((q->head + 1) % PT_EVENTS_CAP);
        q->count -= 1;
    }
    uint8_t tail = (uint8_t) ((q->head + q->count) % PT_EVENTS_CAP);
    q->buf[tail].kind = kind;
    q->buf[tail].at_minute = minute;
    q->buf[tail].a = a;
    q->buf[tail].b = b;
    q->count += 1;
    return true;
}

bool pt_events_take(pt_events_t *q, pt_event_t *out)
{
    if (q->count == 0) {
        return false;
    }
    *out = q->buf[q->head];
    q->head = (uint8_t) ((q->head + 1) % PT_EVENTS_CAP);
    q->count -= 1;
    return true;
}
