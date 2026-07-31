#include <stdint.h>
#include <jni/aosp/jni.h>
#include "test_arm_native.h"

#define ENV_VALUE ((JNIEnv*)(uintptr_t)0x11223344)
#define RECEIVER_VALUE ((jobject)(uintptr_t)0x55667788)
#define CHECK_RECEIVER                                                                                                 \
    if(env != ENV_VALUE || receiver != RECEIVER_VALUE) {                                                               \
        return 0;                                                                                                      \
    }
#define RETURN_SCALAR(name, type, value)                                                                               \
    type VITAARC_SOFTFP test_arm_native_##name(JNIEnv* env, jobject receiver)                                          \
    {                                                                                                                  \
        CHECK_RECEIVER return value;                                                                                   \
    }

RETURN_SCALAR(boolean, jboolean, JNI_TRUE)
RETURN_SCALAR(byte, jbyte, -101)
RETURN_SCALAR(char, jchar, 0xfedc)
RETURN_SCALAR(short, jshort, -23456)
RETURN_SCALAR(int, jint, -123456789)
RETURN_SCALAR(long, jlong, INT64_C(-0x123456789abcde))
RETURN_SCALAR(float, jfloat, -3.25f)
RETURN_SCALAR(double, jdouble, 12345.625)
RETURN_SCALAR(object, jobject, (jobject)(uintptr_t)0x76543210)

volatile unsigned test_arm_native_void_seen;
void test_arm_native_void(JNIEnv* env, jobject receiver)
{
    test_arm_native_void_seen = env == ENV_VALUE && receiver == RECEIVER_VALUE;
}

__attribute__((naked)) jint test_arm_native_alignment(JNIEnv* env, jobject receiver)
{
    __asm__("mov r0, sp\n and r0, r0, #7\n bx lr");
}

jlong VITAARC_SOFTFP test_arm_native_pair(JNIEnv* env, jobject receiver, jlong value)
{
    CHECK_RECEIVER
    return value == INT64_C(0x123456789abcde) ? -value : 0;
}

jlong VITAARC_SOFTFP test_arm_native_gap(JNIEnv* env, jobject receiver, jint first, jlong value, jint last)
{
    CHECK_RECEIVER
    return first == -17 && last == 31 && value == INT64_C(-0x123456789abcde) ? -value : 0;
}

jdouble VITAARC_SOFTFP test_arm_native_mixed(JNIEnv* env, jobject receiver, jboolean z, jbyte b, jchar c, jshort s,
    jint i, jlong j, jfloat f, jdouble d, jobject object, jobject array)
{
    CHECK_RECEIVER
    if(z != JNI_TRUE || b != -101 || c != 0xfedc || s != -23456 || i != -123456789 || j != INT64_C(-0x123456789abcde) ||
        f != -3.25f || d != 12345.625 || object != (jobject)(uintptr_t)0x1234 || array != (jobject)(uintptr_t)0x5678) {
        return 0;
    }

    return -d;
}
