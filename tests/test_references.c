#include <assert.h>
#include <dlfcn.h>
#include <string.h>

#include <jni/jni.h>
#include <jni/runtime.h>

static JNIEnv* __env;

static jni_runtime_stats_t __stats(void)
{
    jni_runtime_stats_t stats;
    jni_runtime_get_stats(&stats);
    return stats;
}

static void __expect_text(jobject value, const char* text)
{
    const char* actual = jni_runtime_get_string_utf(value);
    assert(actual != NULL && strcmp(actual, text) == 0);
}

static void __test_ownership(void)
{
    jstring local = (*__env)->NewStringUTF(__env, "retained");
    jobject global = (*__env)->NewGlobalRef(__env, local);
    jweak weak = (*__env)->NewWeakGlobalRef(__env, local);
    jobject alias = (*__env)->NewLocalRef(__env, global);
    assert(local != global && local != alias && alias != global);
    assert((*__env)->IsSameObject(__env, local, global));
    assert((*__env)->GetObjectRefType(__env, local) == JNILocalRefType);
    assert((*__env)->GetObjectRefType(__env, global) == JNIGlobalRefType);
    assert((*__env)->GetObjectRefType(__env, weak) == JNIWeakGlobalRefType);
    (*__env)->DeleteLocalRef(__env, local);
    (*__env)->DeleteLocalRef(__env, alias);
    __expect_text(global, "retained");
    assert(!(*__env)->IsSameObject(__env, weak, NULL));
    (*__env)->DeleteGlobalRef(__env, global);
    assert((*__env)->IsSameObject(__env, weak, NULL));
    assert((*__env)->NewLocalRef(__env, weak) == NULL);
    assert((*__env)->NewGlobalRef(__env, weak) == NULL);
    assert((*__env)->GetObjectRefType(__env, weak) == JNIWeakGlobalRefType);
    (*__env)->DeleteWeakGlobalRef(__env, weak);
}

static void __test_frames(void)
{
    assert((*__env)->PushLocalFrame(__env, 64) == JNI_OK);
    assert((*__env)->EnsureLocalCapacity(__env, 128) == JNI_OK);
    jstring result = (*__env)->NewStringUTF(__env, "frame result");
    jobject discarded = (*__env)->NewStringUTF(__env, "discarded");
    jweak weak = (*__env)->NewWeakGlobalRef(__env, discarded);
    for(int index = 0; index < 128; ++index) {
        assert((*__env)->NewLocalRef(__env, result) != NULL);
    }

    jobject survivor = (*__env)->PopLocalFrame(__env, result);
    __expect_text(survivor, "frame result");
    assert((*__env)->IsSameObject(__env, weak, NULL));
    (*__env)->DeleteWeakGlobalRef(__env, weak);
    (*__env)->DeleteLocalRef(__env, survivor);
    assert((*__env)->PushLocalFrame(__env, -1) == JNI_ERR);
    assert((*__env)->ExceptionCheck(__env));
    (*__env)->ExceptionClear(__env);
    assert((*__env)->EnsureLocalCapacity(__env, -1) == JNI_ERR);
    (*__env)->ExceptionClear(__env);
    jni_runtime_stats_t before = __stats();
    for(int index = 0; index < 3000; ++index) {
        assert((*__env)->PushLocalFrame(__env, 4) == JNI_OK);
        assert((*__env)->NewStringUTF(__env, "transient") != NULL);
        assert((*__env)->NewIntArray(__env, 256) != NULL);
        (*__env)->PopLocalFrame(__env, NULL);
    }

    jni_runtime_stats_t after = __stats();
    /* The first [I class is interned for the VM lifetime. */
    assert(after.objects <= before.objects + 1);
    assert(after.references == before.references);
    jclass helper = (*__env)->FindClass(__env, "org/cocos2dx/lib/Cocos2dxHelper");
    jmethodID language = (*__env)->GetStaticMethodID(__env, helper, "getCurrentLanguage", "()Ljava/lang/String;");
    before = __stats();
    for(int index = 0; index < 3000; ++index) {
        jobject value = (*__env)->CallStaticObjectMethod(__env, helper, language);
        (*__env)->DeleteLocalRef(__env, value);
    }

    after = __stats();
    assert(after.objects == before.objects && after.references == before.references);
    (*__env)->DeleteLocalRef(__env, helper);
}

