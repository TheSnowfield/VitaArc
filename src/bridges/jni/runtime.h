#ifndef __BRIDGES_JNI_RUNTIME_H
#define __BRIDGES_JNI_RUNTIME_H

#include "aosp/jni.h"

JNIEnv *jni_runtime_get_env(void);
JavaVM *jni_runtime_get_java_vm(void);

jstring jni_runtime_new_string_utf(const char *text);
const char *jni_runtime_get_string_utf(jstring string);

#endif /* __BRIDGES_JNI_RUNTIME_H */
