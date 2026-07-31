#ifndef VITAARC_JNI_RUNTIME_NATIVE_H
#define VITAARC_JNI_RUNTIME_NATIVE_H

#include "object.h"
#include "../descriptor.h"

jni_obj_t* jni_binding(jni_obj_t* clazz, const char* name, const char* signature);
jni_obj_t* jni_native_decl(JNIEnv* env, jni_obj_t* method);
bool jni_native_run(
    JNIEnv* env, jobject receiver, jni_obj_t* method, const jni_sig_t* sig, const jvalue* arguments, jvalue* result);

#endif
