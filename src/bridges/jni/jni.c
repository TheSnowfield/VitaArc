#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <config.h>
#include <loader.h>
#include <logcat/logcat.h>
#include <libc/posix.h>
#include <android/asset_cache.h>

#include <abi/calling.h>

#include "jni.h"
#include "runtime.h"
#include "cocos.h"

typedef void (*jni_void_call_t)(JNIEnv*, jobject);
typedef void (*jni_native_init_t)(JNIEnv*, jobject, jint, jint);
typedef void (*jni_native_set_context_t)(JNIEnv*, jobject, jobject, jobject);
typedef void (*jni_sdkbox_init_t)(JNIEnv*, jclass, jobject, jobject);
typedef void (*jni_string_call_t)(JNIEnv*, jobject, jstring);
typedef void (*jni_bitmap_call_t)(JNIEnv*, jclass, jint, jint, jbyteArray);
typedef void VITAARC_SOFTFP (*jni_touch_point_t)(JNIEnv*, jobject, jint, jfloat, jfloat);
typedef void (*jni_touch_move_t)(JNIEnv*, jobject, jintArray, jfloatArray, jfloatArray);
typedef void VITAARC_SOFTFP (*jni_touch_point_v7_t)(JNIEnv*, jobject, jint, jfloat, jfloat, jfloat, jlong);
typedef void (*jni_touch_move_v7_t)(JNIEnv*, jobject, jintArray, jfloatArray, jfloatArray, jfloatArray, jlong);
typedef jboolean (*jni_key_event_t)(JNIEnv*, jobject, jint, jboolean);

typedef struct cocos_jni_exports_t {
    jni_void_call_t native_render;
    jni_void_call_t native_pause;
    jni_void_call_t native_resume;
    jni_touch_point_t touch_begin;
    jni_touch_point_t touch_end;
    jni_touch_move_t touch_move;
    jni_touch_point_v7_t touch_begin_v7;
    jni_touch_point_v7_t touch_end_v7;
    jni_touch_move_v7_t touch_move_v7;
    bool touch_v7;
    jni_key_event_t key_event;
    jni_void_call_t expansion_complete;
    bool expansion_delivered;
    jni_void_call_t startup;
    bool startup_delivered;
    jni_bitmap_call_t bitmap;
} cocos_jni_exports_t;

typedef struct touch_arrays_t {
    jintArray ids;
    jfloatArray xs;
    jfloatArray ys;
    jfloatArray pressure;
} touch_arrays_t;

static cocos_jni_exports_t __cocos;
static touch_arrays_t __touch_arrays[9];
static JNIEnv* __env;
static bool __ready;

static bool __app_version(char version[64])
{
    bool valid = vita_version_read_metadata(version, 64);
    if(!valid)
        log_e(TAG, "Invalid APK version metadata in %s", PATH_TO_PACKAGE);
    return valid;
}

bool bridge_jni_cocos_is_ready(void)
{
    return __ready;
}

static bool __check_exception(const char* stage)
{
    if(!(*__env)->ExceptionCheck(__env)) {
        return true;
    }

    log_e(TAG, "JNI exception during %s", stage);
    jthrowable exception = (*__env)->ExceptionOccurred(__env);
    (*__env)->ExceptionDescribe(__env);
    (*__env)->Throw(__env, exception);
    (*__env)->DeleteLocalRef(__env, exception);
    return false;
}

bool bridge_jni_cocos_complete_expansion(void)
{
    if(!__env || jni_runtime_get_env() != __env || !__cocos.expansion_complete) {
        return false;
    }

    if(!__cocos.expansion_delivered) {
        __cocos.expansion_complete(__env, NULL);
        if(!__check_exception("expansion setup")) {
            return false;
        }

        __cocos.expansion_delivered = true;
    }

    return true;
}

