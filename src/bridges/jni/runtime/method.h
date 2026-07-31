#ifndef VITAARC_JNI_RUNTIME_METHOD_H
#define VITAARC_JNI_RUNTIME_METHOD_H

#include "object.h"

bool jni_sig_check(JNIEnv* env, const char* name, const char* signature);
bool jni_method_is(jni_obj_t* method, const char* name, const char* signature);
bool jni_owner_is(JNIEnv* env, jni_obj_t* method, const char* class_name);

#endif
