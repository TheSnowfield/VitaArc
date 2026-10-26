#include <pthread.h>
#include <string.h>
#include <psp2/ctrl.h>
#include <psp2/gxm.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <vitaGL.h>

#include "screen.h"
#include "text.h"
#include "logo.h"

#define LOG_FRAME_US 66666u
#define MENU_POLL_US 10000u
#define MENU_REPEAT_DELAY_US 350000u
#define MENU_REPEAT_US 100000u

static pthread_mutex_t __mutex = PTHREAD_MUTEX_INITIALIZER;
static log_text_t __text;
static SceUID __owner;
static unsigned __width;
static unsigned __height;
static uint64_t __last;
static bool __active;
static bool __ready;
static bool __menu_visible;
static const char* const* __menu_names;
static unsigned __menu_count;
static unsigned __menu_selected;

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
    bool menu = __menu_visible;
    const char* const* names = __menu_names;
    unsigned count = __menu_count;
    unsigned selected = __menu_selected;
    pthread_mutex_unlock(&__mutex);

    unsigned pitch = (width + 63u) & ~63u;
    log_text_draw(&text, pixels, width, height, pitch);
    if(menu) {
        log_text_draw_menu(&text, names, count, selected, pixels, width, height, pitch);
    }
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
    if(level < ERROR && now - __last < LOG_FRAME_US) {
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
    const log_image_t* image = log_logo_open();
    pthread_mutex_lock(&__mutex);
    memset(&__text, 0, sizeof(__text));
    log_text_image(&__text, image);
    __active = true;
    __ready = false;
    __owner = 0;
    __last = 0;
    __menu_visible = false;
    __menu_names = NULL;
    __menu_count = 0;
    __menu_selected = 0;
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

static bool __can_select(void)
{
    pthread_mutex_lock(&__mutex);
    bool ready = __active && __ready && __owner == sceKernelGetThreadId();
    pthread_mutex_unlock(&__mutex);
    return ready;
}

static void __present(void)
{
    vglSwapBuffers(GL_FALSE);
    sceGxmDisplayQueueFinish();
}

bool log_screen_select(const char* const* names, unsigned count, unsigned* selected)
{
    if(!__can_select() || (count && (!names || !selected))) {
        return false;
    }

    unsigned choice = count && *selected < count ? *selected : 0;
    pthread_mutex_lock(&__mutex);
    __menu_names = names;
    __menu_count = count;
    __menu_selected = choice;
    __menu_visible = true;
    pthread_mutex_unlock(&__mutex);
    __present();

    /* A held confirm/cancel button must be released before it is accepted. */
    SceCtrlData pad;
    bool accepted = false;
    if(sceCtrlPeekBufferPositive(0, &pad, 1) > 0) {
        uint32_t previous = pad.buttons;
        uint64_t repeat = sceKernelGetProcessTimeWide() + MENU_REPEAT_DELAY_US;
        for(;;) {
            sceKernelDelayThread(MENU_POLL_US);
            if(sceCtrlPeekBufferPositive(0, &pad, 1) <= 0) {
                break;
            }

            uint32_t pressed = pad.buttons & ~previous;
            uint64_t now = sceKernelGetProcessTimeWide();
            uint32_t direction = pad.buttons & (SCE_CTRL_UP | SCE_CTRL_DOWN);
            uint32_t moved = pressed & (SCE_CTRL_UP | SCE_CTRL_DOWN);
            if(moved) {
                repeat = now + MENU_REPEAT_DELAY_US;
            } else if(direction && now >= repeat) {
                moved = direction;
                repeat = now + MENU_REPEAT_US;
            }

            previous = pad.buttons;
            if(pressed & SCE_CTRL_CIRCLE) {
                break;
            }

            if(count && (pressed & SCE_CTRL_CROSS)) {
                *selected = choice;
                accepted = true;
                break;
            }

            if(!count || !moved || moved == (SCE_CTRL_UP | SCE_CTRL_DOWN)) {
                continue;
            }

            if(moved & SCE_CTRL_UP) {
                choice = choice ? choice - 1 : count - 1;
            } else {
                choice = choice + 1 == count ? 0 : choice + 1;
            }

            pthread_mutex_lock(&__mutex);
            __menu_selected = choice;
            pthread_mutex_unlock(&__mutex);
            __present();
        }
    }

    pthread_mutex_lock(&__mutex);
    __menu_visible = false;
    __menu_names = NULL;
    pthread_mutex_unlock(&__mutex);
    /* Flush callbacks before the caller can release its version-name storage. */
    __present();
    return accepted;
}

void log_screen_wait(void)
{
    if(!__can_select()) {
        return;
    }

    log_i("startup", "Press Circle to exit.");
    __present();
    SceCtrlData pad;
    if(sceCtrlPeekBufferPositive(0, &pad, 1) <= 0) {
        return;
    }

    uint32_t previous = pad.buttons;
    for(;;) {
        sceKernelDelayThread(MENU_POLL_US);
        if(sceCtrlPeekBufferPositive(0, &pad, 1) <= 0 ||
            ((pad.buttons & SCE_CTRL_CIRCLE) && !(previous & SCE_CTRL_CIRCLE))) {
            return;
        }

        previous = pad.buttons;
    }
}

void log_screen_end(void)
{
    log_set_view(NULL);
    pthread_mutex_lock(&__mutex);
    bool ready = __ready;
    __ready = false;
    __active = false;
    pthread_mutex_unlock(&__mutex);
    if(ready) {
        sceGxmDisplayQueueFinish();
        vglSetDisplayCallback(NULL);
    }

    log_logo_close();
}

/* Keep the library's display infrastructure; replace only its boot animation. */
void __wrap_invoke_splashscreen(void)
{}
