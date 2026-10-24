#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <psp2/kernel/rng.h>
#ifdef __vita__
#include <psp2/io/devctl.h>
#else
#include <sys/statvfs.h>
#endif

#include <config.h>
#include <logcat/logcat.h>
#include <platform/timing.h>
#include "cocos.h"
#include "preferences.h"
#include "runtime.h"
#include "thread.h"
#include "jni.h"
#include "bitmap.h"
#include "descriptor.h"
#include "names.h"

#define HELPER_CLASS "org/cocos2dx/lib/Cocos2dxHelper"
#define ACTIVITY_CLASS "org/cocos2dx/cpp/AppActivity"
#define STRING "Ljava/lang/String;"

typedef enum cocos_operation {
    COCOS_GET_PREFERENCE,
    COCOS_SET_PREFERENCE,
    COCOS_COMMIT_PREFERENCE,
    COCOS_DELETE_PREFERENCE,
    COCOS_STRING,
    COCOS_WRITABLE_PATH,
    COCOS_GUID,
    COCOS_DPI,
    COCOS_ASSET_MANAGER,
    COCOS_CONTEXT,
    COCOS_LOG_LEVEL,
    COCOS_EXPANSION_CHECK,
    COCOS_STARTUP,
    COCOS_STORAGE,
    COCOS_REFRESH_RATE,
    COCOS_ANDROID_VERSION,
    COCOS_ANIMATION_INTERVAL,
    COCOS_BREADCRUMB,
    COCOS_BITMAP,
    COCOS_ANALYTICS
} cocos_operation_t;

typedef struct cocos_method {
    const char* class_name;
    const char* name;
    const char* signature;
    cocos_operation_t operation;
    char value_type;
    const char* text;
} cocos_method_t;

#define PREFERENCE_METHODS(name, sig, type)                                                                            \
    {HELPER_CLASS, "get" name "ForKey", "(" STRING sig ")" sig, COCOS_GET_PREFERENCE, type, NULL},                     \
    {                                                                                                                  \
        HELPER_CLASS, "set" name "ForKey", "(" STRING sig ")V", COCOS_SET_PREFERENCE, type, NULL                       \
    }

