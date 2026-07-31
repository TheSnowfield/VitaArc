#ifndef __BRIDGES_JNI_COCOS_H
#define __BRIDGES_JNI_COCOS_H

#include <stdbool.h>
#include "aosp/jni.h"

bool jni_cocos_dispatch(
    JNIEnv* env, const char* class_name, const char* name, const char* signature, const jvalue* args, jvalue* result);
jobject jni_cocos_get_context(void);
jobject jni_cocos_get_asset_manager(void);

#endif /* __BRIDGES_JNI_COCOS_H */
