#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include <config.h>
#include <jni/descriptor.h>
#include <jni/runtime.h>

static JNIEnv* __env;

static void __expect_exception(const char* name)
{
    jthrowable exception = (*__env)->ExceptionOccurred(__env);
    assert(exception != NULL);
    assert((*__env)->IsInstanceOf(__env, exception, (*__env)->FindClass(__env, name)));
    (*__env)->ExceptionClear(__env);
}

static void __expect_text(jobject value, const char* text)
{
    const char* actual = jni_runtime_get_string_utf(value);
    assert(actual != NULL && strcmp(actual, text) == 0);
}

static jobject __call_object_v(jobject receiver, jmethodID method, ...)
{
    va_list args;
    va_start(args, method);
    jobject result = (*__env)->CallObjectMethodV(__env, receiver, method, args);
    va_end(args);
    return result;
}

static jint __call_int_v(jobject receiver, jmethodID method, ...)
{
    va_list args;
    va_start(args, method);
    jint result = (*__env)->CallIntMethodV(__env, receiver, method, args);
    va_end(args);
    return result;
}

static jint __call_nonvirtual_int_v(jobject receiver, jclass clazz, jmethodID method, ...)
{
    va_list args;
    va_start(args, method);
    jint result = (*__env)->CallNonvirtualIntMethodV(__env, receiver, clazz, method, args);
    va_end(args);
    return result;
}

static jobject __new_object_v(jclass clazz, jmethodID method, ...)
{
    va_list args;
    va_start(args, method);
    jobject result = (*__env)->NewObjectV(__env, clazz, method, args);
    va_end(args);
    return result;
}

static void __test_descriptors(void)
{
    jni_sig_t descriptor;
    assert(jni_sig_parse("(ZBCSIJFDLjava/lang/String;[[I)[Ljava/lang/Object;", &descriptor));
    assert(descriptor.argc == 10);
    assert(memcmp(descriptor.args, "ZBCSIJFDLL", 10) == 0);
    assert(descriptor.ret == 'L');
    assert(jni_sig_parse("()V", &descriptor));
    static const char* invalid[] = {"",
        "I",
        "(",
        "()",
        "(V)V",
        "([V)V",
        "(L;)V",
        "(Ljava.lang.String;)V",
        "(I)Vextra",
        "()II",
        "(Q)V",
        "(Ljava/lang/String)V",
        "([)I"};

    jclass clazz = (*__env)->FindClass(__env, "test/Descriptors");
    for(size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        assert(!jni_sig_parse(invalid[index], &descriptor));
        assert((*__env)->GetMethodID(__env, clazz, "invalid", invalid[index]) == NULL);
        __expect_exception("java/lang/IllegalArgumentException");
    }

    assert(!jni_sig_parse(NULL, &descriptor));
    assert(!jni_sig_parse("()V", NULL));
    assert((*__env)->GetMethodID(__env, clazz, "<init>", "()I") == NULL);
    __expect_exception("java/lang/IllegalArgumentException");
}

