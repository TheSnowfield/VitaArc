#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <psp2/ctrl.h>
#include <psp2/appmgr.h>
#include <psp2/touch.h>

#include <bridges/jni/jni.h>
#include <logcat/logcat.h>

#include "input.h"

#define SCREEN_WIDTH 960.0f
#define SCREEN_HEIGHT 544.0f

typedef struct touch_t {
    bool active;
    uint8_t id;
    float x;
    float y;
} touch_t;

typedef struct keymap_t {
    uint32_t vita_button;
    int32_t android_key;
} keymap_t;

static const keymap_t __key_mappings[] = {
    {SCE_CTRL_UP, 19},
    {SCE_CTRL_DOWN, 20},
    {SCE_CTRL_LEFT, 21},
    {SCE_CTRL_RIGHT, 22},
    {SCE_CTRL_CROSS, 96},
    {SCE_CTRL_CIRCLE, 97},
    {SCE_CTRL_SQUARE, 99},
    {SCE_CTRL_TRIANGLE, 100},
    {SCE_CTRL_LTRIGGER, 102},
    {SCE_CTRL_RTRIGGER, 103},
    {SCE_CTRL_START, 108},
    {SCE_CTRL_SELECT, 109},
};

static SceTouchPanelInfo __panel_info;
static touch_t __previous_touches[SCE_TOUCH_MAX_REPORT];
static uint32_t __previous_buttons;
static bool __paused;

static void __set_paused(bool paused)
{
    if(paused == __paused) {
        return;
    }

    __paused = paused;
    if(__paused) {
        bridge_jni_cocos_pause();
        log_i(TAG, "Cocos lifecycle paused");
    } else {
        bridge_jni_cocos_resume();
        log_i(TAG, "Cocos lifecycle resumed");
    }
}

static float __scale_touch_x(int16_t x)
{
    int32_t range = __panel_info.maxDispX - __panel_info.minDispX;
    if(range <= 0) {
        return (float)x * SCREEN_WIDTH / 1920.0f;
    }

    return ((float)x - __panel_info.minDispX) * SCREEN_WIDTH / (float)range;
}

static float __scale_touch_y(int16_t y)
{
    int32_t range = __panel_info.maxDispY - __panel_info.minDispY;
    if(range <= 0) {
        return (float)y * SCREEN_HEIGHT / 1088.0f;
    }

    return ((float)y - __panel_info.minDispY) * SCREEN_HEIGHT / (float)range;
}

static int32_t __find_touch(const touch_t* points, uint8_t id)
{
    for(int32_t index = 0; index < SCE_TOUCH_MAX_REPORT; ++index) {
        if(points[index].active && points[index].id == id) {
            return index;
        }
    }

    return -1;
}

static void __poll_touch(void)
{
    SceTouchData data;
    memset(&data, 0, sizeof(data));
    if(sceTouchPeek(SCE_TOUCH_PORT_FRONT, &data, 1) <= 0) {
        return;
    }

    touch_t current[SCE_TOUCH_MAX_REPORT];
    int32_t ids[SCE_TOUCH_MAX_REPORT];
    float xs[SCE_TOUCH_MAX_REPORT];
    float ys[SCE_TOUCH_MAX_REPORT];
    memset(current, 0, sizeof(current));

    uint32_t count = data.reportNum;
    if(count > SCE_TOUCH_MAX_REPORT) {
        count = SCE_TOUCH_MAX_REPORT;
    }

    for(uint32_t index = 0; index < count; ++index) {
        current[index].active = true;
        current[index].id = data.report[index].id;
        current[index].x = __scale_touch_x(data.report[index].x);
        current[index].y = __scale_touch_y(data.report[index].y);
        ids[index] = current[index].id;
        xs[index] = current[index].x;
        ys[index] = current[index].y;

        if(__find_touch(__previous_touches, current[index].id) < 0) {
            bridge_jni_cocos_touch_begin(ids[index], xs[index], ys[index]);
        }
    }

    for(uint32_t index = 0; index < SCE_TOUCH_MAX_REPORT; ++index) {
        if(__previous_touches[index].active && __find_touch(current, __previous_touches[index].id) < 0) {
            bridge_jni_cocos_touch_end(
                __previous_touches[index].id, __previous_touches[index].x, __previous_touches[index].y);
        }
    }

    if(count > 0) {
        bridge_jni_cocos_touch_move(ids, xs, ys, count);
    }

    memcpy(__previous_touches, current, sizeof(current));
}

static void __poll_controller(void)
{
    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));
    if(sceCtrlPeekBufferPositive(0, &pad, 1) <= 0) {
        return;
    }

    bool paused = (pad.buttons & SCE_CTRL_INTERCEPTED) != 0;
    SceAppMgrAppState app_state;
    memset(&app_state, 0, sizeof(app_state));
    if(_sceAppMgrGetAppState(&app_state, sizeof(app_state), 0) == 0) {
        paused = paused || app_state.isSystemUiOverlaid;
    }

    __set_paused(paused);

    uint32_t changed = pad.buttons ^ __previous_buttons;
    for(uint32_t index = 0; index < sizeof(__key_mappings) / sizeof(__key_mappings[0]); ++index) {
        if(changed & __key_mappings[index].vita_button) {
            bridge_jni_cocos_key_event(
                __key_mappings[index].android_key, (pad.buttons & __key_mappings[index].vita_button) != 0);
        }
    }

    __previous_buttons = pad.buttons;
}

bool vita_input_initialize(void)
{
    memset(&__panel_info, 0, sizeof(__panel_info));
    memset(__previous_touches, 0, sizeof(__previous_touches));
    __previous_buttons = 0;
    __paused = false;

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    if(sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &__panel_info) < 0) {
        log_w(TAG, "Could not read front touch panel geometry");
    }

    if(sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START) < 0) {
        log_e(TAG, "Could not start front touch sampling");
        return false;
    }

    return true;
}

void vita_input_poll(void)
{
    __poll_controller();
    if(!__paused) {
        __poll_touch();
    }
}

bool vita_input_is_paused(void)
{
    return __paused;
}
