#ifndef __BRIDGES_JNI_JNI_H
#define __BRIDGES_JNI_JNI_H

#include <stdbool.h>
#include <stdint.h>

#include <dynalib.h>

bool bridge_jni_cocos_start(dynalib_t* library, uint32_t screen_width, uint32_t screen_height);
void bridge_jni_cocos_stop(void);
bool bridge_jni_cocos_is_ready(void);
bool bridge_jni_cocos_complete_expansion(void);
bool bridge_jni_cocos_notify_startup(void);
bool bridge_jni_cocos_publish_bitmap(int width, int height, const uint8_t* rgba);
void bridge_jni_cocos_render(void);
void bridge_jni_cocos_pause(void);
void bridge_jni_cocos_resume(void);
void bridge_jni_cocos_touch_begin(int32_t id, float x, float y);
void bridge_jni_cocos_touch_end(int32_t id, float x, float y);
void bridge_jni_cocos_touch_move(const int32_t* ids, const float* xs, const float* ys, uint32_t count);
bool bridge_jni_cocos_key_event(int32_t key_code, bool pressed);

#endif /* __BRIDGES_JNI_JNI_H */
