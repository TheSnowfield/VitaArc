/* Bounded real-game startup, lifecycle callbacks and production unload.
 * This does not establish host presentation, physical input or gameplay. */
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <vitaGL.h>
#include <vitashark.h>

#include <config.h>
#include <loader.h>
#include <logcat/logcat.h>
#include <bridges/cocos2dx/cocos2dx.h>
#include <bridges/compat/compat.h>
#include <bridges/jni/jni.h>
#include <bridges/jni/runtime.h>
#include <bridges/opengl/opengl.h>
#include <bridges/opengl/shader_cache.h>
#include <platform/timing.h>
#include <malloc.h>

#ifndef VITAARC_LIFECYCLE_FRAMES
#define VITAARC_LIFECYCLE_FRAMES 1200
#endif
#define SNAPSHOT_FRAMES 600
#define STEADY_FROM 1800

static jni_runtime_stats_t snapshot(unsigned frame)
{
    jni_runtime_stats_t stats;
    jni_runtime_get_stats(&stats);
    struct mallinfo heap = mallinfo();
    sceClibPrintf("VitaArc lifecycle probe: frame=%u objects=%u references=%u heap=%lu\n",
        frame,
        (unsigned)stats.objects,
        (unsigned)stats.references,
        (unsigned long)heap.uordblks);
    return stats;
}

int main(void)
{
    int result = 1;
    log_begin(PATH_TO_LOGFILE);
    vglSetupRuntimeShaderCompiler(SHARK_OPT_UNSAFE, SHARK_ENABLE, SHARK_ENABLE, SHARK_ENABLE);
    vglSetVDMBufferSize(128 * 1024);
    vglSetVertexBufferSize(2 * 1024 * 1024);
    vglSetFragmentBufferSize(512 * 1024);
    vglSetUSSEBufferSize(16 * 1024);
    vglInitExtended(0, 960, 544, 24 * 1024 * 1024, SCE_GXM_MULTISAMPLE_NONE);
    if(!bridge_shader_enable_offline() && shark_init(NULL) < 0 &&
        shark_init("ur0:data/external/libshacccg.suprx") < 0) {
        goto done;
    }

    bridge_compat_initialize();
    dynalib_t* game = loader_load_library(LIBRARY_LIBCOCOS2DCPP);
    if(!game) {
        goto done;
    }

    bridge_patch_gl(game);
    bridge_patch_cocos2dx(game);
    loader_flush_caches(game);
    if(!loader_init_library(game) || !bridge_cocos_configure_storage(game) || !bridge_jni_cocos_start(game, 960, 544)) {
        goto stop;
    }

    sceClibPrintf("VitaArc lifecycle probe: real game started\n");
    jni_runtime_stats_t steady = {0};

    for(unsigned frame = 0; frame < VITAARC_LIFECYCLE_FRAMES; ++frame) {
        uint64_t started = sceKernelGetProcessTimeWide();
        bridge_jni_cocos_render();
        if(!bridge_jni_cocos_is_ready()) {
            goto stop;
        }

        vglSwapBuffers(GL_TRUE);
        uint32_t delay = vita_frame_delay(started, sceKernelGetProcessTimeWide());
        if(delay) {
            sceKernelDelayThread(delay);
        }

        if((frame + 1) % SNAPSHOT_FRAMES == 0) {
            jni_runtime_stats_t current = snapshot(frame + 1);
            if(frame + 1 == STEADY_FROM) {
                steady = current;
            } else if(frame + 1 > STEADY_FROM &&

                (current.objects != steady.objects || current.references != steady.references)) {
                sceClibPrintf("VitaArc lifecycle probe: JNI counts changed after warmup\n");
                goto stop;
            }
        }

        if((frame + 1) % SNAPSHOT_FRAMES == 0 && frame + 1 < VITAARC_LIFECYCLE_FRAMES) {
            bridge_jni_cocos_pause();
            if(!bridge_jni_cocos_is_ready()) {
                goto stop;
            }

            sceKernelDelayThread(100000);
            bridge_jni_cocos_resume();
            if(!bridge_jni_cocos_is_ready()) {
                goto stop;
            }

            sceClibPrintf("VitaArc lifecycle probe: pause/resume passed\n");
        }
    }

    if(VITAARC_LIFECYCLE_FRAMES >= STEADY_FROM + SNAPSHOT_FRAMES) {
        sceClibPrintf("VitaArc lifecycle probe: steady JNI counts passed\n");
    }

    bridge_jni_cocos_pause();
    if(!bridge_jni_cocos_is_ready()) {
        goto stop;
    }

    result = 0;
stop:
    bridge_jni_cocos_stop();
    glFinish();
    sceClibPrintf("VitaArc lifecycle probe: production unload begin\n");
    loader_free_library(game);
    if(loader_find_library("libcocos2dcpp.so")) {
        sceClibPrintf("VitaArc lifecycle probe: game mappings retained; unload not proven\n");
        result = 2;
    } else {
        glFinish();
        bridge_gl_shutdown_buffers();
        sceClibPrintf("VitaArc lifecycle probe: production unload completed\n");
    }

done:
    sceClibPrintf("VitaArc lifecycle probe: exit=%d\n", result);
    log_end();
    sceKernelExitProcess(result);
    return result;
}
