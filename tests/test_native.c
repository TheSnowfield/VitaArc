#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <jni/runtime.h>
#include <jni/descriptor.h>

static JNIEnv* __env;
static jobject __receiver;
static jclass __clazz;
static jintArray __array;
static jstring __text;
static unsigned __void_calls;
static jmethodID __inner_method;
static jobject __caller_argument;
static jweak __weak_argument;
static jobject __exit_monitor;
static jclass __exit_class;
static jmethodID __exit_method;

static void __exception(const char* name)
{
    jthrowable error = (*__env)->ExceptionOccurred(__env);
    assert(error && (*__env)->IsInstanceOf(__env, error, (*__env)->FindClass(__env, name)));
    (*__env)->ExceptionClear(__env);
    (*__env)->DeleteLocalRef(__env, error);
}

static void __check_receiver(JNIEnv* env, jobject receiver)
{
    assert(env == __env);
    assert((*env)->GetObjectRefType(env, receiver) == JNILocalRefType);
    assert((*env)->IsSameObject(env, receiver, __receiver));
}

static jlong VITAARC_SOFTFP __mixed(JNIEnv* env, jobject receiver, jboolean z, jbyte b, jchar c, jshort s, jint i,
    jlong j, jfloat f, jdouble d, jstring text, jintArray array)
{
    __check_receiver(env, receiver);
    assert(z == JNI_TRUE && b == -101 && c == 0xfedc && s == -23456 && i == -123456789);
    assert(j == INT64_C(-0x123456789abcde) && f == -3.25f && d == 12345.625);
    assert((*env)->IsSameObject(env, text, __text));
    assert((*env)->IsSameObject(env, array, __array));
    jint value = 0;
    (*env)->GetIntArrayRegion(env, array, 0, 1, &value);
    assert(value == 31);
    assert((*env)->GetVersion(env) == JNI_VERSION_1_6);
    return INT64_C(0x765432109abcdef);
}

static jlong __mixed_v(jobject receiver, jmethodID method, ...)
{
    va_list arguments;
    va_start(arguments, method);
    jlong result = (*__env)->CallLongMethodV(__env, receiver, method, arguments);
    va_end(arguments);
    return result;
}

static jlong __static_mixed_v(jclass receiver, jmethodID method, ...)
{
    va_list arguments;
    va_start(arguments, method);
    jlong result = (*__env)->CallStaticLongMethodV(__env, receiver, method, arguments);
    va_end(arguments);
    return result;
}

#define MIXED_ARGS                                                                                                     \
    JNI_TRUE, -101, 0xfedc, -23456, -123456789, INT64_C(-0x123456789abcde), -3.25, 12345.625, __text, __array

static void __test_mixed(void)
{
    const char* signature = "(ZBCSIJFDLjava/lang/String;[I)J";
    jmethodID instance = (*__env)->GetMethodID(__env, __clazz, "mixed", signature);
    /* The static ID exists before registration, and the instance ID after it. */
    jmethodID static_method = (*__env)->GetStaticMethodID(__env, __clazz, "staticMixed", signature);
    JNINativeMethod registrations[] = {
        {"mixed", (char*)signature, __mixed}, {"staticMixed", (char*)signature, __mixed}};

    assert((*__env)->RegisterNatives(__env, __clazz, registrations, 2) == JNI_OK);
    assert((*__env)->GetMethodID(__env, __clazz, "mixed", signature) == instance);
    jvalue args[] = {{.z = JNI_TRUE},
        {.b = -101},
        {.c = 0xfedc},
        {.s = -23456},
        {.i = -123456789},
        {.j = INT64_C(-0x123456789abcde)},
        {.f = -3.25f},
        {.d = 12345.625},
        {.l = __text},
        {.l = __array}};

    __receiver = (*__env)->AllocObject(__env, __clazz);
    assert((*__env)->CallLongMethodA(__env, __receiver, instance, args) == INT64_C(0x765432109abcdef));
    assert((*__env)->CallLongMethod(__env, __receiver, instance, MIXED_ARGS) == INT64_C(0x765432109abcdef));
    assert(__mixed_v(__receiver, instance, MIXED_ARGS) == INT64_C(0x765432109abcdef));
    assert(
        (*__env)->CallNonvirtualLongMethodA(__env, __receiver, __clazz, instance, args) == INT64_C(0x765432109abcdef));
    __receiver = __clazz;
    assert((*__env)->CallStaticLongMethodA(__env, __clazz, static_method, args) == INT64_C(0x765432109abcdef));
    assert((*__env)->CallStaticLongMethod(__env, __clazz, static_method, MIXED_ARGS) == INT64_C(0x765432109abcdef));
    assert(__static_mixed_v(__clazz, static_method, MIXED_ARGS) == INT64_C(0x765432109abcdef));
}

