#ifndef VITAARC_JNI_RUNTIME_SERVICES_H
#define VITAARC_JNI_RUNTIME_SERVICES_H

#include "object.h"

bool jni_service_call(JNIEnv* env, jni_obj_t* object, jni_obj_t* method, const jvalue* args, jvalue* result);

#endif