static const cocos_method_t __methods[] = {
/* These APK Java methods forward statistics to Android analytics SDKs.
 * Vita retains an explicit local event sink; none performs a purchase,
 * login, score submission or network request. Exact descriptors remain
 * required, as for the other platform services. */
#define ANALYTICS_METHOD(name, sig) {"org/cocos2dx/cpp/AnalyticsManager", name, sig, COCOS_ANALYTICS, 0, NULL}

    ANALYTICS_METHOD("logEvent", "(" STRING ")V"),
    ANALYTICS_METHOD("logEvent", "(" STRING STRING STRING ")V"),
    ANALYTICS_METHOD("logEvent", "(" STRING STRING STRING STRING STRING ")V"),
    ANALYTICS_METHOD("setGDPR", "(Z)V"),
    ANALYTICS_METHOD("setUserId", "(" STRING ")V"),
    ANALYTICS_METHOD("trackABTestPerformed", "(" STRING ")V"),
    ANALYTICS_METHOD("trackABTestTarget", "(" STRING ")V"),
    ANALYTICS_METHOD("trackLogin", "()V"),
    ANALYTICS_METHOD("trackMemoryPurchase", "(I)V"),
    ANALYTICS_METHOD("trackPackPurchase", "(" STRING ")V"),
    ANALYTICS_METHOD("trackRegister", "()V"),
    ANALYTICS_METHOD("trackSinglePurchase", "(" STRING ")V"),
    ANALYTICS_METHOD("trackSongComplete", "(" STRING STRING ")V"),
    ANALYTICS_METHOD("trackSongStart", "(" STRING ")V"),
    ANALYTICS_METHOD("trackStaminaConvert", "()V"),
    ANALYTICS_METHOD("trackStaminaPurchase", "()V"),
    ANALYTICS_METHOD("trackTutorialComplete", "()V"),
    ANALYTICS_METHOD("trackTutorialStart", "()V"),
    ANALYTICS_METHOD("trackUnlock", "(" STRING ")V"),
    ANALYTICS_METHOD("trackBundleDownloadFail", "(" STRING STRING ")V"),
    ANALYTICS_METHOD("trackCacheAbsent", "()V"),
    ANALYTICS_METHOD("trackCacheCorrupt", "()V"),
    ANALYTICS_METHOD("trackCacheFull", "()V"),
    ANALYTICS_METHOD("trackCachePresent", "(" STRING ")V"),
    ANALYTICS_METHOD("trackCacheWriteFail", "()V"),
    ANALYTICS_METHOD("trackHighscore", "(" STRING ")V"),
    ANALYTICS_METHOD("trackRegionlessPurchaseFailure", "(" STRING ")V"),
    ANALYTICS_METHOD("trackStoryOpen", "(" STRING STRING ")V"),
    ANALYTICS_METHOD("trackUnknownSongDownload", "(" STRING ")V"),
#undef ANALYTICS_METHOD
    {ACTIVITY_CLASS, "trackPurchaseComplete", "(" STRING "F)V", COCOS_ANALYTICS, 0, NULL},
    {ACTIVITY_CLASS, "trackPurchaseStarted", "(" STRING "F)V", COCOS_ANALYTICS, 0, NULL},
    {"com/sdkbox/plugin/TrackingInfoAndroid", "trackRequest", "(" STRING ")V", COCOS_ANALYTICS, 0, NULL},
    {"org/cocos2dx/lib/Cocos2dxBitmap",
        "createTextBitmapShadowStroke",
        "([BLjava/lang/String;IIIIIIIIZFFFFZIIIIFZI)Z",
        COCOS_BITMAP,
        0,
        NULL},
    PREFERENCE_METHODS("Bool", "Z", 'Z'),
    PREFERENCE_METHODS("Integer", "I", 'I'),
    PREFERENCE_METHODS("Float", "F", 'F'),
    PREFERENCE_METHODS("Double", "D", 'D'),
    PREFERENCE_METHODS("String", STRING, 'S'),
    {HELPER_CLASS, "setBoolForKey", "(" STRING "Z)Z", COCOS_COMMIT_PREFERENCE, 'Z', NULL},
    {HELPER_CLASS, "setStringForKey", "(" STRING STRING ")Z", COCOS_COMMIT_PREFERENCE, 'S', NULL},
    {HELPER_CLASS, "deleteValueForKey", "(" STRING ")V", COCOS_DELETE_PREFERENCE, 0, NULL},
    {HELPER_CLASS, "getCocos2dxPackageName", "()" STRING, COCOS_STRING, 0, "moe.low.arc"},
    {HELPER_CLASS, "getCurrentLanguage", "()" STRING, COCOS_STRING, 0, "en"},
    {HELPER_CLASS, "getAssetsPath", "()" STRING, COCOS_STRING, 0, PATH_TO_PACKAGE},
    {HELPER_CLASS, "getDeviceModel", "()" STRING, COCOS_STRING, 0, "PlayStation Vita"},
    {HELPER_CLASS, "getCocos2dxWritablePath", "()" STRING, COCOS_WRITABLE_PATH, 0, NULL},
    {HELPER_CLASS, "getDPI", "()I", COCOS_DPI, 0, NULL},
    {HELPER_CLASS, "getAssetManager", "()Landroid/content/res/AssetManager;", COCOS_ASSET_MANAGER, 0, NULL},
    {HELPER_CLASS, "getActivity", "()Landroid/app/Activity;", COCOS_CONTEXT, 0, NULL},
    {ACTIVITY_CLASS, "getContext", "()Landroid/content/Context;", COCOS_CONTEXT, 0, NULL},
    {ACTIVITY_CLASS, "getDeviceModelName", "()" STRING, COCOS_STRING, 0, "PlayStation Vita"},
    {ACTIVITY_CLASS, "generateGuid", "()" STRING, COCOS_GUID, 0, NULL},
    {ACTIVITY_CLASS, "logCrashlytics", "(" STRING ")V", COCOS_BREADCRUMB, 0, NULL},
    {ACTIVITY_CLASS, "startSceneLoadFinished", "()V", COCOS_EXPANSION_CHECK, 0, NULL},
    {"low/moe/AppActivity", "startSceneLoadFinished", "()V", COCOS_STARTUP, 0, NULL},
    {"low/moe/AppActivity", "getDeviceEstimatedStorageLeftKb", "()J", COCOS_STORAGE, 0, NULL},
    {"low/moe/AppActivity", "getDisplayRefreshRate", "()F", COCOS_REFRESH_RATE, 0, NULL},
    {"low/moe/AppActivity", "isAndroidVersion9OrHigher", "()Z", COCOS_ANDROID_VERSION, 0, NULL},
    {"org/cocos2dx/lib/Cocos2dxRenderer", "setAnimationInterval", "(F)V", COCOS_ANIMATION_INTERVAL, 0, NULL},
    {"com/sdkbox/plugin/SdkboxLog", "setDefaultDebugLevel", "(I)V", COCOS_LOG_LEVEL, 0, "_defaultLevel"}};