bool bridge_jni_cocos_notify_startup(void)
{
    if(!__env || !__cocos.startup) {
        return false;
    }

    if(__cocos.startup_delivered) {
        return true;
    }

    __cocos.startup(__env, jni_cocos_get_context());
    if(!__check_exception("native startup notification")) {
        return false;
    }

    __cocos.startup_delivered = true;
    return true;
}

bool bridge_jni_cocos_publish_bitmap(int width, int height, const uint8_t* rgba)
{
    JNIEnv* env = jni_runtime_get_env();
    if(!env || !__cocos.bitmap || !rgba || width < 1 || height < 1 || width > 2048 || height > 2048 ||
        (*env)->ExceptionCheck(env))
        return false;

    jsize length = width * height * 4;
    jbyteArray pixels = (*env)->NewByteArray(env, length);
    jclass clazz = (*env)->FindClass(env, "org/cocos2dx/lib/Cocos2dxBitmap");
    if(pixels && clazz) {
        (*env)->SetByteArrayRegion(env, pixels, 0, length, (const jbyte*)rgba);
        if(!(*env)->ExceptionCheck(env))
            __cocos.bitmap(env, clazz, width, height, pixels);
    }

    bool success = pixels && clazz && !(*env)->ExceptionCheck(env);
    (*env)->DeleteLocalRef(env, pixels);
    (*env)->DeleteLocalRef(env, clazz);
    return success;
}

void bridge_jni_cocos_stop(void)
{
    a_asset_cache_clear();
    __ready = false;
    memset(&__cocos, 0, sizeof(__cocos));
    if(__env) {
        for(size_t index = 0; index < sizeof(__touch_arrays) / sizeof(__touch_arrays[0]); ++index) {
            (*__env)->DeleteGlobalRef(__env, __touch_arrays[index].ids);
            (*__env)->DeleteGlobalRef(__env, __touch_arrays[index].xs);
            (*__env)->DeleteGlobalRef(__env, __touch_arrays[index].ys);
            (*__env)->DeleteGlobalRef(__env, __touch_arrays[index].pressure);
        }
    }

    memset(__touch_arrays, 0, sizeof(__touch_arrays));
    jni_runtime_clear_natives();
}

typedef struct {
    const char* prefix;
    const char* clazz;
} jni_export_t;

typedef struct {
    const char* name;
    const char* sig;
} jni_proto_t;

static const jni_export_t __classes[] = {
    {"Java_org_cocos2dx_lib_Cocos2dxHelper_", "org/cocos2dx/lib/Cocos2dxHelper"},
    {"Java_org_cocos2dx_lib_Cocos2dxRenderer_", "org/cocos2dx/lib/Cocos2dxRenderer"},
    {"Java_com_sdkbox_plugin_SDKBox_", "com/sdkbox/plugin/SDKBox"},
    {"Java_org_cocos2dx_cpp_AppActivity_", "org/cocos2dx/cpp/AppActivity"},
    {"Java_low_moe_AppActivity_", "low/moe/AppActivity"},
    {"Java_org_cocos2dx_lib_Cocos2dxBitmap_", "org/cocos2dx/lib/Cocos2dxBitmap"},
};

static const jni_proto_t __signatures[] = {
    {"nativeSetContext", "(Landroid/content/Context;Landroid/content/res/AssetManager;)V"},
    {"nativeInit", "(II)V"},
    {"nativeRender", "()V"},
    {"nativeOnPause", "()V"},
    {"nativeOnResume", "()V"},
    {"nativeTouchesBegin", "(IFF)V"},
    {"nativeTouchesBegin", "(IFFFJ)V"},
    {"nativeTouchesEnd", "(IFF)V"},
    {"nativeTouchesEnd", "(IFFFJ)V"},
    {"nativeTouchesMove", "([I[F[F)V"},
    {"nativeTouchesMove", "([I[F[F[FJ)V"},
    {"nativeKeyEvent", "(IZ)Z"},
    {"initJVMPlatformUtils", "()V"},
    {"initJVMAnalytics", "()V"},
    {"notifyExpansionCheckComplete", "()V"},
    {"notifyNativeStartupComplete", "()V"},
    {"nativeSetApkPath", "(Ljava/lang/String;)V"},
    {"setObbPath", "(Ljava/lang/String;)V"},
    {"setDeviceId", "(Ljava/lang/String;)V"},
    {"setAppVersion", "(Ljava/lang/String;)V"},
    {"nativeInitBitmapDC", "(II[B)V"},
};

