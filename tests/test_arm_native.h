#ifndef __TESTS_ARM_NATIVE_H
#define __TESTS_ARM_NATIVE_H

#include <jni/aosp/jni.h>

#define DECLARE_NATIVE(name, type) type VITAARC_SOFTFP test_arm_native_##name(JNIEnv* env, jobject receiver)
DECLARE_NATIVE(boolean, jboolean);
DECLARE_NATIVE(byte, jbyte);
DECLARE_NATIVE(char, jchar);
DECLARE_NATIVE(short, jshort);
DECLARE_NATIVE(int, jint);
DECLARE_NATIVE(long, jlong);
DECLARE_NATIVE(float, jfloat);
DECLARE_NATIVE(double, jdouble);
DECLARE_NATIVE(object, jobject);
#undef DECLARE_NATIVE
void test_arm_native_void(JNIEnv* env, jobject receiver);
jint test_arm_native_alignment(JNIEnv* env, jobject receiver);
jlong VITAARC_SOFTFP test_arm_native_pair(JNIEnv* env, jobject receiver, jlong value);
jlong VITAARC_SOFTFP test_arm_native_gap(JNIEnv* env, jobject receiver, jint first, jlong value, jint last);
jdouble VITAARC_SOFTFP test_arm_native_mixed(JNIEnv* env, jobject receiver, jboolean z, jbyte b, jchar c, jshort s,
    jint i, jlong j, jfloat f, jdouble d, jobject object, jobject array);
extern volatile unsigned test_arm_native_void_seen;

#endif