static void __test_strings(void)
{
    jclass clazz = (*__env)->FindClass(__env, "java/lang/String");
    const jchar text[] = {'A', 0, 0xd83d, 0xde00};

    jstring string = (*__env)->NewString(__env, text, 4);
    jmethodID length = (*__env)->GetMethodID(__env, clazz, "length", "()I");
    assert((*__env)->CallIntMethod(__env, string, length) == 4);
    assert(__call_int_v(string, length) == 4);
    assert((*__env)->CallIntMethodA(__env, string, length, NULL) == 4);
    assert((*__env)->CallNonvirtualIntMethod(__env, string, clazz, length) == 4);
    assert(__call_nonvirtual_int_v(string, clazz, length) == 4);
    assert((*__env)->CallNonvirtualIntMethodA(__env, string, clazz, length, NULL) == 4);
    jmethodID char_at = (*__env)->GetMethodID(__env, clazz, "charAt", "(I)C");
    assert((*__env)->CallCharMethod(__env, string, char_at, 2) == 0xd83d);
    (*__env)->CallCharMethod(__env, string, char_at, 4);
    __expect_exception("java/lang/StringIndexOutOfBoundsException");
    jclass object_class = (*__env)->FindClass(__env, "java/lang/Object");
    jmethodID equals = (*__env)->GetMethodID(__env, object_class, "equals", "(Ljava/lang/Object;)Z");
    jstring copy = (*__env)->NewString(__env, text, 4);
    assert((*__env)->CallBooleanMethod(__env, string, equals, copy));
    assert(!(*__env)->CallNonvirtualBooleanMethod(__env, string, object_class, equals, copy));
    assert((*__env)->CallNonvirtualBooleanMethod(__env, string, object_class, equals, string));
    assert(!(*__env)->CallBooleanMethod(__env, string, equals, NULL));
    (*__env)->CallIntMethod(__env, NULL, length);
    __expect_exception("java/lang/NullPointerException");
    (*__env)->CallFloatMethod(__env, string, length);
    __expect_exception("java/lang/IllegalArgumentException");
    (*__env)->CallCharMethodA(__env, string, char_at, NULL);
    __expect_exception("java/lang/IllegalArgumentException");
    (*__env)->CallStaticIntMethod(__env, clazz, length);
    __expect_exception("java/lang/IllegalArgumentException");
    (*__env)->CallNonvirtualIntMethod(__env, string, object_class, length);
    __expect_exception("java/lang/IllegalArgumentException");
    (*__env)->CallNonvirtualIntMethod(__env, string, NULL, length);
    __expect_exception("java/lang/IllegalArgumentException");
    jobject object = (*__env)->AllocObject(__env, object_class);
    (*__env)->CallIntMethod(__env, object, length);
    __expect_exception("java/lang/IllegalArgumentException");
    jmethodID unsupported = (*__env)->GetMethodID(__env, clazz, "hashCode", "()I");
    assert((*__env)->CallIntMethod(__env, string, unsupported) == 0);
    __expect_exception("java/lang/UnsupportedOperationException");
}

