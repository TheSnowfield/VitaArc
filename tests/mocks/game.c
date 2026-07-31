#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define JNI_OnLoad jni_on_load_t
#include <jni/aosp/jni.h>
#undef JNI_OnLoad

int mock_render_count;
int mock_pause_count;
int mock_resume_count;
int mock_touch_count;
int mock_stage;
int mock_fail_on_load;
int mock_fail_init;
int mock_register_render;
int mock_fail_callback;
int mock_callback_count;
int mock_allocate_render;
int mock_sdkbox_count;
int mock_fail_sdkbox;
int mock_register_sdkbox;
int mock_expansion_count;
int mock_bitmap_count;
int mock_bitmap_fail;

void Java_org_cocos2dx_lib_Cocos2dxBitmap_nativeInitBitmapDC(
    JNIEnv* env, jclass type, jint width, jint height, jbyteArray pixels)
{
    jclass expected = (*env)->FindClass(env, "org/cocos2dx/lib/Cocos2dxBitmap");
    assert((*env)->IsSameObject(env, type, expected));
    (*env)->DeleteLocalRef(env, expected);
    assert(width > 0 && height > 0 && (*env)->GetArrayLength(env, pixels) == width * height * 4);
    jbyte* bytes = (*env)->GetByteArrayElements(env, pixels, NULL);
    assert(bytes);
    unsigned nonzero = 0;
    for(int index = 3; index < width * height * 4; index += 4)
        nonzero += bytes[index] != 0;
    assert(nonzero > 30);
    (*env)->ReleaseByteArrayElements(env, pixels, bytes, JNI_ABORT);
    ++mock_bitmap_count;
    if(mock_bitmap_fail) {
        expected = (*env)->FindClass(env, "java/lang/RuntimeException");
        (*env)->ThrowNew(env, expected, "bitmap callback failure");
        (*env)->DeleteLocalRef(env, expected);
    }
}

void Java_org_cocos2dx_cpp_AppActivity_setAppVersion(JNIEnv* env, jobject type, jstring version)
{
    (void)type;
    const char* expected = getenv("VITAARC_TEST_APK_VERSION");
    const char* text = (*env)->GetStringUTFChars(env, version, NULL);
    assert(text && strcmp(text, expected ? expected : "1.6.1c") == 0);
    (*env)->ReleaseStringUTFChars(env, version, text);
}

static void __sdkbox_init(JNIEnv* env, jclass sdkbox, jobject context, jobject loader)
{
    assert(mock_stage == 2);
    jclass expected = (*env)->FindClass(env, "com/sdkbox/plugin/SDKBox");
    assert((*env)->IsSameObject(env, sdkbox, expected));
    (*env)->DeleteLocalRef(env, expected);
    expected = (*env)->FindClass(env, "org/cocos2dx/cpp/AppActivity");
    assert((*env)->IsInstanceOf(env, context, expected));
    (*env)->DeleteLocalRef(env, expected);
    expected = (*env)->FindClass(env, "java/lang/ClassLoader");
    assert((*env)->IsInstanceOf(env, loader, expected));
    jmethodID load_class = (*env)->GetMethodID(env, expected, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring name = (*env)->NewStringUTF(env, "com.sdkbox.plugin.SDKBox");
    jobject loaded = (*env)->CallObjectMethod(env, loader, load_class, name);
    assert((*env)->IsSameObject(env, loaded, sdkbox));
    (*env)->DeleteLocalRef(env, loaded);
    (*env)->DeleteLocalRef(env, name);
    (*env)->DeleteLocalRef(env, expected);
    ++mock_sdkbox_count;
    if(mock_fail_sdkbox) {
        expected = (*env)->FindClass(env, "java/lang/RuntimeException");
        (*env)->ThrowNew(env, expected, "mock SDKBOX initialization failure");
        (*env)->DeleteLocalRef(env, expected);
    }
}

void Java_com_sdkbox_plugin_SDKBox_nativeInit(JNIEnv* env, jclass sdkbox, jobject context, jobject loader)
{
    __sdkbox_init(env, sdkbox, context, loader);
}

static void __callback(JNIEnv* env, int stage)
{
    ++mock_callback_count;
    if(mock_fail_callback == stage) {
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), "mock callback failure");
    }
}

static void __registered_render(JNIEnv* env, jobject object)
{
    assert(mock_stage == 5);
    mock_render_count += 10;
    __callback(env, 1);
}

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env = NULL;
    assert((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) == JNI_OK);
    assert(env != NULL);
    if(mock_register_render) {
        jclass clazz = (*env)->FindClass(env, "org/cocos2dx/lib/Cocos2dxRenderer");
        JNINativeMethod method = {"nativeRender", "()V", __registered_render};

        assert((*env)->RegisterNatives(env, clazz, &method, 1) == JNI_OK);
    }

    if(mock_register_sdkbox) {
        jclass clazz = (*env)->FindClass(env, "com/sdkbox/plugin/SDKBox");
        JNINativeMethod method = {"nativeInit", "(Ljava/lang/Object;Ljava/lang/ClassLoader;)V", __sdkbox_init};

        assert((*env)->RegisterNatives(env, clazz, &method, 1) == JNI_OK);
        (*env)->DeleteLocalRef(env, clazz);
    }

    mock_stage = 1;
    return mock_fail_on_load ? JNI_ERR : JNI_VERSION_1_6;
}

