#include <stdint.h>
#include <jni/native_call.h>
#include "test_arm_native.h"
#include "native-fixture.h"

/* The separate callee object is compiled in ARM mode; dispatch is Thumb. */
#define ENV_VALUE ((JNIEnv*)(uintptr_t)0x11223344)
#define RECEIVER_VALUE ((jobject)(uintptr_t)0x55667788)

int test_arm_native_entry(void)
{
    jni_sig_t descriptor;
    jvalue result;
#define CHECK_RETURN(name, member, code, expected, error)                                                              \
    if(!jni_sig_parse("()" code, &descriptor) ||                                                                       \
        !jni_native_call(test_arm_native_##name, ENV_VALUE, RECEIVER_VALUE, &descriptor, 0, &result) ||                \
        result.member != (expected)) {                                                                                 \
        return error;                                                                                                  \
    }
    CHECK_RETURN(boolean, z, "Z", JNI_TRUE, 1)
    CHECK_RETURN(byte, b, "B", -101, 2)
    CHECK_RETURN(char, c, "C", 0xfedc, 3)
    CHECK_RETURN(short, s, "S", -23456, 4)
    CHECK_RETURN(int, i, "I", -123456789, 5)
    CHECK_RETURN(long, j, "J", INT64_C(-0x123456789abcde), 6)
    CHECK_RETURN(float, f, "F", -3.25f, 7)
    CHECK_RETURN(double, d, "D", 12345.625, 8)
    CHECK_RETURN(object, l, "Ljava/lang/Object;", (jobject)(uintptr_t)0x76543210, 9)
    CHECK_RETURN(alignment, i, "I", 0, 10)
    if(!jni_sig_parse("()V", &descriptor) ||
        !jni_native_call(test_arm_native_void, ENV_VALUE, RECEIVER_VALUE, &descriptor, 0, &result) ||
        !test_arm_native_void_seen) {
        return 11;
    }

    jvalue args[255] = {{.j = INT64_C(0x123456789abcde)}};

    if(!jni_sig_parse("(J)J", &descriptor) ||
        !jni_native_call(test_arm_native_pair, ENV_VALUE, RECEIVER_VALUE, &descriptor, args, &result) ||
        result.j != INT64_C(-0x123456789abcde)) {
        return 12;
    }

    args[0].i = -17;
    args[1].j = INT64_C(-0x123456789abcde);
    args[2].i = 31;
    if(!jni_sig_parse("(IJI)J", &descriptor) ||
        !jni_native_call(test_arm_native_gap, ENV_VALUE, RECEIVER_VALUE, &descriptor, args, &result) ||
        result.j != INT64_C(0x123456789abcde)) {
        return 13;
    }

    const jvalue mixed[] = {{.z = JNI_TRUE},
        {.b = -101},
        {.c = 0xfedc},
        {.s = -23456},
        {.i = -123456789},
        {.j = INT64_C(-0x123456789abcde)},
        {.f = -3.25f},
        {.d = 12345.625},
        {.l = (jobject)(uintptr_t)0x1234},
        {.l = (jobject)(uintptr_t)0x5678}};

    if(!jni_sig_parse("(ZBCSIJFDLjava/lang/Object;[I)D", &descriptor) ||
        !jni_native_call(test_arm_native_mixed, ENV_VALUE, RECEIVER_VALUE, &descriptor, mixed, &result) ||
        result.d != -12345.625) {
        return 14;
    }

    char signature[260];
    signature[0] = '(';
    for(unsigned index = 0; index < 255; ++index) {
        signature[index + 1] = 'I';
        args[index].i = (jint)index * 7 - 300;
    }

    signature[256] = ')';
    signature[257] = 'I';
    signature[258] = 0;
    if(!jni_sig_parse(signature, &descriptor) ||
        !jni_native_call(test_native_255, ENV_VALUE, RECEIVER_VALUE, &descriptor, args, &result) ||
        result.i != 123456789) {
        return 15;
    }

    for(unsigned index = 0; index < 127; ++index) {
        signature[index + 1] = 'J';
        args[index].j = INT64_C(0x1234567800000000) + index * 37;
    }

    args[127].i = -123;
    signature[128] = 'I';
    signature[129] = ')';
    signature[130] = 'J';
    signature[131] = 0;
    if(!jni_sig_parse(signature, &descriptor) ||
        !jni_native_call(test_native_127_wide, ENV_VALUE, RECEIVER_VALUE, &descriptor, args, &result) ||
        result.j != INT64_C(0x123456789ab)) {
        return 16;
    }

    return 0;
}
