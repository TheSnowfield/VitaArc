#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <android/assetmgr.h>
#include <config.h>
#include <jni/jni.h>
#include <jni/runtime.h>
#include <libc/file.h>
#include <platform/timing.h>

#define HELPER "org/cocos2dx/lib/Cocos2dxHelper"
#define STRING "Ljava/lang/String;"

static JNIEnv* __env;
static jclass __helper;
extern const char* mock_missing_export;
extern _Thread_local char mock_last_info[256];
void test_jni_dispatch(void);
void test_application_loop(const char* path);
void test_jni_references(const char* path);

static void __static_void_v(jclass clazz, jmethodID method, ...)
{
    va_list args;
    va_start(args, method);
    (*__env)->CallStaticVoidMethodV(__env, clazz, method, args);
    va_end(args);
}

static void __test_animation_interval(void)
{
    jclass renderer = (*__env)->FindClass(__env, "org/cocos2dx/lib/Cocos2dxRenderer");
    jmethodID method = (*__env)->GetStaticMethodID(__env, renderer, "setAnimationInterval", "(F)V");
    assert(method && vita_frame_delay(100, 100) == 0);
    (*__env)->CallStaticVoidMethod(__env, renderer, method, 0.05);
    assert(!(*__env)->ExceptionCheck(__env));
    assert(vita_frame_delay(100000, 120000) == 30000);
    assert(vita_frame_delay(100000, 150000) == 0);
    assert(vita_frame_delay(100000, 180000) == 0);
    assert(vita_frame_delay(100000, 99999) == 0);
    jvalue value = {.f = 0.02f};

    (*__env)->CallStaticVoidMethodA(__env, renderer, method, &value);
    assert(!(*__env)->ExceptionCheck(__env));
    assert(vita_frame_delay(0, 17000) == 3000);
    __static_void_v(renderer, method, 0.0);
    assert(!(*__env)->ExceptionCheck(__env) && vita_frame_delay(0, 0) == 0);
    const float invalid[] = {-1.0f, NAN, INFINITY, 5000.0f};

    for(size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        value.f = invalid[index];
        (*__env)->CallStaticVoidMethodA(__env, renderer, method, &value);
        assert((*__env)->ExceptionCheck(__env));
        (*__env)->ExceptionClear(__env);
        assert(vita_frame_delay(0, 0) == 0);
    }

    __static_void_v(renderer, method, 1.0 / 60.0);
    assert(!(*__env)->ExceptionCheck(__env) && vita_frame_delay(0, 0) == 0);
    assert(vita_frame_set_interval(0.05f));
    assert(vita_frame_delay(UINT64_MAX - 100000, UINT64_MAX - 90000) == 40000);
    assert(vita_frame_set_interval(0.0000001f) && vita_frame_delay(0, 0) == 0);
    assert(vita_frame_set_interval(4294.0f));
    assert(vita_frame_delay(0, 0) == UINT32_C(4294000000));
    assert(vita_frame_delay(0, UINT64_MAX) == 0);
    assert(vita_frame_set_interval(1.0f / 60.0f));
    (*__env)->DeleteLocalRef(__env, renderer);
}

static jmethodID __method(const char* name, const char* signature)
{
    jmethodID method = (*__env)->GetStaticMethodID(__env, __helper, name, signature);
    assert(method != NULL);
    return method;
}

static jstring __string(const char* text)
{
    jstring result = (*__env)->NewStringUTF(__env, text);
    assert(result != NULL);
    return result;
}

static void __expect_string(jstring string, const char* expected)
{
    assert(string != NULL);
    const char* actual = (*__env)->GetStringUTFChars(__env, string, NULL);
    assert(actual != NULL);
    assert(strcmp(actual, expected) == 0);
    (*__env)->ReleaseStringUTFChars(__env, string, actual);
}

static void __call_void_v(jmethodID method, ...)
{
    va_list args;
    va_start(args, method);
    (*__env)->CallStaticVoidMethodV(__env, __helper, method, args);
    va_end(args);
}

#define DEFINE_CALL_V(name, lower_name, type)                                                                          \
    static type __call_##lower_name##_v(jmethodID method, ...)                                                         \
    {                                                                                                                  \
        va_list args;                                                                                                  \
        va_start(args, method);                                                                                        \
        type result = (*__env)->CallStatic##name##MethodV(__env, __helper, method, args);                              \
        va_end(args);                                                                                                  \
        return result;                                                                                                 \
    }
DEFINE_CALL_V(Object, object, jobject)
DEFINE_CALL_V(Boolean, boolean, jboolean)
DEFINE_CALL_V(Int, int, jint)
DEFINE_CALL_V(Float, float, jfloat)
DEFINE_CALL_V(Double, double, jdouble)