#define DEFINE_SCALAR(name, type, code, expected)                                                                      \
    static type VITAARC_SOFTFP __return_##name(JNIEnv* env, jobject receiver)                                          \
    {                                                                                                                  \
        __check_receiver(env, receiver);                                                                               \
        return expected;                                                                                               \
    }                                                                                                                  \
    static type __call_##name##_v(jobject receiver, jmethodID method, ...)                                             \
    {                                                                                                                  \
        va_list args;                                                                                                  \
        va_start(args, method);                                                                                        \
        type result = (*__env)->Call##name##MethodV(__env, receiver, method, args);                                    \
        va_end(args);                                                                                                  \
        return result;                                                                                                 \
    }                                                                                                                  \
    static type __call_static_##name##_v(jclass receiver, jmethodID method, ...)                                       \
    {                                                                                                                  \
        va_list args;                                                                                                  \
        va_start(args, method);                                                                                        \
        type result = (*__env)->CallStatic##name##MethodV(__env, receiver, method, args);                              \
        va_end(args);                                                                                                  \
        return result;                                                                                                 \
    }                                                                                                                  \
    static type __call_nonvirtual_##name##_v(jobject receiver, jclass clazz, jmethodID method, ...)                    \
    {                                                                                                                  \
        va_list args;                                                                                                  \
        va_start(args, method);                                                                                        \
        type result = (*__env)->CallNonvirtual##name##MethodV(__env, receiver, clazz, method, args);                   \
        va_end(args);                                                                                                  \
        return result;                                                                                                 \
    }                                                                                                                  \
    static void __test_##name(void)                                                                                    \
    {                                                                                                                  \
        JNINativeMethod registration = {#name, "()" code, __return_##name};                                            \
        assert((*__env)->RegisterNatives(__env, __clazz, &registration, 1) == JNI_OK);                                 \
        jmethodID method = (*__env)->GetMethodID(__env, __clazz, #name, "()" code);                                    \
        jobject object = (*__env)->AllocObject(__env, __clazz);                                                        \
        __receiver = object;                                                                                           \
        assert((*__env)->Call##name##MethodA(__env, object, method, NULL) == expected);                                \
        assert((*__env)->Call##name##Method(__env, object, method) == expected);                                       \
        assert(__call_##name##_v(object, method) == expected);                                                         \
        assert((*__env)->CallNonvirtual##name##MethodA(__env, object, __clazz, method, NULL) == expected);             \
        assert((*__env)->CallNonvirtual##name##Method(__env, object, __clazz, method) == expected);                    \
        assert(__call_nonvirtual_##name##_v(object, __clazz, method) == expected);                                     \
        method = (*__env)->GetStaticMethodID(__env, __clazz, #name, "()" code);                                        \
        __receiver = __clazz;                                                                                          \
        assert((*__env)->CallStatic##name##MethodA(__env, __clazz, method, NULL) == expected);                         \
        assert((*__env)->CallStatic##name##Method(__env, __clazz, method) == expected);                                \
        assert(__call_static_##name##_v(__clazz, method) == expected);                                                 \
        (*__env)->DeleteLocalRef(__env, object);                                                                       \
    }

DEFINE_SCALAR(Boolean, jboolean, "Z", JNI_TRUE)
DEFINE_SCALAR(Byte, jbyte, "B", -101)
DEFINE_SCALAR(Char, jchar, "C", 0xfedc)
DEFINE_SCALAR(Short, jshort, "S", -23456)
DEFINE_SCALAR(Int, jint, "I", -123456789)
DEFINE_SCALAR(Long, jlong, "J", INT64_C(-0x123456789abcde))
DEFINE_SCALAR(Float, jfloat, "F", -3.25f)
DEFINE_SCALAR(Double, jdouble, "D", 12345.625)

static void __return_void(JNIEnv* env, jobject receiver)
{
    __check_receiver(env, receiver);
    ++__void_calls;
}

static jobject __return_object(JNIEnv* env, jobject receiver)
{
    __check_receiver(env, receiver);
    assert((*env)->PushLocalFrame(env, 8) == JNI_OK);
    (*env)->NewStringUTF(env, "temporary");
    assert((*env)->PushLocalFrame(env, 4) == JNI_OK);
    return (*env)->NewStringUTF(env, "native result");
}

static jobject __identity(JNIEnv* env, jobject receiver, jobject argument)
{
    __check_receiver(env, receiver);
    if(argument) {
        assert((*env)->GetObjectRefType(env, argument) == JNILocalRefType);
    }

    return argument;
}

static jobject __object_v(jobject receiver, jmethodID method, int scope, ...)
{
    va_list args;
    va_start(args, scope);
    jobject result;
    if(scope == 0) {
        result = (*__env)->CallObjectMethodV(__env, receiver, method, args);
    } else if(scope == 1) {
        result = (*__env)->CallStaticObjectMethodV(__env, (jclass)receiver, method, args);
    } else {
        result = (*__env)->CallNonvirtualObjectMethodV(__env, receiver, __clazz, method, args);
    }

    va_end(args);
    return result;
}

static void __void_v(jobject receiver, jmethodID method, int scope, ...)
{
    va_list args;
    va_start(args, scope);
    if(scope == 0) {
        (*__env)->CallVoidMethodV(__env, receiver, method, args);
    } else if(scope == 1) {
        (*__env)->CallStaticVoidMethodV(__env, (jclass)receiver, method, args);
    } else {
        (*__env)->CallNonvirtualVoidMethodV(__env, receiver, __clazz, method, args);
    }

    va_end(args);
}

static void __check_object_result(jobject result)
{
    assert(result);
    const char* text = (*__env)->GetStringUTFChars(__env, result, NULL);
    assert(text && strcmp(text, "native result") == 0);
    (*__env)->ReleaseStringUTFChars(__env, result, text);
    (*__env)->DeleteLocalRef(__env, result);
}

static void __test_object_and_void_scopes(void)
{
    jobject instance = (*__env)->AllocObject(__env, __clazz);
    for(int scope = 0; scope < 3; ++scope) {
        __receiver = scope == 1 ? __clazz : instance;
        jmethodID object = scope == 1 ? (*__env)->GetStaticMethodID(__env, __clazz, "object", "()Ljava/lang/String;")
                                      : (*__env)->GetMethodID(__env, __clazz, "object", "()Ljava/lang/String;");
        jmethodID method = scope == 1 ? (*__env)->GetStaticMethodID(__env, __clazz, "void", "()V")
                                      : (*__env)->GetMethodID(__env, __clazz, "void", "()V");
        if(scope == 0) {
            __check_object_result((*__env)->CallObjectMethod(__env, instance, object));
            __check_object_result((*__env)->CallObjectMethodA(__env, instance, object, NULL));
            (*__env)->CallVoidMethod(__env, instance, method);
            (*__env)->CallVoidMethodA(__env, instance, method, NULL);
        } else if(scope == 1) {
            __check_object_result((*__env)->CallStaticObjectMethod(__env, __clazz, object));
            __check_object_result((*__env)->CallStaticObjectMethodA(__env, __clazz, object, NULL));
            (*__env)->CallStaticVoidMethod(__env, __clazz, method);
            (*__env)->CallStaticVoidMethodA(__env, __clazz, method, NULL);
        } else {
            __check_object_result((*__env)->CallNonvirtualObjectMethod(__env, instance, __clazz, object));
            __check_object_result((*__env)->CallNonvirtualObjectMethodA(__env, instance, __clazz, object, NULL));
            (*__env)->CallNonvirtualVoidMethod(__env, instance, __clazz, method);
            (*__env)->CallNonvirtualVoidMethodA(__env, instance, __clazz, method, NULL);
        }

        __check_object_result(__object_v(__receiver, object, scope));
        __void_v(__receiver, method, scope);
    }

    assert(__void_calls == 9);
    (*__env)->DeleteLocalRef(__env, instance);
}

static jint __throwing(JNIEnv* env, jobject receiver)
{
    __check_receiver(env, receiver);
    (*env)->NewStringUTF(env, "discard on exception");
    assert((*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), "native failure") == JNI_OK);
    return 29;
}

static jint __inner(JNIEnv* env, jobject receiver)
{
    __check_receiver(env, receiver);
    return 41;
}

static jint __outer(JNIEnv* env, jobject receiver)
{
    __check_receiver(env, receiver);
    return (*env)->CallStaticIntMethod(env, __clazz, __inner_method) + 1;
}

static jint __churn(JNIEnv* env, jobject receiver)
{
    __check_receiver(env, receiver);
    jni_runtime_stats_t before, after;
    jni_runtime_get_stats(&before);
    for(unsigned index = 0; index < 1000; ++index) {
        jobject temporary = (*env)->NewStringUTF(env, "native body allocation");
        (*env)->DeleteLocalRef(env, temporary);
    }

    jni_runtime_get_stats(&after);
    assert(before.objects == after.objects && before.references == after.references);
    return 1;
}

static void* __worker(void* unused)
{
    (void)unused;
    JavaVM* vm = jni_runtime_get_java_vm();
    JNIEnv* env = NULL;
    assert((*vm)->AttachCurrentThread(vm, &env, NULL) == JNI_OK && env != __env);
    assert((*env)->PushLocalFrame(env, 4) == JNI_OK);
    (*env)->NewStringUTF(env, "collect other threads");
    (*env)->PopLocalFrame(env, NULL);
    assert(!(*env)->IsSameObject(env, __weak_argument, NULL));
    jclass clazz = (*env)->GetObjectClass(env, __weak_argument);
    assert((*env)->UnregisterNatives(env, clazz) == JNI_OK);
    assert((*vm)->DetachCurrentThread(vm) == JNI_OK);
    return NULL;
}

static jboolean __join_worker(JNIEnv* env, jobject receiver, jobject argument)
{
    __check_receiver(env, receiver);
    __weak_argument = (*env)->NewWeakGlobalRef(env, argument);
    /* Neither a caller local nor the native argument local remains. Only the
     * VM's native argument root keeps this object alive during worker GC. */
    (*env)->DeleteLocalRef(env, argument);
    (*env)->DeleteLocalRef(env, __caller_argument);
    JavaVM* vm = jni_runtime_get_java_vm();
    assert((*vm)->DetachCurrentThread(vm) == JNI_ERR);
    assert((*vm)->DestroyJavaVM(vm) == JNI_ERR);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, __worker, NULL) == 0);
    assert(pthread_join(thread, NULL) == 0);
    jboolean retained = !(*env)->IsSameObject(env, __weak_argument, NULL);
    (*env)->DeleteWeakGlobalRef(env, __weak_argument);
    __weak_argument = NULL;
    return retained;
}

static void __test_frames_exceptions_and_threads(void)
{
    JNINativeMethod methods[] = {{"void", "()V", __return_void},
        {"object", "()Ljava/lang/String;", __return_object},
        {"identity", "(Ljava/lang/Object;)Ljava/lang/Object;", __identity},
        {"throw", "()I", __throwing},
        {"inner", "()I", __inner},
        {"outer", "()I", __outer},
        {"churn", "()I", __churn},
        {"worker", "(Ljava/lang/Object;)Z", __join_worker}};

    assert((*__env)->RegisterNatives(__env, __clazz, methods, sizeof(methods) / sizeof(methods[0])) == JNI_OK);
    __test_object_and_void_scopes();
    __receiver = __clazz;
    jmethodID method = (*__env)->GetStaticMethodID(__env, __clazz, "void", "()V");
    (*__env)->CallStaticVoidMethod(__env, __clazz, method);
    (*__env)->CallStaticVoidMethodA(__env, __clazz, method, NULL);
    assert(__void_calls == 11);
    jmethodID object = (*__env)->GetStaticMethodID(__env, __clazz, "object", "()Ljava/lang/String;");
    jobject result = (*__env)->CallStaticObjectMethod(__env, __clazz, object);
    const char* text = (*__env)->GetStringUTFChars(__env, result, NULL);
    assert(text && strcmp(text, "native result") == 0);
    (*__env)->ReleaseStringUTFChars(__env, result, text);
    (*__env)->DeleteLocalRef(__env, result);
    jni_runtime_stats_t before, after;
    jni_runtime_get_stats(&before);
    for(unsigned index = 0; index < 1000; ++index) {
        result = (*__env)->CallStaticObjectMethodA(__env, __clazz, object, NULL);
        (*__env)->DeleteLocalRef(__env, result);
    }

    jni_runtime_get_stats(&after);
    assert(before.objects == after.objects && before.references == after.references);
    method = (*__env)->GetStaticMethodID(__env, __clazz, "identity", "(Ljava/lang/Object;)Ljava/lang/Object;");
    result = (*__env)->CallStaticObjectMethod(__env, __clazz, method, __text);
    assert(result != __text && (*__env)->IsSameObject(__env, result, __text));
    (*__env)->DeleteLocalRef(__env, result);
    assert((*__env)->GetStringLength(__env, __text) == 7);
    assert((*__env)->CallStaticObjectMethod(__env, __clazz, method, NULL) == NULL);
    assert((*__env)->CallStaticObjectMethod(__env, __clazz, method, (jobject)(uintptr_t)0x1234) == NULL);
    __exception("java/lang/IllegalArgumentException");
    method = (*__env)->GetStaticMethodID(__env, __clazz, "throw", "()I");
    assert((*__env)->CallStaticIntMethod(__env, __clazz, method) == 0);
    assert((*__env)->ExceptionCheck(__env));
    __exception("java/lang/RuntimeException");
    __inner_method = (*__env)->GetStaticMethodID(__env, __clazz, "inner", "()I");
    method = (*__env)->GetStaticMethodID(__env, __clazz, "outer", "()I");
    assert((*__env)->CallStaticIntMethod(__env, __clazz, method) == 42);
    method = (*__env)->GetStaticMethodID(__env, __clazz, "churn", "()I");
    assert((*__env)->CallStaticIntMethod(__env, __clazz, method) == 1);
    __caller_argument = (*__env)->AllocObject(__env, __clazz);
    method = (*__env)->GetStaticMethodID(__env, __clazz, "worker", "(Ljava/lang/Object;)Z");
    assert((*__env)->CallStaticBooleanMethod(__env, __clazz, method, __caller_argument));
    assert(!(*__env)->CallStaticBooleanMethod(__env, __clazz, method, NULL));
    __exception("java/lang/UnsatisfiedLinkError");
}

static jint __base(JNIEnv* env, jobject receiver)
{
    (void)env;
    (void)receiver;
    return 11;
}

static jint __override(JNIEnv* env, jobject receiver)
{
    (void)env;
    (void)receiver;
    return 22;
}

static jint __declaring_class(JNIEnv* env, jclass clazz)
{
    jclass expected = (*env)->FindClass(env, "android/content/Context");
    return (*env)->IsSameObject(env, clazz, expected) ? 33 : 0;
}

static void __test_registration_and_inheritance(void)
{
    jclass base = (*__env)->FindClass(__env, "android/content/Context");
    jclass child = (*__env)->FindClass(__env, "android/app/Activity");
    jobject instance = (*__env)->AllocObject(__env, child);
    jmethodID late_instance = (*__env)->GetMethodID(__env, child, "lateInstance", "()I");
    jmethodID late_static = (*__env)->GetStaticMethodID(__env, child, "lateStatic", "()I");
    JNINativeMethod late_methods[] = {{"lateInstance", "()I", __base}, {"lateStatic", "()I", __declaring_class}};

    assert((*__env)->RegisterNatives(__env, base, late_methods, 2) == JNI_OK);
    assert((*__env)->CallIntMethod(__env, instance, late_instance) == 11);
    assert((*__env)->CallNonvirtualIntMethod(__env, instance, base, late_instance) == 11);
    assert((*__env)->CallStaticIntMethod(__env, child, late_static) == 33);
    assert((*__env)->GetMethodID(__env, child, "lateInstance", "()I") ==
        (*__env)->GetMethodID(__env, base, "lateInstance", "()I"));
    late_methods[0].fnPtr = __override;
    late_methods[1].fnPtr = __override;
    assert((*__env)->RegisterNatives(__env, child, late_methods, 2) == JNI_OK);
    jmethodID child_instance = (*__env)->GetMethodID(__env, child, "lateInstance", "()I");
    jmethodID child_static = (*__env)->GetStaticMethodID(__env, child, "lateStatic", "()I");
    assert((*__env)->CallIntMethod(__env, instance, late_instance) == 22);
    assert((*__env)->CallNonvirtualIntMethod(__env, instance, base, late_instance) == 11);
    assert((*__env)->CallNonvirtualIntMethod(__env, instance, child, child_instance) == 22);
    assert((*__env)->CallStaticIntMethod(__env, child, late_static) == 33);
    assert((*__env)->CallStaticIntMethod(__env, child, child_static) == 22);
    JNINativeMethod method = {"number", "()I", __base};

    assert((*__env)->RegisterNatives(__env, base, &method, 1) == JNI_OK);
    jmethodID base_id = (*__env)->GetMethodID(__env, base, "number", "()I");
    assert((*__env)->GetMethodID(__env, child, "number", "()I") == base_id);
    assert((*__env)->CallIntMethod(__env, instance, base_id) == 11);
    method.fnPtr = __override;
    assert((*__env)->RegisterNatives(__env, child, &method, 1) == JNI_OK);
    assert((*__env)->CallIntMethod(__env, instance, base_id) == 22);
    assert((*__env)->CallNonvirtualIntMethod(__env, instance, base, base_id) == 11);
    jmethodID static_id = (*__env)->GetStaticMethodID(__env, base, "number", "()I");
    assert((*__env)->CallStaticIntMethod(__env, child, static_id) == 11);
    method.fnPtr = __override;
    assert((*__env)->RegisterNatives(__env, base, &method, 1) == JNI_OK);
    assert((*__env)->CallNonvirtualIntMethod(__env, instance, base, base_id) == 22);
    JNINativeMethod invalid[] = {{"number", "()I", __base}, {"bad", "(V)I", __base}};

    assert((*__env)->RegisterNatives(__env, base, invalid, 2) == JNI_EINVAL);
    assert((*__env)->CallStaticIntMethod(__env, base, static_id) == 22);
    assert((*__env)->RegisterNatives(__env, (jclass)__text, &method, 1) == JNI_EINVAL);
    assert((*__env)->UnregisterNatives(__env, (jclass)__text) == JNI_EINVAL);
    assert((*__env)->UnregisterNatives(__env, base) == JNI_OK);
    assert((*__env)->CallStaticIntMethod(__env, base, static_id) == 0);
    __exception("java/lang/UnsatisfiedLinkError");
    assert((*__env)->UnregisterNatives(__env, child) == JNI_OK);
    assert((*__env)->CallIntMethod(__env, instance, base_id) == 0);
    __exception("java/lang/UnsatisfiedLinkError");
}

static void __exit_native(JNIEnv* env, jclass clazz, jobject monitor)
{
    (void)clazz;
    assert((*env)->GetVersion(env) == JNI_VERSION_1_6);
    assert((*env)->PushLocalFrame(env, 8) == JNI_OK);
    (*env)->NewStringUTF(env, "discard at native thread exit");
    assert((*env)->MonitorEnter(env, monitor) == JNI_OK);
    pthread_exit(NULL);
}

static void* __exit_worker(void* unused)
{
    (void)unused;
    JavaVM* vm = jni_runtime_get_java_vm();
    JNIEnv* env = NULL;
    assert((*vm)->AttachCurrentThread(vm, &env, NULL) == JNI_OK);
    (*env)->CallStaticVoidMethod(env, __exit_class, __exit_method, __exit_monitor);
    assert(0 && "native pthread_exit unexpectedly returned");
    return NULL;
}

static void __test_native_thread_exit(void)
{
    JNINativeMethod registration = {"exit", "(Ljava/lang/Object;)V", __exit_native};

    assert((*__env)->RegisterNatives(__env, __clazz, &registration, 1) == JNI_OK);
    __exit_method = (*__env)->GetStaticMethodID(__env, __clazz, "exit", "(Ljava/lang/Object;)V");
    __exit_class = (*__env)->NewGlobalRef(__env, __clazz);
    jobject object = (*__env)->AllocObject(__env, __clazz);
    __exit_monitor = (*__env)->NewGlobalRef(__env, object);
    (*__env)->DeleteLocalRef(__env, object);
    jni_runtime_stats_t before, after;
    jni_runtime_get_stats(&before);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, __exit_worker, NULL) == 0);
    assert(pthread_join(thread, NULL) == 0);
    assert((*__env)->MonitorEnter(__env, __exit_monitor) == JNI_OK);
    assert((*__env)->MonitorExit(__env, __exit_monitor) == JNI_OK);
    jni_runtime_get_stats(&after);
    assert(before.objects == after.objects && before.references == after.references);
    (*__env)->DeleteGlobalRef(__env, __exit_monitor);
    (*__env)->DeleteGlobalRef(__env, __exit_class);
}

void test_native_many(JNIEnv* env);

int main(void)
{
    __env = jni_runtime_get_env();
    __clazz = (*__env)->FindClass(__env, "test/NativeCalls");
    __text = (*__env)->NewStringUTF(__env, "fixture");
    __array = (*__env)->NewIntArray(__env, 1);
    jint value = 31;
    (*__env)->SetIntArrayRegion(__env, __array, 0, 1, &value);
    __test_mixed();
    __test_Boolean();
    __test_Byte();
    __test_Char();
    __test_Short();
    __test_Int();
    __test_Long();
    __test_Float();
    __test_Double();
    __test_frames_exceptions_and_threads();
    __test_registration_and_inheritance();
    __test_native_thread_exit();
    test_native_many(__env);
    assert(!(*__env)->ExceptionCheck(__env));
    JavaVM* vm = jni_runtime_get_java_vm();
    assert((*vm)->DestroyJavaVM(vm) == JNI_OK);
    jni_runtime_stats_t stats;
    jni_runtime_get_stats(&stats);
    assert(stats.objects == 0 && stats.references == 0);
    puts(
        "Native JNI dispatch passed: scalars, mixed ABI, scopes, frames, inheritance, replacement, exceptions and worker JNI");
    return 0;
}
