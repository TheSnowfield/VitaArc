#ifndef __BRIDGES_JNI_NATIVE_CALL_H
#define __BRIDGES_JNI_NATIVE_CALL_H

#include "aosp/jni.h"
#include "descriptor.h"

/* Native callbacks use Android base PCS on Vita and the host ABI in tests.
 * The caller owns reference validation, rooting, local frames and locking. */
bool jni_native_call(
    void* proc, JNIEnv* env, jobject receiver, const jni_sig_t* sig, const jvalue* arguments, jvalue* result);

#endif