static void* __registered_export(const char* name)
{

    for(size_t index = 0; index < sizeof(__classes) / sizeof(__classes[0]); ++index) {
        size_t length = strlen(__classes[index].prefix);
        if(strncmp(name, __classes[index].prefix, length) != 0) {
            continue;
        }

        const char* method_name = name + length;
        if(strcmp(__classes[index].clazz, "com/sdkbox/plugin/SDKBox") == 0) {
            if(strcmp(method_name, "nativeInit") != 0) {
                return NULL;
            }

            return jni_runtime_find_native(
                __classes[index].clazz, method_name, "(Ljava/lang/Object;Ljava/lang/ClassLoader;)V");
        }

        for(size_t member = 0; member < sizeof(__signatures) / sizeof(__signatures[0]); ++member) {
            if(strcmp(method_name, __signatures[member].name) == 0) {
                void* call = jni_runtime_find_native(__classes[index].clazz, method_name, __signatures[member].sig);
                if(call) {
                    return call;
                }
            }
        }
    }

    return NULL;
}

static void* __resolve_export(dynalib_t* library, const char* name)
{
    void* registered = __registered_export(name);
    if(registered) {
        return registered;
    }

    void* proc = loader_get_proc_address(library, name);
    if(proc) {
        return proc;
    }

    const char* prefix = "Java_org_cocos2dx_cpp_AppActivity_";
    size_t length = strlen(prefix);
    if(strncmp(name, prefix, length)) {
        return NULL;
    }

    char alias[160];
    if(snprintf(alias, sizeof(alias), "Java_low_moe_AppActivity_%s", name + length) >= (int)sizeof(alias)) {
        return NULL;
    }

    registered = __registered_export(alias);
    return registered ? registered : loader_get_proc_address(library, alias);
}

static void* __required_export(dynalib_t* library, const char* name)
{
    void* proc = __resolve_export(library, name);
    if(!proc) {
        log_e(TAG, "Required JNI export is missing: %s", name);
    }

    return proc;
}

static bool __call_string_export(dynalib_t* library, const char* name, const char* value)
{
    jni_string_call_t call = (jni_string_call_t)__resolve_export(library, name);
    if(!call) {
        return true;
    }

    jstring argument = jni_runtime_new_string_utf(value);
    if(!argument) {
        return false;
    }

    call(__env, NULL, argument);
    (*__env)->DeleteLocalRef(__env, argument);
    return __check_exception(name);
}

static bool __initialize_sdkbox(dynalib_t* library, jobject context)
{
    jni_sdkbox_init_t initialize =
        (jni_sdkbox_init_t)__resolve_export(library, "Java_com_sdkbox_plugin_SDKBox_nativeInit");
    if(!initialize) {
        return true;
    }

    bool initialized = false;
    jclass context_class = (*__env)->GetObjectClass(__env, context);
    jclass sdkbox_class = NULL;
    jobject class_loader = NULL;
    if(!context_class) {
        goto done;
    }

    jmethodID get_loader = (*__env)->GetMethodID(__env, context_class, "getClassLoader", "()Ljava/lang/ClassLoader;");
    if(!get_loader) {
        goto done;
    }

    class_loader = (*__env)->CallObjectMethod(__env, context, get_loader);
    if(!class_loader || (*__env)->ExceptionCheck(__env)) {
        goto done;
    }

    sdkbox_class = (*__env)->FindClass(__env, "com/sdkbox/plugin/SDKBox");
    if(!sdkbox_class) {
        goto done;
    }

    // Establish SDKBOX's VM/activity/loader before native renderer startup.
    initialize(__env, sdkbox_class, context, class_loader);
    initialized = __check_exception("SDKBox.nativeInit");
done:
    (*__env)->DeleteLocalRef(__env, class_loader);
    (*__env)->DeleteLocalRef(__env, sdkbox_class);
    (*__env)->DeleteLocalRef(__env, context_class);
    return initialized;
}