static jobject __context;
static jobject __asset_manager;
static const char* __activity = ACTIVITY_CLASS;

void jni_cocos_set_activity(bool modern)
{
    const char* name = modern ? "low/moe/AppActivity" : ACTIVITY_CLASS;
    if(strcmp(name, __activity) == 0) {
        return;
    }

    JNIEnv* env = jni_runtime_get_env();
    if(__context && env) {
        (*env)->DeleteGlobalRef(env, __context);
    }

    __context = NULL;
    __activity = name;
}

static const cocos_method_t* __find_method(const char* class_name, const char* name, const char* signature)
{
    for(size_t index = 0; index < sizeof(__methods) / sizeof(__methods[0]); ++index) {
        const cocos_method_t* method = &__methods[index];
        if(strcmp(method->class_name, class_name) != 0) {
            continue;
        }

        if(strcmp(method->name, name) != 0) {
            continue;
        }

        if(strcmp(method->signature, signature) != 0) {
            continue;
        }

        return method;
    }

    if(strcmp(class_name, "low/moe/AppActivity") == 0) {
        return __find_method(ACTIVITY_CLASS, name, signature);
    }

    if(strcmp(class_name, "low/moe/AnalyticsManager") == 0) {
        return __find_method("org/cocos2dx/cpp/AnalyticsManager", name, signature);
    }

    return NULL;
}

static void __throw_io(JNIEnv* env)
{
    int error = errno;
    if(!error) {
        error = EIO;
    }

    jclass clazz = (*env)->FindClass(env, JEX_IO);
    (*env)->ThrowNew(env, clazz, strerror(error));
    (*env)->DeleteLocalRef(env, clazz);
}

static jobject __get_service(jobject* service, const char* class_name)
{
    JNIEnv* env = jni_runtime_get_env();
    if(!env) {
        return NULL;
    }

    jni_thread_lock();
    if(*service) {
        jni_thread_unlock();
        return *service;
    }

    jclass clazz = (*env)->FindClass(env, class_name);
    jobject object = (*env)->AllocObject(env, clazz);
    *service = (*env)->NewGlobalRef(env, object);
    (*env)->DeleteLocalRef(env, object);
    (*env)->DeleteLocalRef(env, clazz);
    jni_thread_unlock();
    return *service;
}

jobject jni_cocos_get_context(void)
{
    return __get_service(&__context, __activity);
}

jobject jni_cocos_get_asset_manager(void)
{
    return __get_service(&__asset_manager, "android/content/res/AssetManager");
}

static jstring __generate_guid(JNIEnv* env)
{
    unsigned char bytes[16];
    if(sceKernelGetRandomNumber(bytes, sizeof(bytes)) < 0) {
        errno = EIO;
        __throw_io(env);
        return NULL;
    }

    bytes[6] = (bytes[6] & 15) | 0x40;
    bytes[8] = (bytes[8] & 63) | 0x80;
    char text[37];
    snprintf(text,
        sizeof(text),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0],
        bytes[1],
        bytes[2],
        bytes[3],
        bytes[4],
        bytes[5],
        bytes[6],
        bytes[7],
        bytes[8],
        bytes[9],
        bytes[10],
        bytes[11],
        bytes[12],
        bytes[13],
        bytes[14],
        bytes[15]);
    return (*env)->NewStringUTF(env, text);
}

static bool __parse_number(const char* value, char stored_type, double* number)
{
    if(stored_type == 'Z') {
        *number = strcmp(value, "true") == 0 ? 1 : 0;
        return true;
    }

    char* end;
    *number = strtod(value, &end);
    if(end == value) {
        return false;
    }

    return *end == 0;
}

static jint __number_to_integer(double number)
{
    /* Match Java's numeric cast without undefined float-to-integer conversions. */
    if(isnan(number)) {
        return 0;
    }

    if(number >= INT32_MAX) {
        return INT32_MAX;
    }

    if(number <= INT32_MIN) {
        return INT32_MIN;
    }

    return (jint)number;
}

static void __decode_preference(JNIEnv* env, char type, char stored_type, const char* value, jvalue* result)
{
    if(type == 'S') {
        result->l = (*env)->NewStringUTF(env, value);
        return;
    }

    if(type == 'Z') {
        if(stored_type == 'S' || stored_type == 'Z') {
            result->z = strcasecmp(value, "true") == 0;
            return;
        }
    }

    double number;
    if(!__parse_number(value, stored_type, &number)) {
        return;
    }

    switch(type) {
        case 'Z':
            result->z = number != 0;
            break;

        case 'I':
            result->i = __number_to_integer(number);
            break;

        case 'F':
            result->f = (jfloat)number;
            break;

        case 'D':
            result->d = (jdouble)(jfloat)number;
            break;
    }
}