static void __test_services(void)
{
    jclass helper = (*__env)->FindClass(__env, "org/cocos2dx/lib/Cocos2dxHelper");
    jmethodID get_activity = (*__env)->GetStaticMethodID(__env, helper, "getActivity", "()Landroid/app/Activity;");
    jobject activity = (*__env)->CallStaticObjectMethod(__env, helper, get_activity);
    jclass context = (*__env)->FindClass(__env, "android/content/Context");
    assert((*__env)->IsInstanceOf(__env, activity, context));
    assert((*__env)->IsAssignableFrom(__env, (*__env)->GetObjectClass(__env, activity), context));
    jmethodID package = (*__env)->GetMethodID(__env, context, "getPackageName", "()Ljava/lang/String;");
    __expect_text((*__env)->CallObjectMethod(__env, activity, package), "moe.low.arc");
    __expect_text(__call_object_v(activity, package), "moe.low.arc");
    __expect_text((*__env)->CallObjectMethodA(__env, activity, package, NULL), "moe.low.arc");
    jmethodID get_loader = (*__env)->GetMethodID(__env, context, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject loader = (*__env)->CallObjectMethod(__env, activity, get_loader);
    jclass loader_class = (*__env)->GetObjectClass(__env, loader);
    jmethodID load = (*__env)->GetMethodID(__env, loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jvalue name = {.l = (*__env)->NewStringUTF(__env, "org.cocos2dx.lib.Cocos2dxHelper")};

    assert((*__env)->IsSameObject(__env, (*__env)->CallObjectMethodA(__env, loader, load, &name), helper));
    (*__env)->CallObjectMethod(__env, loader, load, NULL);
    __expect_exception("java/lang/NullPointerException");
    jclass class_class = (*__env)->GetObjectClass(__env, helper);
    jmethodID get_name = (*__env)->GetMethodID(__env, class_class, "getName", "()Ljava/lang/String;");
    __expect_text((*__env)->CallObjectMethod(__env, helper, get_name), "org.cocos2dx.lib.Cocos2dxHelper");
    jmethodID get_files = (*__env)->GetMethodID(__env, context, "getFilesDir", "()Ljava/io/File;");
    jobject file = (*__env)->CallObjectMethod(__env, activity, get_files);
    assert(file != NULL);
    jclass file_class = (*__env)->GetObjectClass(__env, file);
    jmethodID path = (*__env)->GetMethodID(__env, file_class, "getAbsolutePath", "()Ljava/lang/String;");
    __expect_text((*__env)->CallObjectMethod(__env, file, path), ANDROID_FILES_PATH);
    jmethodID directory = (*__env)->GetMethodID(__env, file_class, "isDirectory", "()Z");
    assert((*__env)->CallBooleanMethod(__env, file, directory));
    jmethodID constructor = (*__env)->GetMethodID(__env, file_class, "<init>", "(Ljava/lang/String;)V");
    jvalue argument = {.l = (*__env)->NewStringUTF(__env, "app0:package/assets/fixture.txt")};

    jobject files[] = {(*__env)->NewObject(__env, file_class, constructor, argument.l),
        __new_object_v(file_class, constructor, argument.l),
        (*__env)->NewObjectA(__env, file_class, constructor, &argument)};

    jmethodID exists = (*__env)->GetMethodID(__env, file_class, "exists", "()Z");
    for(size_t index = 0; index < sizeof(files) / sizeof(files[0]); ++index) {
        assert(files[index] != NULL);
        assert((*__env)->CallBooleanMethod(__env, files[index], exists));
        __expect_text((*__env)->CallObjectMethod(__env, files[index], path), "app0:package/assets/fixture.txt");
    }

    assert((*__env)->NewObject(__env, file_class, constructor, NULL) == NULL);
    __expect_exception("java/lang/NullPointerException");
    assert((*__env)->NewObject(__env, file_class, path) == NULL);
    __expect_exception("java/lang/IllegalArgumentException");
}

static void __test_arrays(void)
{
    jclass string_class = (*__env)->FindClass(__env, "java/lang/String");
    jobject text = (*__env)->NewStringUTF(__env, "array");
    jobjectArray strings = (*__env)->NewObjectArray(__env, 2, string_class, text);
    assert(strings != NULL);
    assert((*__env)->IsSameObject(__env, (*__env)->GetObjectArrayElement(__env, strings, 1), text));
    jobject plain = (*__env)->AllocObject(__env, (*__env)->FindClass(__env, "java/lang/Object"));
    (*__env)->SetObjectArrayElement(__env, strings, 0, plain);
    __expect_exception("java/lang/ArrayStoreException");
    assert((*__env)->IsSameObject(__env, (*__env)->GetObjectArrayElement(__env, strings, 0), text));
    (*__env)->SetObjectArrayElement(__env, strings, 0, NULL);
    assert((*__env)->GetObjectArrayElement(__env, strings, 0) == NULL);
    (*__env)->GetObjectArrayElement(__env, strings, INT_MAX);
    __expect_exception("java/lang/ArrayIndexOutOfBoundsException");
    assert((*__env)->NewObjectArray(__env, 1, string_class, plain) == NULL);
    __expect_exception("java/lang/ArrayStoreException");
    assert((*__env)->NewIntArray(__env, -1) == NULL);
    __expect_exception("java/lang/NegativeArraySizeException");
    jintArray ints = (*__env)->NewIntArray(__env, 2);
    (*__env)->GetObjectArrayElement(__env, (jobjectArray)ints, 0);
    __expect_exception("java/lang/IllegalArgumentException");
    (*__env)->SetObjectArrayElement(__env, (jobjectArray)ints, 0, text);
    __expect_exception("java/lang/IllegalArgumentException");
    jfloat out;
    (*__env)->GetFloatArrayRegion(__env, (jfloatArray)ints, 0, 1, &out);
    __expect_exception("java/lang/IllegalArgumentException");
    (*__env)->GetIntArrayElements(__env, (jintArray)strings, NULL);
    __expect_exception("java/lang/IllegalArgumentException");
    assert((*__env)->GetPrimitiveArrayCritical(__env, strings, NULL) == NULL);
    __expect_exception("java/lang/IllegalArgumentException");
    assert((*__env)->IsInstanceOf(__env, ints, (*__env)->FindClass(__env, "[I")));
    assert((*__env)->IsInstanceOf(__env, strings, (*__env)->FindClass(__env, "[Ljava/lang/Object;")));
    assert(!(*__env)->IsInstanceOf(__env, ints, (*__env)->FindClass(__env, "[J")));
    assert((*__env)->IsInstanceOf(__env, ints, (*__env)->FindClass(__env, "java/lang/Cloneable")));
    jobjectArray nested = (*__env)->NewObjectArray(__env, 1, (*__env)->FindClass(__env, "[I"), ints);
    assert(nested != NULL);
    assert((*__env)->IsInstanceOf(__env, nested, (*__env)->FindClass(__env, "[[I")));
    assert((*__env)->IsInstanceOf(__env, nested, (*__env)->FindClass(__env, "[Ljava/lang/Object;")));
    assert(!(*__env)->IsInstanceOf(__env, nested, (*__env)->FindClass(__env, "[[Ljava/lang/Object;")));
    jint value = 0;
    (*__env)->GetIntArrayRegion(__env, ints, 1, INT_MAX, &value);
    __expect_exception("java/lang/ArrayIndexOutOfBoundsException");
    (*__env)->GetIntArrayRegion(__env, ints, 0, 1, NULL);
    __expect_exception("java/lang/NullPointerException");
    (*__env)->GetIntArrayRegion(__env, ints, 2, 0, NULL);
    assert(!(*__env)->ExceptionCheck(__env));
    jboolean copy = JNI_TRUE;
    jint* elements = (*__env)->GetIntArrayElements(__env, ints, &copy);
    assert(elements != NULL && !copy);
    elements[0] = 17;
    (*__env)->ReleaseIntArrayElements(__env, ints, elements, JNI_ABORT);
    (*__env)->GetIntArrayRegion(__env, ints, 0, 1, &value);
    assert(value == 17);
    (*__env)->ReleaseIntArrayElements(__env, ints, &value, 0);
    __expect_exception("java/lang/IllegalArgumentException");
    (*__env)->ReleaseIntArrayElements(__env, ints, elements, 99);
    __expect_exception("java/lang/IllegalArgumentException");
}

static void __test_inherited_fields(void)
{
    jclass base = (*__env)->FindClass(__env, "android/content/Context");
    jclass child = (*__env)->FindClass(__env, "org/cocos2dx/cpp/AppActivity");
    jfieldID field = (*__env)->GetFieldID(__env, base, "inheritedCount", "I");
    jfieldID inherited = (*__env)->GetFieldID(__env, child, "inheritedCount", "I");
    assert(field == inherited);
    jobject object = (*__env)->AllocObject(__env, child);
    (*__env)->SetIntField(__env, object, field, 123);
    assert((*__env)->GetIntField(__env, object, inherited) == 123);
    jfieldID shared = (*__env)->GetStaticFieldID(__env, base, "sharedCount", "I");
    assert((*__env)->GetStaticFieldID(__env, child, "sharedCount", "I") == shared);
    (*__env)->SetStaticIntField(__env, child, shared, 321);
    assert((*__env)->GetStaticIntField(__env, base, shared) == 321);
    (*__env)->SetObjectField(__env, object, field, object);
    __expect_exception("java/lang/IllegalArgumentException");
    assert((*__env)->GetIntField(__env, object, field) == 123);
    (*__env)->GetStaticIntField(__env, base, field);
    __expect_exception("java/lang/IllegalArgumentException");
    (*__env)->GetIntField(__env, NULL, field);
    __expect_exception("java/lang/NullPointerException");
    assert((*__env)->GetFieldID(__env, base, "invalidField", "V") == NULL);
    __expect_exception("java/lang/IllegalArgumentException");
}

void test_jni_dispatch(void)
{
    __env = jni_runtime_get_env();
    __test_descriptors();
    __test_strings();
    __test_services();
    __test_arrays();
    __test_inherited_fields();
    (*__env)->ThrowNew(__env, (*__env)->FindClass(__env, "java/lang/RuntimeException"), "describe clears exception");
    (*__env)->ExceptionDescribe(__env);
    assert(!(*__env)->ExceptionCheck(__env));
}