static bool __start(dynalib_t* library, uint32_t screen_width, uint32_t screen_height)
{
    bridge_jni_cocos_stop();
    __env = jni_runtime_get_env();
    if(!library || !screen_width || !screen_height) {
        return false;
    }

    if(!__check_exception("startup entry")) {
        return false;
    }

    bool modern = loader_get_proc_address(library, "Java_low_moe_AppActivity_initJVMPlatformUtils") != NULL;
    __cocos.touch_v7 = modern;
    jni_cocos_set_activity(modern);

    JNI_OnLoad on_load = (JNI_OnLoad)__required_export(library, "JNI_OnLoad");
    if(!on_load) {
        return false;
    }

    jint version = on_load(jni_runtime_get_java_vm(), NULL);
    if(!__check_exception("JNI_OnLoad")) {
        return false;
    }

    if(version != JNI_VERSION_1_1 && version != JNI_VERSION_1_2 && version != JNI_VERSION_1_4 &&
        version != JNI_VERSION_1_6) {
        log_e(TAG, "JNI_OnLoad returned unsupported version 0x%08X", version);
        return false;
    }

    log_i(TAG, "JNI_OnLoad returned 0x%08X", version);
    jni_native_set_context_t native_set_context =
        (jni_native_set_context_t)__required_export(library, "Java_org_cocos2dx_lib_Cocos2dxHelper_nativeSetContext");
    if(!native_set_context) {
        return false;
    }

    jni_native_init_t native_init =
        (jni_native_init_t)__required_export(library, "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit");
    if(!native_init) {
        return false;
    }

    jni_void_call_t init_platform =
        (jni_void_call_t)__required_export(library, "Java_org_cocos2dx_cpp_AppActivity_initJVMPlatformUtils");
    if(!init_platform) {
        return false;
    }

    jni_void_call_t init_analytics =
        (jni_void_call_t)__required_export(library, "Java_org_cocos2dx_cpp_AppActivity_initJVMAnalytics");
    if(!init_analytics) {
        return false;
    }

    __cocos.native_render =
        (jni_void_call_t)__required_export(library, "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeRender");
    if(!__cocos.native_render) {
        return false;
    }

    __cocos.native_pause =
        (jni_void_call_t)__required_export(library, "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnPause");
    if(!__cocos.native_pause) {
        return false;
    }

    __cocos.native_resume =
        (jni_void_call_t)__required_export(library, "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnResume");
    if(!__cocos.native_resume) {
        return false;
    }

    void* touch_begin = __required_export(library, "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeTouchesBegin");
    if(!touch_begin) {
        return false;
    }

    void* touch_end = __required_export(library, "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeTouchesEnd");
    if(!touch_end) {
        return false;
    }

    void* touch_move = __required_export(library, "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeTouchesMove");
    if(!touch_move) {
        return false;
    }

    if(modern) {
        __cocos.touch_begin_v7 = touch_begin;
        __cocos.touch_end_v7 = touch_end;
        __cocos.touch_move_v7 = touch_move;
    } else {
        __cocos.touch_begin = touch_begin;
        __cocos.touch_end = touch_end;
        __cocos.touch_move = touch_move;
    }

    __cocos.key_event =
        (jni_key_event_t)__required_export(library, "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeKeyEvent");
    if(!__cocos.key_event) {
        return false;
    }

    jobject context = jni_cocos_get_context();
    jobject assets = jni_cocos_get_asset_manager();
    if(!context || !assets) {
        return false;
    }

    native_set_context(__env, NULL, context, assets);
    if(!__check_exception("nativeSetContext")) {
        return false;
    }

    if(!__initialize_sdkbox(library, context)) {
        return false;
    }

    if(!__call_string_export(library, "Java_org_cocos2dx_lib_Cocos2dxHelper_nativeSetApkPath", PATH_TO_PACKAGE)) {
        return false;
    }

    if(!__call_string_export(library, "Java_org_cocos2dx_cpp_AppActivity_setObbPath", PATH_TO_OBB)) {
        return false;
    }

    if(!__call_string_export(library, "Java_org_cocos2dx_cpp_AppActivity_setDeviceId", "VitaArc-PSVita")) {
        return false;
    }

    char app_version[64];
    __cocos.bitmap =
        (jni_bitmap_call_t)__resolve_export(library, "Java_org_cocos2dx_lib_Cocos2dxBitmap_nativeInitBitmapDC");
    if(!__app_version(app_version) ||
        !__call_string_export(library, "Java_org_cocos2dx_cpp_AppActivity_setAppVersion", app_version)) {
        return false;
    }

    log_i(TAG, "Configured APK version %s", app_version);

    init_platform(__env, NULL);
    if(!__check_exception("initJVMPlatformUtils")) {
        return false;
    }

    init_analytics(__env, NULL);
    if(!__check_exception("initJVMAnalytics")) {
        return false;
    }

    __cocos.expansion_complete =
        (jni_void_call_t)__resolve_export(library, "Java_org_cocos2dx_cpp_AppActivity_notifyExpansionCheckComplete");
    if(__cocos.expansion_complete && !bridge_jni_cocos_complete_expansion()) {
        return false;
    }

    __cocos.startup = (jni_void_call_t)__resolve_export(library, "Java_low_moe_AppActivity_notifyNativeStartupComplete");

    native_init(__env, NULL, (jint)screen_width, (jint)screen_height);
    if(!__check_exception("nativeInit")) {
        return false;
    }

    __ready = true;
    log_i(TAG, "Cocos initialized at %ux%u", screen_width, screen_height);
    return true;
}

