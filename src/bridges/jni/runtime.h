#ifndef __BRIDGES_JNI_RUNTIME_H
#define __BRIDGES_JNI_RUNTIME_H

#include "aosp/jni.h"

JNIEnv* jni_runtime_get_env(void);
JavaVM* jni_runtime_get_java_vm(void);

typedef struct jni_runtime_stats {
    size_t objects;
    size_t references;
} jni_runtime_stats_t;

void jni_runtime_get_stats(jni_runtime_stats_t* stats);

jstring jni_runtime_new_string_utf(const char* text);
const char* jni_runtime_get_string_utf(jstring string);
void* jni_runtime_find_native(const char* class_name, const char* name, const char* signature);
void jni_runtime_clear_natives(void);

#endif /* __BRIDGES_JNI_RUNTIME_H */