static void __test_defaults(void)
{
    jstring key = __string("missing");
    jvalue args[2] = {{.l = key}, {.i = 93}};

    jmethodID method = __method("getIntegerForKey", "(" STRING "I)I");
    assert((*__env)->CallStaticIntMethod(__env, __helper, method, key, 93) == 93);
    assert(__call_int_v(method, key, 93) == 93);
    assert((*__env)->CallStaticIntMethodA(__env, __helper, method, args) == 93);
    args[1].z = JNI_TRUE;
    method = __method("getBoolForKey", "(" STRING "Z)Z");
    assert((*__env)->CallStaticBooleanMethod(__env, __helper, method, key, JNI_TRUE));
    assert(__call_boolean_v(method, key, JNI_TRUE));
    assert((*__env)->CallStaticBooleanMethodA(__env, __helper, method, args));
    args[1].f = 3.25f;
    method = __method("getFloatForKey", "(" STRING "F)F");
    assert((*__env)->CallStaticFloatMethod(__env, __helper, method, key, 3.25) == 3.25f);
    assert(__call_float_v(method, key, 3.25) == 3.25f);
    assert((*__env)->CallStaticFloatMethodA(__env, __helper, method, args) == 3.25f);
    args[1].d = 0.1;
    method = __method("getDoubleForKey", "(" STRING "D)D");
    assert((*__env)->CallStaticDoubleMethod(__env, __helper, method, key, 0.1) == (double)(float)0.1);
    assert(__call_double_v(method, key, 0.1) == (double)(float)0.1);
    assert((*__env)->CallStaticDoubleMethodA(__env, __helper, method, args) == (double)(float)0.1);
    args[1].l = __string("default");
    method = __method("getStringForKey", "(" STRING STRING ")" STRING);
    __expect_string((*__env)->CallStaticObjectMethod(__env, __helper, method, key, args[1].l), "default");
    __expect_string(__call_object_v(method, key, args[1].l), "default");
    __expect_string((*__env)->CallStaticObjectMethodA(__env, __helper, method, args), "default");
    assert(!(*__env)->ExceptionCheck(__env));
}

static void __test_strings_and_arrays(void)
{
    const jchar chars[] = {'A', 0, 0x6E56, 0xD83D, 0xDE00};

    const char expected[] = "A\xC0\x80\xE6\xB9\x96\xED\xA0\xBD\xED\xB8\x80";
    jstring string = (*__env)->NewString(__env, chars, 5);
    assert((*__env)->GetStringLength(__env, string) == 5);
    assert((*__env)->GetStringUTFLength(__env, string) == 12);
    __expect_string(string, expected);
    jstring roundtrip = __string(expected);
    assert((*__env)->GetStringLength(__env, roundtrip) == 5);
    assert(memcmp((*__env)->GetStringChars(__env, roundtrip, NULL), chars, sizeof(chars)) == 0);
    char region[5] = {0, 0, 0, 'x', 'y'};

    (*__env)->GetStringUTFRegion(__env, string, 2, 1, region);
    assert(memcmp(region, "\xE6\xB9\x96xy", 5) == 0);
    (*__env)->GetStringUTFRegion(__env, string, INT_MAX, 2, region);
    assert((*__env)->ExceptionCheck(__env));
    (*__env)->ExceptionClear(__env);
    (*__env)->GetStringRegion(__env, string, 5, 0, NULL);
    assert(!(*__env)->ExceptionCheck(__env));
    jintArray array = (*__env)->NewIntArray(__env, 2);
    jint data[] = {42, -7};

    (*__env)->SetIntArrayRegion(__env, array, 0, 2, data);
    jint result[2] = {0};

    (*__env)->GetIntArrayRegion(__env, array, 0, 2, result);
    assert(memcmp(data, result, sizeof(data)) == 0);
    (*__env)->SetIntArrayRegion(__env, array, INT_MAX, 2, data);
    assert((*__env)->ExceptionCheck(__env));
    (*__env)->ExceptionClear(__env);
    (*__env)->GetIntArrayRegion(__env, array, 0, 2, result);
    assert(memcmp(data, result, sizeof(data)) == 0);
    assert((*__env)->NewDirectByteBuffer(__env, data, (jlong)INT_MAX + 1) == NULL);
    jobject buffer = (*__env)->NewDirectByteBuffer(__env, data, sizeof(data));
    assert((*__env)->GetDirectBufferAddress(__env, buffer) == data);
    assert((*__env)->GetDirectBufferCapacity(__env, buffer) == sizeof(data));
}