bool bridge_jni_cocos_start(dynalib_t* library, uint32_t screen_width, uint32_t screen_height)
{
    JNIEnv* env = jni_runtime_get_env();
    if(!env || (*env)->PushLocalFrame(env, 64) != JNI_OK) {
        return false;
    }

    bool started = __start(library, screen_width, screen_height);
    (*env)->PopLocalFrame(env, NULL);
    if(!started) {
        bridge_jni_cocos_stop();
    }

    return started;
}

static bool __callback_ready(void)
{
    if(!__ready) {
        return false;
    }

    if(!__check_exception("callback entry")) {
        bridge_jni_cocos_stop();
        return false;
    }

    if((*__env)->PushLocalFrame(__env, 32) != JNI_OK) {
        __check_exception("callback local frame");
        bridge_jni_cocos_stop();
        return false;
    }

    return true;
}

static bool __callback_complete(const char* stage)
{
    bool succeeded = __check_exception(stage);
    (*__env)->PopLocalFrame(__env, NULL);
    if(succeeded) {
        return true;
    }

    bridge_jni_cocos_stop();
    return false;
}

static void __call_callback(jni_void_call_t call, const char* stage)
{
    if(!call || !__callback_ready()) {
        return;
    }

    call(__env, NULL);
    __callback_complete(stage);
}

void bridge_jni_cocos_render(void)
{
    __call_callback(__cocos.native_render, "nativeRender");
}

void bridge_jni_cocos_pause(void)
{
    __call_callback(__cocos.native_pause, "nativeOnPause");
}

void bridge_jni_cocos_resume(void)
{
    __call_callback(__cocos.native_resume, "nativeOnResume");
}

static jlong __touch_time(void)
{
    struct timespec time;
    if(bridge_clock_gettime(1, &time)) {
        return 0;
    }

    return (jlong)time.tv_sec * 1000 + time.tv_nsec / 1000000;
}

