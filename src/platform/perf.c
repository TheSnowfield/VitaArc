#include <logcat/logcat.h>

#include "perf.h"

#define PERF_INTERVAL UINT64_C(30000000)
#define FRAME_BUDGET 16667u

typedef struct {
    uint64_t render;
    uint64_t present;
    uint32_t frames;
    uint32_t peak_render;
    uint32_t peak_present;
    uint32_t slow;
} frame_times_t;

static frame_times_t __times;
static uint64_t __next;

void vita_perf_frame(uint64_t started, uint64_t rendered, uint64_t presented)
{
    if(rendered < started || presented < rendered) {
        return;
    }

    uint64_t render = rendered - started;
    uint64_t present = presented - rendered;
    __times.render += render;
    __times.present += present;
    ++__times.frames;
    if(render > __times.peak_render) {
        __times.peak_render = render > UINT32_MAX ? UINT32_MAX : (uint32_t)render;
    }

    if(present > __times.peak_present) {
        __times.peak_present = present > UINT32_MAX ? UINT32_MAX : (uint32_t)present;
    }

    if(render > FRAME_BUDGET) {
        ++__times.slow;
    }

    if(!__next) {
        __next = presented + PERF_INTERVAL;
    }

    if(presented < __next) {
        return;
    }

    /* Presentation includes GPU/display waits. These are wall times, not CPU
     * utilization; compare render and present separately when diagnosing stalls. */
    log_i(TAG,
        "Frame times: count=%u render avg=%u peak=%u present avg=%u peak=%u slow-render=%u us",
        __times.frames,
        (unsigned)(__times.render / __times.frames),
        __times.peak_render,
        (unsigned)(__times.present / __times.frames),
        __times.peak_present,
        __times.slow);

    __times = (frame_times_t){0};
    __next = presented + PERF_INTERVAL;
}
