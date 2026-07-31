#include <bridges/jni/jni.h>

#include "input.h"
#include "loop.h"

vita_frame_state_t vita_frame_step(void)
{
    if(!bridge_jni_cocos_is_ready()) {
        return VITA_FRAME_FAILED;
    }

    vita_input_poll();
    if(!bridge_jni_cocos_is_ready()) {
        return VITA_FRAME_FAILED;
    }

    if(vita_input_is_paused()) {
        return VITA_FRAME_PAUSED;
    }

    bridge_jni_cocos_render();
    return bridge_jni_cocos_is_ready() ? VITA_FRAME_READY : VITA_FRAME_FAILED;
}