static void __touch_point(
    jni_touch_point_t call, jni_touch_point_v7_t modern, int32_t id, float x, float y, const char* stage)
{
    if(!call && !modern) {
        return;
    }

    if(!__callback_ready()) {
        return;
    }

    if(modern) {
        modern(__env, NULL, id, x, y, 1.0f, __touch_time());
    } else {
        call(__env, NULL, id, x, y);
    }

    __callback_complete(stage);
}

void bridge_jni_cocos_touch_begin(int32_t id, float x, float y)
{
    __touch_point(__cocos.touch_begin, __cocos.touch_begin_v7, id, x, y, "nativeTouchesBegin");
}

void bridge_jni_cocos_touch_end(int32_t id, float x, float y)
{
    __touch_point(__cocos.touch_end, __cocos.touch_end_v7, id, x, y, "nativeTouchesEnd");
}

static jarray __retain_array(jarray array)
{
    jarray reference = (*__env)->NewGlobalRef(__env, array);
    (*__env)->DeleteLocalRef(__env, array);
    return reference;
}

static bool __prepare_touch_arrays(touch_arrays_t* arrays, jsize count)
{
    if(!arrays->ids) {
        arrays->ids = __retain_array((*__env)->NewIntArray(__env, count));
    }

    if(!arrays->xs) {
        arrays->xs = __retain_array((*__env)->NewFloatArray(__env, count));
    }

    if(!arrays->ys) {
        arrays->ys = __retain_array((*__env)->NewFloatArray(__env, count));
    }

    if(!__cocos.touch_v7) {
        return arrays->ids && arrays->xs && arrays->ys;
    }

    if(!arrays->pressure) {
        const jfloat pressure[8] = {1, 1, 1, 1, 1, 1, 1, 1};
        arrays->pressure = __retain_array((*__env)->NewFloatArray(__env, count));
        if(arrays->pressure) {
            (*__env)->SetFloatArrayRegion(__env, arrays->pressure, 0, count, pressure);
        }
    }

    return arrays->ids && arrays->xs && arrays->ys && arrays->pressure;
}

void bridge_jni_cocos_touch_move(const int32_t* ids, const float* xs, const float* ys, uint32_t count)
{
    if(!ids || !xs || !ys || count == 0 || count > 8) {
        return;
    }

    if(!__cocos.touch_move && !__cocos.touch_move_v7) {
        return;
    }

    if(!__callback_ready()) {
        return;
    }

    touch_arrays_t* arrays = &__touch_arrays[count];
    if(!__prepare_touch_arrays(arrays, (jsize)count)) {
        __callback_complete("touch array allocation");
        return;
    }

    (*__env)->SetIntArrayRegion(__env, arrays->ids, 0, (jsize)count, (const jint*)ids);
    (*__env)->SetFloatArrayRegion(__env, arrays->xs, 0, (jsize)count, (const jfloat*)xs);
    (*__env)->SetFloatArrayRegion(__env, arrays->ys, 0, (jsize)count, (const jfloat*)ys);
    if((*__env)->ExceptionCheck(__env)) {
        __callback_complete("touch array update");
        return;
    }

    if(__cocos.touch_move_v7) {
        __cocos.touch_move_v7(__env, NULL, arrays->ids, arrays->xs, arrays->ys, arrays->pressure, __touch_time());
    } else {
        __cocos.touch_move(__env, NULL, arrays->ids, arrays->xs, arrays->ys);
    }

    __callback_complete("nativeTouchesMove");
}

bool bridge_jni_cocos_key_event(int32_t key_code, bool pressed)
{
    if(!__cocos.key_event || !__callback_ready()) {
        return false;
    }

    jboolean handled = __cocos.key_event(__env, NULL, (jint)key_code, pressed ? JNI_TRUE : JNI_FALSE);
    return __callback_complete("nativeKeyEvent") && handled == JNI_TRUE;
}
