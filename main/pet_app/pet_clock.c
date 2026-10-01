#include "pet_clock.h"

#include "esp_timer.h"

static int64_t s_anchor_us;
static int32_t s_anchor_minute;

void pet_clock_anchor(int32_t game_minute)
{
    s_anchor_us = esp_timer_get_time();
    s_anchor_minute = game_minute;
}

int32_t pet_clock_now_minute(void)
{
    int64_t elapsed_min = (esp_timer_get_time() - s_anchor_us) / (60LL * 1000000LL);
    return s_anchor_minute + (int32_t) elapsed_min;
}

int64_t pet_clock_uptime_ms(void)
{
    return esp_timer_get_time() / 1000LL;
}
