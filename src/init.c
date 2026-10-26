#include <stdio.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/display.h>
#include <psp2/power.h>
#include <psp2/touch.h>
#include <vitaGL.h>
#include <vitashark.h>

#include "bridges/jni/jni.h"
#include "bridges/opengl/opengl.h"
#include "bridges/opengl/shader_cache.h"
#include "bridges/cocos2dx/cocos2dx.h"
#include "bridges/compat/compat.h"

#include "loader.h"
#include "logcat/logcat.h"
#include "logcat/screen.h"
#include "platform/input.h"
#include "platform/loop.h"
#include "platform/memory.h"
#include "platform/perf.h"
#include "platform/timing.h"
#include "config.h"
#include "utils/debug.h"
#include "utils/fs.h"

static void __clocks(void)
{
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);
}

static void log_startup_heap(const char* stage)
{
    vita_memory_log(stage);
}

typedef struct {
    dynalib_t* library;
    uint32_t frames;
    uint32_t poll;
    bool capture;
} vita_app_t;

static bool __video(void)
{
    vglSetupRuntimeShaderCompiler(SHARK_OPT_UNSAFE, SHARK_ENABLE, SHARK_ENABLE, SHARK_ENABLE);
    vglSetVDMBufferSize(128 * 1024);
    vglSetVertexBufferSize(2 * 1024 * 1024);
    vglSetFragmentBufferSize(512 * 1024);
    vglSetUSSEBufferSize(16 * 1024);
    if(!vita_memory_setup()) {
        return false;
    }

    log_i(TAG, "VitaGL initialized");
    vita_memory_log("graphics");
    return true;
}

static bool __shaders(void)
{
    /* GLSL compilation can be deferred until linking. Check availability before
     * loading a game that would otherwise fail later in GPU program creation. */
    if(bridge_shader_enable_offline()) {
        log_i(TAG, "Selected offline shader pack; skipping runtime compiler initialization");
        return true;
    }

    int result = shark_init(NULL);
    if(result < 0) {
        result = shark_init("ur0:data/external/libshacccg.suprx");
    }

    if(result < 0) {
        log_f(TAG,
            "Shader compiler unavailable: libshacccg.suprx is required in ur0:data or ur0:data/external, or install an "
            "offline shader pack");
        return false;
    }

    log_i(TAG, "No offline shader pack selected; using runtime compiler");
    return true;
}

static bool __boot(vita_app_t* app)
{
    log_startup_heap("before-library");
    bridge_compat_initialize();
    app->library = loader_load_library(LIBRARY_LIBCOCOS2DCPP);
    dynalib_t* cocos = app->library;
    if(!cocos) {
        log_f(TAG, "Load cocos2dx library failed");
        return false;
    }

    bridge_patch_gl(cocos);
    bridge_patch_cocos2dx(cocos);
    if(!bridge_cocos_configure_textures(cocos)) {
        log_f(TAG, "Texture profile initialization failed");
        return false;
    }

    loader_flush_caches(cocos);
    if(!loader_init_library(cocos)) {
        log_f(TAG, "Cocos constructor initialization failed");
        return false;
    }

    log_startup_heap("after-constructors");
    if(!bridge_cocos_configure_storage(cocos)) {
        log_f(TAG, "Game database initialization failed");
        return false;
    }

    log_startup_heap("before-JNI");
    if(!bridge_jni_cocos_start(cocos, 960, 544)) {
        log_f(TAG, "Cocos JNI initialization failed");
        return false;
    }

    log_startup_heap("after-JNI");
    if(!vita_input_initialize()) {
        log_f(TAG, "Vita input initialization failed");
        return false;
    }

    return true;
}

