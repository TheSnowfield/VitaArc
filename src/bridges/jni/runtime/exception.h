#ifndef VITAARC_JNI_RUNTIME_EXCEPTION_H
#define VITAARC_JNI_RUNTIME_EXCEPTION_H

#include "object.h"

void jni_raise(JNIEnv* env, const char* class_name, const char* message);

#endif
