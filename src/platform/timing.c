#include <math.h>
#include <stdatomic.h>

#include "timing.h"

static atomic_uint __interval_us = 16667;

bool vita_frame_set_interval(float seconds)
{
    double micros = (double)seconds * 1000000.0;
    if(!isfinite(micros) || micros < 0 || micros > UINT32_MAX) {
        return false;
    }

    uint32_t interval = (uint32_t)(micros + 0.5);
    if(seconds > 0 && !interval) {
        interval = 1;
    }

    atomic_store_explicit(&__interval_us, interval, memory_order_relaxed);
    return true;
}

uint32_t vita_frame_delay(uint64_t started_us, uint64_t now_us)
{
    uint32_t interval = atomic_load_explicit(&__interval_us, memory_order_relaxed);
    // Like Cocos's default Java renderer, leave 60 FPS and faster to vsync.
    // A second short sleep can miss a vblank and reduce the default frame rate.
    if(interval <= 16667 || now_us < started_us || now_us - started_us >= interval) {
        return 0;
    }

    return interval - (uint32_t)(now_us - started_us);
}