static void __get_preference(JNIEnv* env, const char* key, char type, jvalue default_value, jvalue* result)
{
    *result = default_value;
    /* Cocos2dxHelper stores doubles as SharedPreferences floats. */
    if(type == 'D') {
        result->d = (jdouble)(jfloat)default_value.d;
    }

    char stored_type;
    char* value = cocos_preferences_get(key, &stored_type);
    if(!value) {
        if(errno) {
            __throw_io(env);
        }

        return;
    }

    __decode_preference(env, type, stored_type, value, result);
    free(value);
}

static bool __set_preference(JNIEnv* env, const char* key, char type, jvalue input)
{
    char number[64];
    const char* value = number;
    switch(type) {
        case 'Z':
            value = input.z ? "true" : "false";
            break;

        case 'I':
            snprintf(number, sizeof(number), "%ld", (long)input.i);
            break;

        case 'F':
            snprintf(number, sizeof(number), "%.9g", (double)input.f);
            break;

        case 'D':
            snprintf(number, sizeof(number), "%.9g", (double)(jfloat)input.d);
            type = 'F';
            break;

        case 'S':
            value = jni_runtime_get_string_utf(input.l);
            break;

        default:
            return false;
    }

    bool saved;
    if(value) {
        saved = cocos_preferences_set(key, type, value);
    } else {
        /* SharedPreferences.putString(key, null) removes the entry. */
        saved = cocos_preferences_delete(key);
    }

    return saved;
}

static void __call_preference(JNIEnv* env, const cocos_method_t* method, const jvalue* args, jvalue* result)
{
    const char* key = NULL;
    if(args) {
        key = jni_runtime_get_string_utf(args[0].l);
    }

    if(!key) {
        jclass clazz = (*env)->FindClass(env, JEX_NULL);
        (*env)->ThrowNew(env, clazz, "preference key is null");
        (*env)->DeleteLocalRef(env, clazz);
        return;
    }

    switch(method->operation) {
        case COCOS_GET_PREFERENCE:
            __get_preference(env, key, method->value_type, args[1], result);
            break;

        case COCOS_SET_PREFERENCE:
            if(!__set_preference(env, key, method->value_type, args[1])) {
                __throw_io(env);
            }

            break;

        case COCOS_COMMIT_PREFERENCE:
            result->z = __set_preference(env, key, method->value_type, args[1]);
            break;

        case COCOS_DELETE_PREFERENCE:
            if(!cocos_preferences_delete(key)) {
                __throw_io(env);
            }

            break;

        default:
            break;
    }
}

static int __analytics_arg(JNIEnv* env, char type, jvalue arg, const char* prefix, char* buffer, size_t size)
{
    switch(type) {
        case 'L': {
            const char* text = NULL;
            if(arg.l) {
                text = (*env)->GetStringUTFChars(env, arg.l, NULL);
            }

            if((*env)->ExceptionCheck(env)) {
                return -1;
            }

            int written = snprintf(buffer, size, "%s%s", prefix, text ? text : "(null)");
            if(text) {
                (*env)->ReleaseStringUTFChars(env, arg.l, text);
            }

            return written;
        }

        case 'F': return snprintf(buffer, size, "%s%.6g", prefix, (double)arg.f);
        case 'Z': return snprintf(buffer, size, "%s%d", prefix, arg.z ? 1 : 0);
        default:  return snprintf(buffer, size, "%s%d", prefix, arg.i);
    }
}

static bool __analytics(JNIEnv* env, const char* clazz, const char* name, const char* signature, const jvalue* args)
{
    jni_sig_t sig;
    if(!jni_sig_parse(signature, &sig)) {
        return false;
    }

    char values[512] = {0};

    size_t used = 0;
    for(size_t index = 0; index < sig.argc && used < sizeof(values) - 1; ++index) {
        int written =
            __analytics_arg(env, sig.args[index], args[index], index ? ", " : "", values + used, sizeof(values) - used);
        if(written < 0) {
            break;
        }

        size_t available = sizeof(values) - used;
        used += (size_t)written < available ? (size_t)written : available - 1;
    }

    log_i(TAG, "Local analytics %s.%s(%s)", clazz, name, values);
    return true;
}

