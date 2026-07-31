#ifndef VITAARC_PLATFORM_TIMING_H
#define VITAARC_PLATFORM_TIMING_H

#include <stdbool.h>
#include <stdint.h>

/* Seconds requested by Cocos2dxRenderer.setAnimationInterval(float).
 * Zero/60 FPS and faster need no delay beyond vitaGL vsync. Invalid values
 * preserve the old interval. */
bool vita_frame_set_interval(float seconds);
uint32_t vita_frame_delay(uint64_t started_us, uint64_t now_us);

#endif
