#include <jni/aosp/jni.h>

extern "C" jfloat test_jni_arm_cpp_float_get(JNIEnv* env, jobject object, jfieldID field)
{
    return env->GetFloatField(object, field);
}

extern "C" void test_jni_arm_cpp_float_set(JNIEnv* env, jobject object, jfieldID field, jfloat value)
{
    env->SetFloatField(object, field, value);
}

extern "C" jdouble test_jni_arm_cpp_double_call(JNIEnv* env, jobject object, jmethodID method)
{
    return env->CallDoubleMethodA(object, method, nullptr);
}
