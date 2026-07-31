#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <jni/runtime.h>

static JNIEnv* __env;
static jobject __receiver;
static bool __valid_receiver(JNIEnv* env, jobject receiver)
{
    return env == __env && (*env)->IsSameObject(env, receiver, __receiver) &&
        (*env)->GetObjectRefType(env, receiver) == JNILocalRefType;
}

#define RETURN_SCALAR(name, type, value)                                                                               \
    static type VITAARC_SOFTFP __return_##name(JNIEnv* env, jobject receiver)                                          \
    {                                                                                                                  \
        return __valid_receiver(env, receiver) ? value : 0;                                                            \
    }
RETURN_SCALAR(boolean, jboolean, JNI_TRUE)
RETURN_SCALAR(byte, jbyte, -101)
RETURN_SCALAR(char, jchar, 0xfedc)
RETURN_SCALAR(short, jshort, -23456)
RETURN_SCALAR(int, jint, -123456789)
RETURN_SCALAR(long, jlong, INT64_C(-0x123456789abcde))
RETURN_SCALAR(float, jfloat, -3.25f)
RETURN_SCALAR(double, jdouble, 12345.625)

static jstring __object(JNIEnv* env, jobject receiver)
{
    if(!__valid_receiver(env, receiver) || (*env)->PushLocalFrame(env, 4) != JNI_OK) {
        return NULL;
    }

    (*env)->NewStringUTF(env, "discarded temporary");
    return (*env)->NewStringUTF(env, "ARM");
}

static jdouble VITAARC_SOFTFP __mixed(
    JNIEnv* env, jobject receiver, jint i, jfloat f, jdouble d, jlong j, jstring string, jintArray array)
{
    if(!__valid_receiver(env, receiver) || i != -17 || f != -3.25f || d != 12345.625 ||
        j != INT64_C(-0x123456789abcde) || (*env)->GetStringLength(env, string) != 3 ||
        (*env)->GetArrayLength(env, array) != 1) {
        return 0;
    }

    jint value = 0;
    (*env)->GetIntArrayRegion(env, array, 0, 1, &value);
    return value == 31 ? -d : 0;
}

static jint __throwing(JNIEnv* env, jobject receiver)
{
    if(__valid_receiver(env, receiver)) {
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), "ARM callback");
    }

    return 19;
}

static jdouble __mixed_v(JNIEnv* env, jclass clazz, jmethodID method, ...)
{
    va_list arguments;
    va_start(arguments, method);
    jdouble result = (*env)->CallStaticDoubleMethodV(env, clazz, method, arguments);
    va_end(arguments);
    return result;
}

/* Generated callback failures stop the emulator at this explicit trap. */
void __attribute__((noreturn)) __assert_func(const char* file, int line, const char* function, const char* expression)
{
    (void)file;
    (void)line;
    (void)function;
    (void)expression;
    __builtin_trap();
}

void test_native_many(JNIEnv* env);