static void __test_services(void)
{
    jmethodID method = __method("getCocos2dxWritablePath", "()" STRING);
    __expect_string((*__env)->CallStaticObjectMethod(__env, __helper, method), ANDROID_FILES_PATH);
    struct stat status;
    assert(stat(PATH_TO_FILES, &status) == 0 && S_ISDIR(status.st_mode));
    method = __method("getAssetManager", "()Landroid/content/res/AssetManager;");
    jobject manager = (*__env)->CallStaticObjectMethod(__env, __helper, method);
    assert(manager != NULL);
    a_asset_manager_t* native = a_asset_manager_from_java(__env, manager);
    a_asset_t* asset = a_asset_manager_open(native, "fixture.txt", AASSET_MODE_RANDOM);
    assert(asset != NULL && a_asset_get_length(asset) == 18);
    char data[32] = {0};

    assert(a_asset_read(asset, data, 5) == 5);
    assert(memcmp(data, "mock ", 5) == 0);
    assert(a_asset_get_remaining_length(asset) == 13);
    assert(memcmp(a_asset_get_buffer(asset), "mock asset payload", 18) == 0);
    assert(a_asset_get_remaining_length(asset) == 13);
    off_t start, length;
    int descriptor = a_asset_open_file_descriptor(asset, &start, &length);
    assert(descriptor >= 0 && start == 0 && length == 18);
    a_asset_close(asset);
    assert(read(descriptor, data, sizeof(data)) == 18);
    close(descriptor);
    assert(a_asset_manager_open(native, "../secret", AASSET_MODE_RANDOM) == NULL);
    a_asset_dir_t* directory = a_asset_manager_open_dir(native, "");
    assert(strcmp(a_asset_dir_get_next_file_name(directory), "empty.txt") == 0);
    assert(strcmp(a_asset_dir_get_next_file_name(directory), "fixture.txt") == 0);
    assert(a_asset_dir_get_next_file_name(directory) == NULL);
    a_asset_dir_rewind(directory);
    assert(strcmp(a_asset_dir_get_next_file_name(directory), "empty.txt") == 0);
    a_asset_dir_close(directory);
    jclass activity = (*__env)->FindClass(__env, "org/cocos2dx/cpp/AppActivity");
    method = (*__env)->GetStaticMethodID(__env, activity, "logCrashlytics", "(" STRING ")V");
    jstring breadcrumb = __string("Screen: main 100% %s");
    (*__env)->CallStaticVoidMethod(__env, activity, method, breadcrumb);
    assert(!(*__env)->ExceptionCheck(__env));
    assert(strcmp(mock_last_info, "Game breadcrumb: Screen: main 100% %s") == 0);
    __static_void_v(activity, method, breadcrumb);
    assert(!(*__env)->ExceptionCheck(__env));
    assert(strcmp(mock_last_info, "Game breadcrumb: Screen: main 100% %s") == 0);
    jvalue log_args = {.l = breadcrumb};

    (*__env)->CallStaticVoidMethodA(__env, activity, method, &log_args);
    assert(!(*__env)->ExceptionCheck(__env));
    assert(strcmp(mock_last_info, "Game breadcrumb: Screen: main 100% %s") == 0);
    (*__env)->DeleteLocalRef(__env, breadcrumb);
    (*__env)->CallStaticVoidMethod(__env, activity, method, (jobject)NULL);
    assert(!(*__env)->ExceptionCheck(__env));
    assert(strcmp(mock_last_info, "Game breadcrumb: (null)") == 0);
    method = (*__env)->GetStaticMethodID(__env, activity, "logCrashlytics", "(I)V");
    (*__env)->CallStaticVoidMethod(__env, activity, method, 0);
    assert((*__env)->ExceptionCheck(__env));
    (*__env)->ExceptionClear(__env);
    method = (*__env)->GetStaticMethodID(__env, activity, "generateGuid", "()" STRING);
    jstring first = (*__env)->CallStaticObjectMethod(__env, activity, method);
    jstring second = (*__env)->CallStaticObjectMethod(__env, activity, method);
    const char* first_text = jni_runtime_get_string_utf(first);
    assert(strlen(first_text) == 36 && first_text[14] == '4');
    assert(strcmp(first_text, jni_runtime_get_string_utf(second)) != 0);
    method = __method("getIntegerForKey", "()I");
    assert((*__env)->CallStaticIntMethod(__env, __helper, method) == 0);
    assert((*__env)->ExceptionCheck(__env));
    (*__env)->ExceptionClear(__env);
    JavaVM* vm = jni_runtime_get_java_vm();
    void* invalid_env = (void*)1;
    assert((*vm)->GetEnv(vm, &invalid_env, -1) == JNI_EVERSION);
    assert(invalid_env == NULL);
}

