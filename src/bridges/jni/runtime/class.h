#ifndef VITAARC_JNI_RUNTIME_CLASS_H
#define VITAARC_JNI_RUNTIME_CLASS_H

#include "object.h"

jni_obj_t* jni_member(jclass clazz, const char* name, const char* signature, jni_kind_t kind, jboolean is_static);
bool jni_named(const jni_obj_t* object, const char* name, const char* signature);
jni_obj_t* jni_super(JNIEnv* env, jni_obj_t* clazz);
bool jni_assignable(JNIEnv* env, jni_obj_t* source, jni_obj_t* target);
jni_obj_t* jni_classof(JNIEnv* env, jni_obj_t* object);
bool jni_instanceof(JNIEnv* env, jobject reference, jni_obj_t* clazz);
jni_obj_t* jni_obj_new(JNIEnv* env, jclass reference);

#endif
