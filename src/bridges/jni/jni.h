#ifndef __BRIDGES_JNI_JNI_H
#define __BRIDGES_JNI_JNI_H

#include <stddef.h>
#include <dynalib.h>

void bridge_call_jni_main(dynalib_t *library);

void bridge_jni_cocos_native_init(dynalib_t *library,
                                  uint32_t screen_width,
                                  uint32_t screen_height);

void bridge_jni_cocos_init_app(dynalib_t *library);

void bridge_jni_cocos_set_app_version(dynalib_t *library);

void bridge_jni_cocos_set_device_id(dynalib_t *library);

#endif /* __BRIDGES_JNI_JNI_H */