static void __test_fields(void)
{
    jclass clazz = (*__env)->FindClass(__env, "test/Fields");
    jobject first = (*__env)->AllocObject(__env, clazz);
    jobject second = (*__env)->AllocObject(__env, clazz);
    jstring text = __string("field value");
#define CHECK_FIELD(name, signature, type, input)                                                                      \
    {                                                                                                                  \
        jfieldID field = (*__env)->GetFieldID(__env, clazz, #name, signature);                                         \
        jfieldID shared = (*__env)->GetStaticFieldID(__env, clazz, #name, signature);                                  \
        assert((*__env)->Get##name##Field(__env, first, field) == (type)0);                                            \
        (*__env)->Set##name##Field(__env, first, field, input);                                                        \
        assert((*__env)->Get##name##Field(__env, first, field) == input);                                              \
        assert((*__env)->Get##name##Field(__env, second, field) == (type)0);                                           \
        (*__env)->SetStatic##name##Field(__env, clazz, shared, input);                                                 \
        assert((*__env)->GetStatic##name##Field(__env, clazz, shared) == input);                                       \
    }
    jfieldID object_field = (*__env)->GetFieldID(__env, clazz, "Object", STRING);
    jfieldID shared_object = (*__env)->GetStaticFieldID(__env, clazz, "Object", STRING);
    assert((*__env)->GetObjectField(__env, first, object_field) == NULL);
    (*__env)->SetObjectField(__env, first, object_field, text);
    assert((*__env)->IsSameObject(__env, (*__env)->GetObjectField(__env, first, object_field), text));
    assert((*__env)->GetObjectField(__env, second, object_field) == NULL);
    (*__env)->SetStaticObjectField(__env, clazz, shared_object, text);
    assert((*__env)->IsSameObject(__env, (*__env)->GetStaticObjectField(__env, clazz, shared_object), text));
    CHECK_FIELD(Boolean, "Z", jboolean, JNI_TRUE)
    CHECK_FIELD(Byte, "B", jbyte, -12)
    CHECK_FIELD(Char, "C", jchar, 0x6e56)
    CHECK_FIELD(Short, "S", jshort, -32123)
    CHECK_FIELD(Int, "I", jint, INT_MAX)
    CHECK_FIELD(Long, "J", jlong, INT64_C(123456789012345))
    CHECK_FIELD(Float, "F", jfloat, 0.125f)
    CHECK_FIELD(Double, "D", jdouble, 0.123456789012345)
#undef CHECK_FIELD
}

static void __test_registration(void)
{
    jclass clazz = (*__env)->FindClass(__env, "test/Native");
    JNINativeMethod methods[] = {{"first", "()V", __test_fields}, {"second", "()V", __test_services}};

    assert((*__env)->RegisterNatives(__env, clazz, methods, 2) == JNI_OK);
    assert(jni_runtime_find_native("test/Native", "first", "()V") == __test_fields);
    assert(jni_runtime_find_native("test/Native", "first", "()I") == NULL);
    assert((*__env)->UnregisterNatives(__env, clazz) == JNI_OK);
    assert(jni_runtime_find_native("test/Native", "first", "()V") == NULL);
    methods[1].fnPtr = NULL;
    assert((*__env)->RegisterNatives(__env, clazz, methods, 2) == JNI_EINVAL);
    assert(jni_runtime_find_native("test/Native", "first", "()V") == NULL);
}

static void __test_save_file(bool write_file)
{
    jstring directory =
        (*__env)->CallStaticObjectMethod(__env, __helper, __method("getCocos2dxWritablePath", "()" STRING));
    const char* text = jni_runtime_get_string_utf(directory);
    assert(text && text[0] == '/' && !strchr(text, ':'));
    char path[1024];
    assert(snprintf(path, sizeof(path), "%s/save-roundtrip", text) < (int)sizeof(path));
    static const unsigned char payload[] = {0, 42, 255, 's', 'a', 'v', 'e'};

    FILE* file = bridge_fopen(path, write_file ? "wb" : "rb");
    assert(file);
    if(write_file) {
        assert(fwrite(payload, 1, sizeof(payload), file) == sizeof(payload));
    } else {
        unsigned char actual[sizeof(payload)];
        assert(fread(actual, 1, sizeof(actual), file) == sizeof(actual));
        assert(memcmp(payload, actual, sizeof(actual)) == 0 && fgetc(file) == EOF);
    }

    assert(fclose(file) == 0);
    /* Independently check the native destination and the second Android alias. */
    file = fopen(PATH_TO_FILES "save-roundtrip", "rb");
    assert(file && fgetc(file) == 0 && fgetc(file) == 42 && fclose(file) == 0);
    file = bridge_fopen("/data/user/0/moe.low.arc/files/save-roundtrip", "rb");
    assert(file && fgetc(file) == 0 && fgetc(file) == 42 && fclose(file) == 0);

    jclass clazz = (*__env)->FindClass(__env, "java/io/File");
    jmethodID constructor = (*__env)->GetMethodID(__env, clazz, "<init>", "(" STRING ")V");
    jobject object = (*__env)->NewObject(__env, clazz, constructor, __string(path));
    assert((*__env)->CallBooleanMethod(__env, object, (*__env)->GetMethodID(__env, clazz, "exists", "()Z")));
    assert(!(*__env)->CallBooleanMethod(__env, object, (*__env)->GetMethodID(__env, clazz, "isDirectory", "()Z")));
    (*__env)->DeleteLocalRef(__env, object);
    (*__env)->DeleteLocalRef(__env, clazz);
    (*__env)->DeleteLocalRef(__env, directory);
}

static void __write_preferences(void)
{
    __call_void_v(
        __method("setStringForKey", "(" STRING STRING ")V"), __string("player/../name"), __string("Lake\nFairy"));
    (*__env)->CallStaticVoidMethod(
        __env, __helper, __method("setIntegerForKey", "(" STRING "I)V"), __string("score"), INT_MAX);
    jvalue args[2] = {{.l = __string("enabled")}, {.z = JNI_TRUE}};

    (*__env)->CallStaticVoidMethodA(__env, __helper, __method("setBoolForKey", "(" STRING "Z)V"), args);
    __call_void_v(__method("setFloatForKey", "(" STRING "F)V"), __string("volume"), 0.625);
    args[0].l = __string("offset");
    args[1].d = 0.123456789;
    (*__env)->CallStaticVoidMethodA(__env, __helper, __method("setDoubleForKey", "(" STRING "D)V"), args);
    __call_void_v(__method("setStringForKey", "(" STRING STRING ")V"), __string("removed"), __string("before"));
    __call_void_v(__method("deleteValueForKey", "(" STRING ")V"), __string("removed"));
    __call_void_v(__method("setStringForKey", "(" STRING STRING ")V"), __string("null-value"), __string("before"));
    __call_void_v(__method("setStringForKey", "(" STRING STRING ")V"), __string("null-value"), NULL);
    assert(!(*__env)->ExceptionCheck(__env));
}

static void __read_preferences(void)
{
    __expect_string(
        __call_object_v(
            __method("getStringForKey", "(" STRING STRING ")" STRING), __string("player/../name"), __string("missing")),
        "Lake\nFairy");
    assert(__call_int_v(__method("getIntegerForKey", "(" STRING "I)I"), __string("score"), 0) == INT_MAX);
    assert(__call_boolean_v(__method("getBoolForKey", "(" STRING "Z)Z"), __string("enabled"), JNI_FALSE));
    assert(__call_float_v(__method("getFloatForKey", "(" STRING "F)F"), __string("volume"), 0.0) == 0.625f);
    assert(__call_double_v(__method("getDoubleForKey", "(" STRING "D)D"), __string("offset"), 0.0) ==
        (double)(float)0.123456789);
    assert(__call_int_v(__method("getIntegerForKey", "(" STRING "I)I"), __string("removed"), 73) == 73);
    assert(__call_int_v(__method("getIntegerForKey", "(" STRING "I)I"), __string("null-value"), 81) == 81);
    assert(!(*__env)->ExceptionCheck(__env));
}

static int* __counter(void* game, const char* name)
{
    int* value = dlsym(game, name);
    assert(value != NULL);
    return value;
}

static void __exercise_callbacks(void)
{
    const int32_t ids[] = {7, 8};

    const float xs[] = {12.5f, 40.0f}, ys[] = {30.25f, 50.0f};

    bridge_jni_cocos_render();
    bridge_jni_cocos_pause();
    bridge_jni_cocos_resume();
    bridge_jni_cocos_touch_begin(7, 12.5f, 30.25f);
    bridge_jni_cocos_touch_end(7, 12.5f, 30.25f);
    bridge_jni_cocos_touch_move(ids, xs, ys, 2);
    bridge_jni_cocos_key_event(4, true);
}

static void __test_callback_failures(void* game, dynalib_t* library)
{
    int* calls = __counter(game, "mock_callback_count");
    int* failure = __counter(game, "mock_fail_callback");
    for(int stage = 1; stage <= 7; ++stage) {
        assert(bridge_jni_cocos_start(library, 960, 544));
        int before = *calls;
        *failure = stage;
        __exercise_callbacks();
        assert(*calls == before + stage);
        assert((*__env)->ExceptionCheck(__env));
        assert(jni_runtime_find_native("org/cocos2dx/lib/Cocos2dxRenderer", "nativeRender", "()V") == NULL);
        (*__env)->ExceptionClear(__env);
        *failure = 0;
        __exercise_callbacks();
        assert(*calls == before + stage);
    }

    assert(bridge_jni_cocos_start(library, 960, 544));
    int before = *calls;
    (*__env)->ThrowNew(__env, (*__env)->FindClass(__env, "java/lang/RuntimeException"), "pending before callback");
    __exercise_callbacks();
    assert(*calls == before);
    assert((*__env)->ExceptionCheck(__env));
    (*__env)->ExceptionClear(__env);
    __exercise_callbacks();
    assert(*calls == before);
    assert(bridge_jni_cocos_start(library, 960, 544));
    __exercise_callbacks();
    assert(*calls == before + 7);
    bridge_jni_cocos_stop();
}

static void __test_startup(const char* path)
{
    void* game = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if(!game) {
        fprintf(stderr, "%s\n", dlerror());
    }

    assert(game != NULL);
    dynalib_t library = {0};

    library.image = game;
    assert(bridge_jni_cocos_start(&library, 960, 544));
    assert(*__counter(game, "mock_sdkbox_count") == 1);
    assert(*__counter(game, "mock_expansion_count") == 1);
    bridge_jni_cocos_render();
    bridge_jni_cocos_pause();
    bridge_jni_cocos_resume();
    bridge_jni_cocos_touch_begin(7, 12.5f, 30.25f);
    bridge_jni_cocos_touch_end(7, 12.5f, 30.25f);
    const int32_t ids[] = {7, 8};

    const float xs[] = {12.5f, 40.0f}, ys[] = {30.25f, 50.0f};

    bridge_jni_cocos_touch_move(ids, xs, ys, 2);
    bridge_jni_cocos_touch_move(NULL, xs, ys, 2);
    assert(bridge_jni_cocos_key_event(4, true));
    assert(!bridge_jni_cocos_key_event(4, false));
    assert(*__counter(game, "mock_render_count") == 1);
    assert(*__counter(game, "mock_pause_count") == 1);
    assert(*__counter(game, "mock_resume_count") == 1);
    assert(*__counter(game, "mock_touch_count") == 3);
    bridge_jni_cocos_stop();
    bridge_jni_cocos_render();
    assert(*__counter(game, "mock_render_count") == 1);
    assert(!bridge_jni_cocos_complete_expansion());
    mock_missing_export = "Java_org_cocos2dx_cpp_AppActivity_notifyExpansionCheckComplete";
    int expansion_calls = *__counter(game, "mock_expansion_count");
    assert(!bridge_jni_cocos_start(&library, 960, 544));
    assert(!bridge_jni_cocos_is_ready() && (*__env)->ExceptionCheck(__env));
    assert(*__counter(game, "mock_expansion_count") == expansion_calls);
    (*__env)->ExceptionClear(__env);
    mock_missing_export = NULL;
    *__counter(game, "mock_fail_sdkbox") = 1;
    assert(!bridge_jni_cocos_start(&library, 960, 544));
    assert(*__counter(game, "mock_stage") == 2);
    assert(!bridge_jni_cocos_is_ready());
    assert((*__env)->ExceptionCheck(__env));
    (*__env)->ExceptionClear(__env);
    *__counter(game, "mock_fail_sdkbox") = 0;
    int sdkbox_calls = *__counter(game, "mock_sdkbox_count");
    mock_missing_export = "Java_com_sdkbox_plugin_SDKBox_nativeInit";
    assert(bridge_jni_cocos_start(&library, 960, 544));
    bridge_jni_cocos_stop();
    assert(*__counter(game, "mock_sdkbox_count") == sdkbox_calls);
    *__counter(game, "mock_register_sdkbox") = 1;
    assert(bridge_jni_cocos_start(&library, 960, 544));
    bridge_jni_cocos_stop();
    assert(*__counter(game, "mock_sdkbox_count") == sdkbox_calls + 1);
    assert(jni_runtime_find_native(
               "com/sdkbox/plugin/SDKBox", "nativeInit", "(Ljava/lang/Object;Ljava/lang/ClassLoader;)V") == NULL);
    *__counter(game, "mock_register_sdkbox") = 0;
    mock_missing_export = NULL;
    mock_missing_export = "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeRender";
    assert(!bridge_jni_cocos_start(&library, 960, 544));
    mock_missing_export = NULL;
    *__counter(game, "mock_fail_on_load") = 1;
    assert(!bridge_jni_cocos_start(&library, 960, 544));
    *__counter(game, "mock_fail_on_load") = 0;
    *__counter(game, "mock_fail_init") = 1;
    assert(!bridge_jni_cocos_start(&library, 960, 544));
    bridge_jni_cocos_render();
    assert(*__counter(game, "mock_render_count") == 1);
    assert((*__env)->ExceptionCheck(__env));
    (*__env)->ExceptionClear(__env);
    *__counter(game, "mock_fail_init") = 0;
    assert(bridge_jni_cocos_start(&library, 960, 544));
    bridge_jni_cocos_stop();
    *__counter(game, "mock_register_render") = 1;
    mock_missing_export = "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeRender";
    assert(bridge_jni_cocos_start(&library, 960, 544));
    bridge_jni_cocos_render();
    assert(*__counter(game, "mock_render_count") == 11);
    bridge_jni_cocos_stop();
    assert(jni_runtime_find_native("org/cocos2dx/lib/Cocos2dxRenderer", "nativeRender", "()V") == NULL);
    mock_missing_export = NULL;
    *__counter(game, "mock_fail_on_load") = 1;
    assert(!bridge_jni_cocos_start(&library, 960, 544));
    assert(jni_runtime_find_native("org/cocos2dx/lib/Cocos2dxRenderer", "nativeRender", "()V") == NULL);
    *__counter(game, "mock_fail_on_load") = 0;
    __test_callback_failures(game, &library);
    dlclose(game);
}

static void __test_bitmap(const char* path)
{
    void* game = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    assert(game);
    dynalib_t library = {0};

    library.image = game;
    assert(bridge_jni_cocos_start(&library, 960, 544));
    jclass clazz = (*__env)->FindClass(__env, "org/cocos2dx/lib/Cocos2dxBitmap");
    const char* signature = "([BLjava/lang/String;IIIIIIIIZFFFFZIIIIFZI)Z";
    jmethodID method = (*__env)->GetStaticMethodID(__env, clazz, "createTextBitmapShadowStroke", signature);
    assert(method);
    jvalue args[23] = {0};

    args[0].l = (*__env)->NewByteArray(__env, 6);
    (*__env)->SetByteArrayRegion(__env, args[0].l, 0, 6, (const jbyte*)"Arcaea");
    args[1].l = (*__env)->NewStringUTF(__env, "Fonts/test.ttf");
    args[2].i = 24;
    args[3].i = 200;
    args[4].i = 150;
    args[5].i = 100;
    args[6].i = 255;
    args[7].i = 0x33;
    args[8].i = 240;
    args[9].i = 80;
    args[15].z = JNI_TRUE;
    args[19].i = 255;
    args[20].f = 2.f;
    assert((*__env)->CallStaticBooleanMethodA(__env, clazz, method, args));
    assert(*__counter(game, "mock_bitmap_count") == 1);
    int before = (*__env)->GetArrayLength(__env, args[0].l);
    assert(before == 6 && !(*__env)->ExceptionCheck(__env));
    args[8].i = INT32_MAX;
    assert(!(*__env)->CallStaticBooleanMethodA(__env, clazz, method, args));
    assert(*__counter(game, "mock_bitmap_count") == 1 && !(*__env)->ExceptionCheck(__env));
    args[8].i = 240;
    *__counter(game, "mock_bitmap_fail") = 1;
    assert(!(*__env)->CallStaticBooleanMethodA(__env, clazz, method, args));
    assert((*__env)->ExceptionCheck(__env));
    (*__env)->ExceptionClear(__env);
    *__counter(game, "mock_bitmap_fail") = 0;
    bridge_jni_cocos_stop();
    assert(!(*__env)->CallStaticBooleanMethodA(__env, clazz, method, args));
    assert(*__counter(game, "mock_bitmap_count") == 2);
    (*__env)->DeleteLocalRef(__env, args[0].l);
    (*__env)->DeleteLocalRef(__env, args[1].l);
    (*__env)->DeleteLocalRef(__env, clazz);
    dlclose(game);
}

static void __test_analytics(void)
{
    jclass analytics = (*__env)->FindClass(__env, "org/cocos2dx/cpp/AnalyticsManager");
    jmethodID start = (*__env)->GetStaticMethodID(__env, analytics, "trackTutorialStart", "()V");
    (*__env)->CallStaticVoidMethod(__env, analytics, start);
    jmethodID song = (*__env)->GetStaticMethodID(__env, analytics, "trackSongComplete", "(" STRING STRING ")V");
    jstring name = (*__env)->NewStringUTF(__env, "song %s");
    jvalue args[2] = {{.l = name}, {.l = NULL}};

    (*__env)->CallStaticVoidMethodA(__env, analytics, song, args);
    jmethodID memory = (*__env)->GetStaticMethodID(__env, analytics, "trackMemoryPurchase", "(I)V");
    (*__env)->CallStaticVoidMethod(__env, analytics, memory, 7);
    jclass activity = (*__env)->FindClass(__env, "org/cocos2dx/cpp/AppActivity");
    jmethodID purchase = (*__env)->GetStaticMethodID(__env, activity, "trackPurchaseStarted", "(" STRING "F)V");
    (*__env)->CallStaticVoidMethod(__env, activity, purchase, name, 1.25);
    assert(!(*__env)->ExceptionCheck(__env));
    jmethodID wrong = (*__env)->GetStaticMethodID(__env, analytics, "trackTutorialStart", "(I)V");
    (*__env)->CallStaticVoidMethod(__env, analytics, wrong, 7);
    assert((*__env)->ExceptionCheck(__env));
    (*__env)->ExceptionClear(__env);
    (*__env)->DeleteLocalRef(__env, name);
    (*__env)->DeleteLocalRef(__env, activity);
    (*__env)->DeleteLocalRef(__env, analytics);
}

static void __test_io_failure(void)
{
    assert(mkdir(PATH_TO_DATA "cocos-preferences.bin.tmp", 0700) == 0);
    (*__env)->CallStaticVoidMethod(
        __env, __helper, __method("setIntegerForKey", "(" STRING "I)V"), __string("score"), 123);
    assert((*__env)->ExceptionCheck(__env));
    (*__env)->ExceptionClear(__env);
    assert(rmdir(PATH_TO_DATA "cocos-preferences.bin.tmp") == 0);
    __read_preferences();
}

static void __test_corruption(void)
{
    static const unsigned char __truncated[] = {'V', 'A', 'R', 'C', 'P', 'R', '1', 0, 1, 0, 0, 0};

    static const unsigned char __invalid_length[] = {
        'V', 'A', 'R', 'C', 'P', 'R', '1', 0, 1, 0, 0, 0, 'I', 255, 255, 255, 255, 1, 0, 0, 0};

    static const unsigned char __invalid_type[] = {
        'V', 'A', 'R', 'C', 'P', 'R', '1', 0, 1, 0, 0, 0, 'X', 1, 0, 0, 0, 1, 0, 0, 0, 'x', '1'};

    static const unsigned char __duplicate[] = {'V',
        'A',
        'R',
        'C',
        'P',
        'R',
        '1',
        0,
        2,
        0,
        0,
        0,
        'I',
        1,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        'x',
        '1',
        'I',
        1,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        'x',
        '2'};

    typedef struct {
        const void* data;
        size_t size;
    } corrupt_t;

    static const corrupt_t __cases[] = {
        {"broken", 6},
        {__truncated, sizeof(__truncated)},
        {__invalid_length, sizeof(__invalid_length)},
        {__invalid_type, sizeof(__invalid_type)},
        {__duplicate, sizeof(__duplicate)},
    };

    for(size_t index = 0; index < sizeof(__cases) / sizeof(__cases[0]); ++index) {
        FILE* file = fopen(PATH_TO_DATA "cocos-preferences.bin", "wb");
        assert(file != NULL);
        assert(fwrite(__cases[index].data, 1, __cases[index].size, file) == __cases[index].size);
        assert(fclose(file) == 0);
        (*__env)->CallStaticVoidMethod(
            __env, __helper, __method("setIntegerForKey", "(" STRING "I)V"), __string("score"), 1);
        assert((*__env)->ExceptionCheck(__env));
        (*__env)->ExceptionClear(__env);
        file = fopen(PATH_TO_DATA "cocos-preferences.bin", "rb");
        assert(file != NULL);
        unsigned char actual[64];
        size_t size = fread(actual, 1, sizeof(actual), file);
        fclose(file);
        assert(size == __cases[index].size);
        assert(memcmp(actual, __cases[index].data, size) == 0);
    }
}

int main(int argc, char** argv)
{
    assert(argc == 3);
    const char* selected = getenv("VITAARC_TEST_VERSION_DIR");
    if(selected)
        assert(vita_version_select(selected));
    __env = jni_runtime_get_env();
    __helper = (*__env)->FindClass(__env, HELPER);
    jstring assets_path = (*__env)->CallStaticObjectMethod(__env, __helper, __method("getAssetsPath", "()" STRING));
    __expect_string(assets_path, PATH_TO_PACKAGE);
    (*__env)->DeleteLocalRef(__env, assets_path);
    if(strcmp(argv[1], "runtime") == 0) {
        __test_analytics();
        __test_animation_interval();
        __test_defaults();
        __test_strings_and_arrays();
        __test_services();
        __test_fields();
        __test_registration();
    } else if(strcmp(argv[1], "write") == 0) {
        __write_preferences();
        __test_save_file(true);
    } else if(strcmp(argv[1], "dispatch") == 0) {
        test_jni_dispatch();
    } else if(strcmp(argv[1], "loop") == 0) {
        test_application_loop(argv[2]);
    } else if(strcmp(argv[1], "references") == 0) {
        test_jni_references(argv[2]);
    } else if(strcmp(argv[1], "read") == 0) {
        __read_preferences();
        __test_save_file(false);
    } else if(strcmp(argv[1], "startup") == 0) {
        __test_startup(argv[2]);
    } else if(strcmp(argv[1], "bitmap") == 0) {
        __test_bitmap(argv[2]);
    } else if(strcmp(argv[1], "io_failure") == 0) {
        __test_io_failure();
    } else if(strcmp(argv[1], "corrupt") == 0) {
        __test_corruption();
    } else {
        assert(0);
    }

    assert(!(*__env)->ExceptionCheck(__env));
    return 0;
}
