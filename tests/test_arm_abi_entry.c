#include <jni/aosp/jni.h>
#include <abi/calling.h>

jfloat test_jni_arm_float_get(JNIEnv* env, jobject object, jfieldID field);
void test_jni_arm_float_set(JNIEnv* env, jobject object, jfieldID field, jfloat value);
jdouble test_jni_arm_double_call(JNIEnv* env, jobject object, jmethodID method);
jfloat test_jni_arm_cpp_float_get(JNIEnv* env, jobject object, jfieldID field);
void test_jni_arm_cpp_float_set(JNIEnv* env, jobject object, jfieldID field, jfloat value);
jdouble test_jni_arm_cpp_double_call(JNIEnv* env, jobject object, jmethodID method);

static volatile float __stored;
static struct JNINativeInterface __table;

static jfloat VITAARC_SOFTFP __get(JNIEnv* env, jobject object, jfieldID field)
{
    return 3.25f;
}

static void VITAARC_SOFTFP __set(JNIEnv* env, jobject object, jfieldID field, jfloat value)
{
    __stored = value;
}

static jdouble VITAARC_SOFTFP __call(JNIEnv* env, jobject object, jmethodID method, const jvalue* args)
{
    return 6.5;
}

int test_jni_arm_entry(void)
{
    __table.GetFloatField = __get;
    __table.SetFloatField = __set;
    __table.CallDoubleMethodA = __call;
    JNIEnv env = &__table;
    jfloat value = test_jni_arm_float_get(&env, NULL, NULL);
    test_jni_arm_float_set(&env, NULL, NULL, 7.75f);
    jdouble number = test_jni_arm_double_call(&env, NULL, NULL);
    if(value != 3.25f || __stored != 7.75f || number != 6.5) {
        return 1;
    }

    value = test_jni_arm_cpp_float_get(&env, NULL, NULL);
    test_jni_arm_cpp_float_set(&env, NULL, NULL, 9.5f);
    number = test_jni_arm_cpp_double_call(&env, NULL, NULL);
    return value == 3.25f && __stored == 9.5f && number == 6.5 ? 0 : 2;
}