bool jni_cocos_dispatch(
    JNIEnv* env, const char* class_name, const char* name, const char* signature, const jvalue* args, jvalue* result)
{
    const cocos_method_t* method = __find_method(class_name, name, signature);
    if(!method) {
        return false;
    }

    switch(method->operation) {
        case COCOS_ANALYTICS: return __analytics(env, class_name, name, signature, args);

        case COCOS_BITMAP:
            result->z = jni_bitmap_create(env, args);
            break;

        case COCOS_ANIMATION_INTERVAL:
            if(!vita_frame_set_interval(args[0].f)) {
                jclass exception = (*env)->FindClass(env, JEX_ARG);
                (*env)->ThrowNew(env, exception, "Invalid animation interval");
                (*env)->DeleteLocalRef(env, exception);
            }

            break;

        case COCOS_BREADCRUMB: {
            // The APK forwards this breadcrumb to Crashlytics.log(String). Preserve it
            // in the local Vita log; no Android analytics service is required.
            const char* text = args[0].l ? (*env)->GetStringUTFChars(env, args[0].l, NULL) : NULL;
            if(!args[0].l || text) {
                log_i(TAG, "Game breadcrumb: %s", text ? text : "(null)");
            }

            if(text) {
                (*env)->ReleaseStringUTFChars(env, args[0].l, text);
            }

            break;
        }

        case COCOS_GET_PREFERENCE:
        case COCOS_SET_PREFERENCE:
        case COCOS_COMMIT_PREFERENCE:
        case COCOS_DELETE_PREFERENCE:
            __call_preference(env, method, args, result);
            break;

        case COCOS_STRING:
            result->l = (*env)->NewStringUTF(env, method->text);
            break;

        case COCOS_WRITABLE_PATH:
            if(!cocos_prepare_files_directory()) {
                __throw_io(env);
                break;
            }

            result->l = (*env)->NewStringUTF(env, ANDROID_FILES_PATH);
            break;

        case COCOS_GUID:
            result->l = __generate_guid(env);
            break;

        case COCOS_DPI:
            result->i = 220;
            break;

        case COCOS_ASSET_MANAGER:
            result->l = jni_cocos_get_asset_manager();
            break;

        case COCOS_CONTEXT:
            result->l = jni_cocos_get_context();
            break;

        case COCOS_LOG_LEVEL: {
            // The APK method stores its integer argument in this Java static field.
            jclass clazz = (*env)->FindClass(env, class_name);
            if(clazz) {
                jfieldID field = (*env)->GetStaticFieldID(env, clazz, method->text, "I");
                if(field) {
                    (*env)->SetStaticIntField(env, clazz, field, args[0].i);
                }

                (*env)->DeleteLocalRef(env, clazz);
            }

            break;
        }

        case COCOS_STARTUP:
            if(!bridge_jni_cocos_notify_startup()) {
                jclass exception = (*env)->FindClass(env, JEX_STATE);
                (*env)->ThrowNew(env, exception, "Native startup notification failed");
                (*env)->DeleteLocalRef(env, exception);
            }

            break;

        case COCOS_STORAGE: {
#ifdef __vita__
            SceIoDevInfo info;
            int status = sceIoDevctl("ux0:", 0x3001, NULL, 0, &info, sizeof(info));
            result->j = status < 0 ? -1 : info.free_size / 1024;
#else
            struct statvfs info;
            int status = statvfs(PATH_TO_USER, &info);
            result->j = status < 0 ? -1 : (jlong)((uint64_t)info.f_bavail * info.f_frsize / 1024);
#endif
            break;
        }

        case COCOS_REFRESH_RATE:
            result->f = 60.0f;
            break;

        case COCOS_ANDROID_VERSION:
            result->z = JNI_FALSE;
            break;

        case COCOS_EXPANSION_CHECK: {
            // The APK posts a storage-permission/expansion check to Android's UI thread.
            // Vita uses packaged assets; startup already delivers its native completion.
            // Acknowledge that same completion once, including Java's persistent flag.
            if(!bridge_jni_cocos_complete_expansion()) {
                jclass exception = (*env)->FindClass(env, JEX_STATE);
                (*env)->ThrowNew(env, exception, "Packaged expansion setup unavailable");
                (*env)->DeleteLocalRef(env, exception);
                break;
            }

            jclass activity = (*env)->FindClass(env, ACTIVITY_CLASS);
            if(activity) {
                jfieldID field = (*env)->GetStaticFieldID(env, activity, "expansionCheckPerformed", "Z");
                if(field) {
                    (*env)->SetStaticBooleanField(env, activity, field, JNI_TRUE);
                }

                (*env)->DeleteLocalRef(env, activity);
            }

            break;
        }
    }

    return true;
}