int test_arm_native_vm_entry(JNIEnv* env, void (*stats)(jni_runtime_stats_t*))
{
    __env = env;
    jclass clazz = (*env)->FindClass(env, "test/ArmNativeVM");
    jobject instance = (*env)->AllocObject(env, clazz);
    if(!clazz || !instance) {
        return 1;
    }

#define CHECK_SCALAR(name, entry, code, expected, failure)                                                             \
    {                                                                                                                  \
        JNINativeMethod registration = {#name, "()" code, __return_##name};                                            \
        if((*env)->RegisterNatives(env, clazz, &registration, 1) != JNI_OK) {                                          \
            return failure;                                                                                            \
        }                                                                                                              \
        __receiver = clazz;                                                                                            \
        jmethodID method = (*env)->GetStaticMethodID(env, clazz, #name, "()" code);                                    \
        if(!method || (*env)->CallStatic##entry##Method(env, clazz, method) != (expected)) {                           \
            return failure;                                                                                            \
        }                                                                                                              \
        method = (*env)->GetMethodID(env, clazz, #name, "()" code);                                                    \
        __receiver = instance;                                                                                         \
        if(!method || (*env)->Call##entry##MethodA(env, instance, method, NULL) != (expected) ||                       \
            (*env)->CallNonvirtual##entry##Method(env, instance, clazz, method) != (expected)) {                       \
            return failure;                                                                                            \
        }                                                                                                              \
    }
    CHECK_SCALAR(boolean, Boolean, "Z", JNI_TRUE, 2)
    CHECK_SCALAR(byte, Byte, "B", -101, 3)
    CHECK_SCALAR(char, Char, "C", 0xfedc, 4)
    CHECK_SCALAR(short, Short, "S", -23456, 5)
    CHECK_SCALAR(int, Int, "I", -123456789, 6)
    CHECK_SCALAR(long, Long, "J", INT64_C(-0x123456789abcde), 7)
    CHECK_SCALAR(float, Float, "F", -3.25f, 8)
    CHECK_SCALAR(double, Double, "D", 12345.625, 9)
    jclass child = (*env)->FindClass(env, "android/app/Activity");
    jclass base = (*env)->FindClass(env, "android/content/Context");
    jmethodID late = (*env)->GetStaticMethodID(env, child, "lateNative", "()I");
    JNINativeMethod late_registration = {"lateNative", "()I", __return_int};

    if((*env)->RegisterNatives(env, base, &late_registration, 1) != JNI_OK) {
        return 19;
    }

    __receiver = base;
    if((*env)->CallStaticIntMethod(env, child, late) != -123456789) {
        return 19;
    }

    JNINativeMethod methods[] = {{"object", "()Ljava/lang/String;", __object},
        {"mixed", "(IFDJLjava/lang/String;[I)D", __mixed},
        {"throw", "()I", __throwing}};

    if((*env)->RegisterNatives(env, clazz, methods, 3) != JNI_OK) {
        return 10;
    }

    __receiver = clazz;
    jmethodID object_method = (*env)->GetStaticMethodID(env, clazz, "object", "()Ljava/lang/String;");
    jstring string = (*env)->CallStaticObjectMethod(env, clazz, object_method);
    if(!string || (*env)->GetStringLength(env, string) != 3) {
        return 11;
    }

    jintArray array = (*env)->NewIntArray(env, 1);
    jint value = 31;
    (*env)->SetIntArrayRegion(env, array, 0, 1, &value);
    jmethodID mixed = (*env)->GetStaticMethodID(env, clazz, "mixed", "(IFDJLjava/lang/String;[I)D");
    const jvalue arguments[] = {
        {.i = -17}, {.f = -3.25f}, {.d = 12345.625}, {.j = INT64_C(-0x123456789abcde)}, {.l = string}, {.l = array}};

    if((*env)->CallStaticDoubleMethodA(env, clazz, mixed, arguments) != -12345.625 ||
        (*env)->CallStaticDoubleMethod(
            env, clazz, mixed, -17, -3.25, 12345.625, INT64_C(-0x123456789abcde), string, array) != -12345.625 ||
        __mixed_v(env, clazz, mixed, -17, -3.25, 12345.625, INT64_C(-0x123456789abcde), string, array) != -12345.625) {
        return 12;
    }

    jni_runtime_stats_t before, after;
    stats(&before);
    for(unsigned index = 0; index < 50; ++index) {
        jobject result = (*env)->CallStaticObjectMethodA(env, clazz, object_method, NULL);
        if(!result) {
            return 13;
        }

        (*env)->DeleteLocalRef(env, result);
    }

    stats(&after);
    if(before.objects != after.objects || before.references != after.references) {
        return 14;
    }

    jmethodID throwing = (*env)->GetStaticMethodID(env, clazz, "throw", "()I");
    if((*env)->CallStaticIntMethod(env, clazz, throwing) != 0 || !(*env)->ExceptionCheck(env)) {
        return 15;
    }

    (*env)->ExceptionClear(env);
    test_native_many(env);
    if((*env)->ExceptionCheck(env)) {
        return 16;
    }

    JavaVM* vm = NULL;
    if((*env)->GetJavaVM(env, &vm) != JNI_OK || (*vm)->DestroyJavaVM(vm) != JNI_OK) {
        return 17;
    }

    stats(&after);
    return after.objects || after.references ? 18 : 0;
}