static void __test_graphs(void)
{
    jclass clazz = (*__env)->FindClass(__env, "test/ReferenceGraph");
    jfieldID next = (*__env)->GetFieldID(__env, clazz, "next", "Ljava/lang/Object;");
    jfieldID shared = (*__env)->GetStaticFieldID(__env, clazz, "root", "Ljava/lang/Object;");
    jobject first = (*__env)->AllocObject(__env, clazz);
    jobject second = (*__env)->AllocObject(__env, clazz);
    jweak weak = (*__env)->NewWeakGlobalRef(__env, second);
    (*__env)->SetObjectField(__env, first, next, second);
    (*__env)->SetObjectField(__env, second, next, first);
    (*__env)->DeleteLocalRef(__env, second);
    assert(!(*__env)->IsSameObject(__env, weak, NULL));
    jobject from_field = (*__env)->GetObjectField(__env, first, next);
    assert((*__env)->IsSameObject(__env, from_field, weak));
    (*__env)->DeleteLocalRef(__env, from_field);
    (*__env)->SetStaticObjectField(__env, clazz, shared, first);
    (*__env)->DeleteLocalRef(__env, first);
    assert(!(*__env)->IsSameObject(__env, weak, NULL));
    (*__env)->SetStaticObjectField(__env, clazz, shared, NULL);
    assert((*__env)->IsSameObject(__env, weak, NULL));
    (*__env)->DeleteWeakGlobalRef(__env, weak);
    jclass object_class = (*__env)->FindClass(__env, "java/lang/Object");
    jobjectArray array = (*__env)->NewObjectArray(__env, 2, object_class, NULL);
    jobject child = (*__env)->NewStringUTF(__env, "array child");
    weak = (*__env)->NewWeakGlobalRef(__env, child);
    (*__env)->SetObjectArrayElement(__env, array, 0, child);
    (*__env)->SetObjectArrayElement(__env, array, 1, array);
    (*__env)->DeleteLocalRef(__env, child);
    assert(!(*__env)->IsSameObject(__env, weak, NULL));
    (*__env)->DeleteLocalRef(__env, array);
    assert((*__env)->IsSameObject(__env, weak, NULL));
    (*__env)->DeleteWeakGlobalRef(__env, weak);
    (*__env)->DeleteLocalRef(__env, object_class);
    (*__env)->DeleteLocalRef(__env, clazz);
}

static void __test_pins_and_exceptions(void)
{
    jstring string = (*__env)->NewStringUTF(__env, "borrowed bytes");
    jweak weak = (*__env)->NewWeakGlobalRef(__env, string);
    const char* bytes = (*__env)->GetStringUTFChars(__env, string, NULL);
    (*__env)->DeleteLocalRef(__env, string);
    assert(strcmp(bytes, "borrowed bytes") == 0);
    jobject alias = (*__env)->NewLocalRef(__env, weak);
    assert(alias != NULL);
    (*__env)->ReleaseStringUTFChars(__env, alias, bytes);
    (*__env)->DeleteLocalRef(__env, alias);
    assert((*__env)->IsSameObject(__env, weak, NULL));
    (*__env)->DeleteWeakGlobalRef(__env, weak);
    jintArray array = (*__env)->NewIntArray(__env, 8);
    weak = (*__env)->NewWeakGlobalRef(__env, array);
    jint* elements = (*__env)->GetPrimitiveArrayCritical(__env, array, NULL);
    (*__env)->DeleteLocalRef(__env, array);
    elements[3] = 123;
    alias = (*__env)->NewLocalRef(__env, weak);
    assert(alias != NULL);
    (*__env)->ReleasePrimitiveArrayCritical(__env, alias, elements, JNI_COMMIT);
    (*__env)->ReleasePrimitiveArrayCritical(__env, alias, elements, 0);
    (*__env)->DeleteLocalRef(__env, alias);
    assert((*__env)->IsSameObject(__env, weak, NULL));
    (*__env)->DeleteWeakGlobalRef(__env, weak);
    assert((*__env)->PushLocalFrame(__env, 8) == JNI_OK);
    jclass exception_class = (*__env)->FindClass(__env, "java/lang/RuntimeException");
    (*__env)->ThrowNew(__env, exception_class, "pending root");
    (*__env)->PopLocalFrame(__env, NULL);
    assert((*__env)->ExceptionCheck(__env));
    jthrowable exception = (*__env)->ExceptionOccurred(__env);
    weak = (*__env)->NewWeakGlobalRef(__env, exception);
    (*__env)->ExceptionClear(__env);
    assert(!(*__env)->IsSameObject(__env, weak, NULL));
    (*__env)->DeleteLocalRef(__env, exception);
    assert((*__env)->IsSameObject(__env, weak, NULL));
    (*__env)->DeleteWeakGlobalRef(__env, weak);
    int external = 42;
    jobject buffer = (*__env)->NewDirectByteBuffer(__env, &external, sizeof(external));
    (*__env)->DeleteLocalRef(__env, buffer);
    assert(external == 42);
}

static void __test_callback_frames(const char* path)
{
    void* game = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    assert(game != NULL);
    int* allocate = dlsym(game, "mock_allocate_render");
    assert(allocate != NULL);
    *allocate = 1;
    dynalib_t library = {.image = game};

    assert(bridge_jni_cocos_start(&library, 960, 544));
    jni_runtime_stats_t before = __stats();
    for(int index = 0; index < 3000; ++index) {
        bridge_jni_cocos_render();
    }

    jni_runtime_stats_t after = __stats();
    assert(before.objects == after.objects && before.references == after.references);
    bridge_jni_cocos_stop();
    dlclose(game);
}

void test_jni_references(const char* path)
{
    __env = jni_runtime_get_env();
    __test_ownership();
    __test_frames();
    __test_graphs();
    __test_pins_and_exceptions();
    __test_callback_frames(path);
    assert(!(*__env)->ExceptionCheck(__env));
}
