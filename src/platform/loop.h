#ifndef __PLATFORM_LOOP_H
#define __PLATFORM_LOOP_H

typedef enum vita_frame_state { VITA_FRAME_READY, VITA_FRAME_PAUSED, VITA_FRAME_FAILED } vita_frame_state_t;

vita_frame_state_t vita_frame_step(void);

#endif /* __PLATFORM_LOOP_H */
