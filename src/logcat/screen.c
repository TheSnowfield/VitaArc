#include <pthread.h>
#include <string.h>
#include <psp2/gxm.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <vitaGL.h>

#include "screen.h"
#include "text.h"

#define LOG_FRAME_US 66666u

static pthread_mutex_t __mutex = PTHREAD_MUTEX_INITIALIZER;
static log_text_t __text;
static SceUID __owner;
static unsigned __width;
static unsigned __height;
static uint64_t __last;
static bool __active;
static bool __ready;

/* Called by vitaGL after GPU completion, before the surface reaches display. */
static void __draw(void* pixels)
{
    pthread_mutex_lock(&__mutex);
    if(!__active) {
        pthread_mutex_unlock(&__mutex);
        return;
    }

    log_text_t text = __text;
    unsigned width = __width;
    unsigned height = __height;
    pthread_mutex_unlock(&__mutex);

    unsigned pitch = (width + 63u) & ~63u;
    log_text_draw(&text, pixels, width, height, pitch);
}

static void __write(log_level_t level, const char* data, size_t size)
{
    pthread_mutex_lock(&__mutex);
    if(!__active) {
        pthread_mutex_unlock(&__mutex);
        return;
    }

    log_text_write(&__text, level, data, size);
    bool ready = __ready;
    SceUID owner = __owner;
    pthread_mutex_unlock(&__mutex);
    if(!ready || sceKernelGetThreadId() != owner) {
        return;
    }

    uint64_t now = sceKernelGetProcessTimeWide();
    if(level == VERBOSE && now - __last < LOG_FRAME_US) {
        return;
    }

    __last = now;
    vglSwapBuffers(GL_FALSE);
    if(level >= ERROR) {
        sceGxmDisplayQueueFinish();
    }
}

void log_screen_begin(void)
{
    pthread_mutex_lock(&__mutex);
    memset(&__text, 0, sizeof(__text));
    __active = true;
    __ready = false;
    __owner = 0;
    __last = 0;
    pthread_mutex_unlock(&__mutex);
    log_set_view(__write);
}

void log_screen_ready(void)
{
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    if(viewport[2] < 32 || viewport[3] < 32 || viewport[2] > 4096 || viewport[3] > 4096) {
        return;
    }

    pthread_mutex_lock(&__mutex);
    __width = (unsigned)viewport[2];
    __height = (unsigned)viewport[3];
    __owner = sceKernelGetThreadId();
    __ready = true;
    pthread_mutex_unlock(&__mutex);

    vglSetDisplayCallback(__draw);
    vglSwapBuffers(GL_FALSE);
}

void log_screen_end(void)
{
    log_set_view(NULL);
    pthread_mutex_lock(&__mutex);
    bool ready = __ready;
    __ready = false;
    __active = false;
    pthread_mutex_unlock(&__mutex);
    if(!ready) {
        return;
    }

    sceGxmDisplayQueueFinish();
    vglSetDisplayCallback(NULL);
}

/* Keep the library's display infrastructure; replace only its boot animation. */
void __wrap_invoke_splashscreen(void)
{}