void Java_org_cocos2dx_lib_Cocos2dxHelper_nativeSetContext(JNIEnv* env, jobject object, jobject context, jobject assets)
{
    assert(mock_stage == 1);
    assert(context != NULL);
    assert(assets != NULL);
    jclass clazz = (*env)->GetObjectClass(env, context);
    jmethodID method = (*env)->GetMethodID(env, clazz, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject loader = (*env)->CallObjectMethod(env, context, method);
    assert(loader != NULL);
    method = (*env)->GetMethodID(
        env, (*env)->GetObjectClass(env, loader), "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring name = (*env)->NewStringUTF(env, "org.cocos2dx.lib.Cocos2dxHelper");
    clazz = (*env)->CallObjectMethod(env, loader, method, name);
    assert((*env)->IsSameObject(env, clazz, (*env)->FindClass(env, "org/cocos2dx/lib/Cocos2dxHelper")));
    mock_stage = 2;
}

void Java_org_cocos2dx_cpp_AppActivity_initJVMPlatformUtils(JNIEnv* env, jobject object)
{
    assert(mock_stage == 2);
    mock_stage = 3;
}

void Java_org_cocos2dx_cpp_AppActivity_initJVMAnalytics(JNIEnv* env, jobject object)
{
    assert(mock_stage == 3);
    mock_stage = 4;
}

void Java_org_cocos2dx_cpp_AppActivity_notifyExpansionCheckComplete(JNIEnv* env, jobject object)
{
    assert(mock_stage == 4);
    ++mock_expansion_count;
}

void Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit(JNIEnv* env, jobject object, jint width, jint height)
{
    assert(mock_stage == 4);
    assert(width == 960 && height == 544);
    jclass logger = (*env)->FindClass(env, "com/sdkbox/plugin/SdkboxLog");
    jmethodID set_level = (*env)->GetStaticMethodID(env, logger, "setDefaultDebugLevel", "(I)V");
    (*env)->CallStaticVoidMethod(env, logger, set_level, -17);
    assert(!(*env)->ExceptionCheck(env));
    jfieldID level = (*env)->GetStaticFieldID(env, logger, "_defaultLevel", "I");
    assert((*env)->GetStaticIntField(env, logger, level) == -17);
    (*env)->DeleteLocalRef(env, logger);
    if(mock_fail_init) {
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), "mock initialization failure");
        return;
    }

    int expansions = mock_expansion_count;
    assert(expansions > 0);
    jclass activity = (*env)->FindClass(env, "org/cocos2dx/cpp/AppActivity");
    jmethodID finished = (*env)->GetStaticMethodID(env, activity, "startSceneLoadFinished", "()V");
    (*env)->CallStaticVoidMethod(env, activity, finished);
    if((*env)->ExceptionCheck(env)) {
        (*env)->DeleteLocalRef(env, activity);
        return;
    }

    (*env)->CallStaticVoidMethod(env, activity, finished);
    assert(!(*env)->ExceptionCheck(env) && mock_expansion_count == expansions);
    jfieldID checked = (*env)->GetStaticFieldID(env, activity, "expansionCheckPerformed", "Z");
    assert((*env)->GetStaticBooleanField(env, activity, checked) == JNI_TRUE);
    (*env)->DeleteLocalRef(env, activity);
    mock_stage = 5;
}

void Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeRender(JNIEnv* env, jobject object)
{
    assert(mock_stage == 5);
    ++mock_render_count;
    if(mock_allocate_render) {
        assert((*env)->NewStringUTF(env, "temporary render object") != NULL);
    }

    __callback(env, 1);
}

void Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnPause(JNIEnv* env, jobject object)
{
    ++mock_pause_count;
    __callback(env, 2);
}

void Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnResume(JNIEnv* env, jobject object)
{
    ++mock_resume_count;
    jclass renderer = (*env)->FindClass(env, "org/cocos2dx/lib/Cocos2dxRenderer");
    jmethodID interval = (*env)->GetStaticMethodID(env, renderer, "setAnimationInterval", "(F)V");
    (*env)->CallStaticVoidMethod(env, renderer, interval, 1.0 / 60.0);
    (*env)->DeleteLocalRef(env, renderer);
    __callback(env, 3);
}

void Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeTouchesBegin(JNIEnv* env, jobject object, jint id, jfloat x, jfloat y)
{
    assert(id == 7 && x == 12.5f && y == 30.25f);
    ++mock_touch_count;
    __callback(env, 4);
}

void Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeTouchesEnd(JNIEnv* env, jobject object, jint id, jfloat x, jfloat y)
{
    assert(id == 7 && x == 12.5f && y == 30.25f);
    ++mock_touch_count;
    __callback(env, 5);
}

void Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeTouchesMove(
    JNIEnv* env, jobject object, jintArray ids, jfloatArray xs, jfloatArray ys)
{
    assert((*env)->GetArrayLength(env, ids) == 2);
    jint actual_ids[2];
    jfloat actual_xs[2], actual_ys[2];
    (*env)->GetIntArrayRegion(env, ids, 0, 2, actual_ids);
    (*env)->GetFloatArrayRegion(env, xs, 0, 2, actual_xs);
    (*env)->GetFloatArrayRegion(env, ys, 0, 2, actual_ys);
    assert(actual_ids[0] == 7 && actual_ids[1] == 8);
    assert(actual_xs[0] == 12.5f && actual_xs[1] == 40.0f);
    assert(actual_ys[0] == 30.25f && actual_ys[1] == 50.0f);
    ++mock_touch_count;
    __callback(env, 6);
}

jboolean Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeKeyEvent(JNIEnv* env, jobject object, jint key, jboolean pressed)
{
    assert(key == 4);
    __callback(env, 7);
    return pressed;
}
