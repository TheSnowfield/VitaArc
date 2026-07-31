/* Headless game-side input testing. This replaces only sceTouchPeek with
 * synthetic panel reports; production input scaling, JNI and the game run.
 * It does not validate Windows mouse input, host presentation or hardware. */
#include <stdio.h>
#include <string.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/touch.h>
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
#include <platform/input.h>
#include <platform/loop.h>
#include <platform/timing.h>
#include <utils/debug.h>

#define PROBE_PATH PATH_TO_USER "touch-probe/"
static SceTouchPanelInfo panel;
static unsigned touches, touch_frame, touch_duration;
static float start_x[2], start_y[2], end_x[2], end_y[2];

int __wrap_sceTouchPeek(SceUInt32 port, SceTouchData* data, SceUInt32 count)
{
    if(port != SCE_TOUCH_PORT_FRONT || !data || !count)
        return -1;

    memset(data, 0, sizeof(*data));
    data->timeStamp = sceKernelGetProcessTimeWide();
    if(touch_frame < touch_duration) {
        data->reportNum = touches;
        float t = touch_duration > 1 ? (float)touch_frame / (touch_duration - 1) : 0;
        for(unsigned i = 0; i < touches; ++i) {
            float x = start_x[i] + (end_x[i] - start_x[i]) * t;
            float y = start_y[i] + (end_y[i] - start_y[i]) * t;
            data->report[i].id = i;
            data->report[i].x = panel.minDispX + x * (panel.maxDispX - panel.minDispX) / 960.0f;
            data->report[i].y = panel.minDispY + y * (panel.maxDispY - panel.minDispY) / 544.0f;
        }

        ++touch_frame;
    }

    return 1;
}

static bool point_valid(float x, float y)
{
    return x >= 0 && x < 960 && y >= 0 && y < 544;
}

static bool checkpoint(unsigned id, unsigned frame)
{
    char path[128], line[192];
    snprintf(path, sizeof(path), PROBE_PATH "%04u.rgba", id);
    if(!debug_capture_display_frame(path))
        return false;

    jni_runtime_stats_t stats;
    jni_runtime_get_stats(&stats);
    snprintf(line,
        sizeof(line),
        "id=%u frame=%u objects=%u references=%u ready=%d\n",
        id,
        frame,
        (unsigned)stats.objects,
        (unsigned)stats.references,
        bridge_jni_cocos_is_ready());
    snprintf(path, sizeof(path), PROBE_PATH "%04u.ack", id);
    SceUID file = sceIoOpen(path, SCE_O_CREAT | SCE_O_WRONLY | SCE_O_TRUNC, 0666);
    if(file < 0)
        return false;

    int length = strlen(line);
    bool saved = sceIoWrite(file, line, length) == length;
    sceIoClose(file);
    sceClibPrintf("VitaArc touch probe: %s", line);
    return saved;
}

/* One command per numbered file. Stop reading until its checkpoint is saved. */
static int command(unsigned id, unsigned* settle)
{
    char path[128], text[160] = {0}, op[16];
    snprintf(path, sizeof(path), PROBE_PATH "%04u.cmd", id);
    SceUID file = sceIoOpen(path, SCE_O_RDONLY, 0);
    if(file < 0)
        return 0;

    int length = sceIoRead(file, text, sizeof(text) - 1);
    sceIoClose(file);
    if(length <= 0 || sscanf(text, "%15s", op) != 1)
        return -1;

    touch_frame = 0;
    touches = 0;
    touch_duration = 0;
    *settle = 120;
    if(!strcmp(op, "quit"))
        return 2;

    if(!strcmp(op, "capture")) {
        if(sscanf(text, "%*s %u", settle) != 1)
            return -1;
    } else if(!strcmp(op, "tap")) {
        if(sscanf(text, "%*s %f %f %u", &start_x[0], &start_y[0], settle) != 3)
            return -1;

        end_x[0] = start_x[0];
        end_y[0] = start_y[0];
        touches = 1;
        touch_duration = 3;
    } else if(!strcmp(op, "drag")) {
        if(sscanf(text,
               "%*s %f %f %f %f %u %u",
               &start_x[0],
               &start_y[0],
               &end_x[0],
               &end_y[0],
               &touch_duration,
               settle) != 6)
            return -1;

        touches = 1;
    } else if(!strcmp(op, "pair")) {
        if(sscanf(text,
               "%*s %f %f %f %f %u %u",
               &start_x[0],
               &start_y[0],
               &start_x[1],
               &start_y[1],
               &touch_duration,
               settle) != 6)
            return -1;

        memcpy(end_x, start_x, sizeof(end_x));
        memcpy(end_y, start_y, sizeof(end_y));
        touches = 2;
    } else

        return -1;

    if(*settle < 1 || *settle > 600 || touch_duration > 600)
        return -1;

    if(touches && !touch_duration)
        return -1;

    for(unsigned i = 0; i < touches; ++i)
        if(!point_valid(start_x[i], start_y[i]) || !point_valid(end_x[i], end_y[i]))
            return -1;

    sceClibPrintf("VitaArc touch probe: command %u %s", id, text);
    return 1;
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
    if(!bridge_shader_enable_offline() && shark_init(NULL) < 0 && shark_init("ur0:data/external/libshacccg.suprx") < 0)
        goto done;

    bridge_compat_initialize();
    dynalib_t* game = loader_load_library(LIBRARY_LIBCOCOS2DCPP);
    if(!game)
        goto done;

    bridge_patch_gl(game);
    bridge_patch_cocos2dx(game);
    loader_flush_caches(game);
    if(!loader_init_library(game) || !bridge_cocos_configure_storage(game) || !bridge_jni_cocos_start(game, 960, 544) ||
        !vita_input_initialize())
        goto stop;

    memset(&panel, 0, sizeof(panel));
    if(sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &panel) < 0 || panel.maxDispX <= panel.minDispX ||
        panel.maxDispY <= panel.minDispY) {
        panel.minDispX = panel.minDispY = 0;
        panel.maxDispX = 1920;
        panel.maxDispY = 1088;
    }

    unsigned next_id = 1, pending_id = 0, capture_at = 600;
    sceClibPrintf("VitaArc touch probe: real game started\n");
    for(unsigned frame = 1; frame <= 36000; ++frame) {
        uint64_t started = sceKernelGetProcessTimeWide();
        if(!capture_at && frame % 15 == 0) {
            unsigned settle;
            int action = command(next_id, &settle);
            if(action < 0)
                goto stop;

            if(action == 2) {
                result = 0;
                goto stop;
            }

            if(action == 1) {
                pending_id = next_id++;
                capture_at = frame + touch_duration + settle;
            }
        }

        if(vita_frame_step() != VITA_FRAME_READY)
            goto stop;

        vglSwapBuffers(GL_TRUE);
        if(capture_at == frame) {
            if(!checkpoint(pending_id, frame))
                goto stop;

            capture_at = 0;
        }

        uint32_t delay = vita_frame_delay(started, sceKernelGetProcessTimeWide());
        if(delay)
            sceKernelDelayThread(delay);
    }

    sceClibPrintf("VitaArc touch probe: bounded frame limit reached\n");
stop:
    bridge_jni_cocos_stop();
    glFinish();
    loader_free_library(game);
    if(loader_find_library("libcocos2dcpp.so"))
        result = 2;
    else {
        glFinish();
        bridge_gl_shutdown_buffers();
    }

done:
    sceClibPrintf("VitaArc touch probe: exit=%d\n", result);
    log_end();
    sceKernelExitProcess(result);
    return result;
}
