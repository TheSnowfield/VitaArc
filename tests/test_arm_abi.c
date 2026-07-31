#include <jni/aosp/jni.h>

jfloat test_jni_arm_float_get(JNIEnv* env, jobject object, jfieldID field)
{
    return (*env)->GetFloatField(env, object, field);
}

void test_jni_arm_float_set(JNIEnv* env, jobject object, jfieldID field, jfloat value)
{
    (*env)->SetFloatField(env, object, field, value);
}

jdouble test_jni_arm_double_call(JNIEnv* env, jobject object, jmethodID method)
{
    return (*env)->CallDoubleMethodA(env, object, method, NULL);
}