static void __present(vita_app_t* app)
{
    if(app->frames <= 1800) {
        ++app->frames;
    }

    if(app->frames == 120) {
        app->capture = util_file_exists(PATH_TO_USER "capture-frame");
    }

    bool capture_display = false;
    if((app->frames == 120 || app->frames == 1800) && app->capture) {
        char path[128];
        snprintf(path, sizeof(path), PATH_TO_USER "frame-%u.rgba", app->frames);
        bool captured = debug_capture_frame(path, 960, 544);
        capture_display = captured;
        log_i(TAG, "Cocos frame %u capture %s", app->frames, captured ? "saved" : "failed");
    }

    if(app->capture && ++app->poll == 120) {
        app->poll = 0;
        // Opt-in native GPU diagnostics can inspect a later input result without
        // restarting the game. Never poll the filesystem in normal operation.
        if(util_file_exists(PATH_TO_USER "capture-next-frame")) {
            bool captured = debug_capture_frame(PATH_TO_USER "frame-live.rgba", 960, 544);
            capture_display = capture_display || captured;
            if(captured) {
                sceIoRemove(PATH_TO_USER "capture-next-frame");
            }

            log_i(TAG, "Requested Cocos frame capture %s", captured ? "saved" : "failed");
        }
    }

    vglSwapBuffers(GL_TRUE);
    if(capture_display) {
        bool captured = debug_capture_display_frame(PATH_TO_USER "frame-displayed.rgba");
        log_i(TAG, "Displayed Cocos frame capture %s", captured ? "saved" : "failed");
    }

    if(app->frames == 1 || app->frames == 120) {
        log_i(TAG, "Presented Cocos frame %u", app->frames);
    }
}

static void __loop(vita_app_t* app)
{
    for(;;) {
        uint64_t frame_started = sceKernelGetProcessTimeWide();
        vita_frame_state_t frame = vita_frame_step();
        if(frame == VITA_FRAME_FAILED) {
            log_e(TAG, "Cocos stopped after a runtime error; shutting down");
            return;
        }

        if(frame == VITA_FRAME_PAUSED) {
            sceKernelDelayThread(16 * 1000);
            continue;
        }

        uint64_t rendered = sceKernelGetProcessTimeWide();
        __present(app);
        vita_perf_frame(frame_started, rendered, sceKernelGetProcessTimeWide());
        vita_memory_tick(frame_started);

        // Count render and swap/vsync time toward Java's requested frame interval.
        uint32_t delay = vita_frame_delay(frame_started, sceKernelGetProcessTimeWide());
        if(delay) {
            sceKernelDelayThread(delay);
        }
    }
}

static void __stop(vita_app_t* app)
{
    bridge_jni_cocos_stop();
    glFinish();
    loader_free_library(app->library);
    bridge_cocos_release_textures();
    glFinish();
    bridge_gl_shutdown_buffers();
}

static bool __select_version(void)
{
    vita_versions_t versions = {0};
    if(!vita_versions_scan(&versions)) {
        log_e(TAG, "Cannot scan game versions: %s", PATH_TO_VERSIONS);
        log_screen_wait();
        return false;
    }

    unsigned selected = 0;
    bool result = log_screen_select((const char* const*)versions.names, versions.count, &selected);
    if(result) {
        result = selected < versions.count && vita_version_select(versions.names[selected]);
        if(result) {
            log_i(TAG, "Selected version: %s", versions.names[selected]);
            log_i(TAG, "Game resources: %s", PATH_TO_PACKAGE);
        } else {
            log_e(TAG, "Selected game directory is unavailable");
            log_screen_wait();
        }
    }

    vita_versions_free(&versions);
    return result;
}

int main(void)
{
    vita_app_t app = {0};

    log_screen_begin();
    sceIoMkdir(PATH_TO_USER, 0777);
    log_begin(PATH_TO_LOGFILE);
    log_i(TAG, "Startup policy: prefer packaged offline shaders before runtime compiler");
    __clocks();
    bool video = __video();
    if(video) {
        log_screen_ready();
    }

    bool ready = false;
    bool selected = video && __select_version();
    if(selected && __shaders()) {
        ready = __boot(&app);
    }

    log_flush();
    if(selected && !ready) {
        log_screen_wait();
    }

    log_screen_end();
    if(ready) {
        __loop(&app);
    }

    if(app.library) {
        __stop(&app);
    }

    log_end();
    sceKernelExitProcess(1);
    return 1;
}
