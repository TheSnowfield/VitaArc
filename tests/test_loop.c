#include <assert.h>
#include <dlfcn.h>
#include <stdbool.h>

#include <jni/jni.h>
#include <jni/runtime.h>
#include <platform/loop.h>

static bool __paused;
static bool __fail_input;
static int __poll_count;

void vita_input_poll(void)
{
    ++__poll_count;
    if(__fail_input) {
        JNIEnv* env = jni_runtime_get_env();
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), "input failure");
        bridge_jni_cocos_pause();
    }
}

bool vita_input_is_paused(void)
{
    return __paused;
}

void test_application_loop(const char* path)
{
    void* game = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    assert(game != NULL);
    dynalib_t library = {.image = game};

    int* renders = dlsym(game, "mock_render_count");
    int* failure = dlsym(game, "mock_fail_callback");
    assert(renders != NULL && failure != NULL);
    assert(vita_frame_step() == VITA_FRAME_FAILED && __poll_count == 0);
    assert(bridge_jni_cocos_start(&library, 960, 544));
    assert(vita_frame_step() == VITA_FRAME_READY && *renders == 1);
    __paused = true;
    assert(vita_frame_step() == VITA_FRAME_PAUSED && *renders == 1);
    __paused = false;
    *failure = 1;
    assert(vita_frame_step() == VITA_FRAME_FAILED && *renders == 2);
    int polls = __poll_count;
    assert(vita_frame_step() == VITA_FRAME_FAILED && __poll_count == polls);
    JNIEnv* env = jni_runtime_get_env();
    assert((*env)->ExceptionCheck(env));
    (*env)->ExceptionClear(env);
    *failure = 0;
    assert(bridge_jni_cocos_start(&library, 960, 544));
    __fail_input = true;
    assert(vita_frame_step() == VITA_FRAME_FAILED && *renders == 2);
    assert((*env)->ExceptionCheck(env));
    (*env)->ExceptionClear(env);
    bridge_jni_cocos_stop();
    dlclose(game);
}
