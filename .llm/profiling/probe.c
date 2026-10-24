#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <vorbis/vorbisfile.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/touch.h>
#include <vitaGL.h>

#include <config.h>
#include <platform/loop.h>
#include <utils/debug.h>
#include <bridges/audio/backend.h>
#include <bridges/opengl/buffers.h>

#define MAILBOX PATH_TO_USER "profile/"

typedef struct {
    unsigned calls;
    uint64_t micros;
    unsigned peak;
    unsigned samples;
} stat_t;

enum {
    STEP, SWAP, AUDIO, DRAW, BIND, DATA, SUBDATA, ATTRIB, UNIFORM,
    ERROR_CHECK, VORBIS, STAT_COUNT
};

static stat_t __stats[STAT_COUNT];
static const char* const __names[] = {
    "render", "swap", "audio", "draw", "bind", "buffer-data", "buffer-subdata",
    "attrib", "uniform", "gl-error", "vorbis"
};
static pthread_mutex_t __mutex = PTHREAD_MUTEX_INITIALIZER;

static unsigned __frames;
static unsigned __command = 1;
static unsigned __settle;
static unsigned __touch;
static unsigned __pending;
static unsigned __x;
static unsigned __y;
static uint64_t __window;
void profile_buffers(void);

static void __time(unsigned slot, uint64_t start)
{
    unsigned micros = (unsigned)(sceKernelGetProcessTimeWide() - start);
    pthread_mutex_lock(&__mutex);
    stat_t* stat = &__stats[slot];
    ++stat->calls;
    stat->micros += micros;
    if(micros > stat->peak) {
        stat->peak = micros;
    }

    ++stat->samples;
    pthread_mutex_unlock(&__mutex);
}

static void __report(void)
{
    uint64_t now = sceKernelGetProcessTimeWide();
    stat_t snapshot[STAT_COUNT];
    pthread_mutex_lock(&__mutex);
    memcpy(snapshot, __stats, sizeof(snapshot));
    memset(__stats, 0, sizeof(__stats));
    pthread_mutex_unlock(&__mutex);
    sceClibPrintf("PROFILE frame=%u window_us=%llu\n", __frames, now - __window);
    for(unsigned index = 0; index < STAT_COUNT; ++index) {
        stat_t* stat = &snapshot[index];
        sceClibPrintf("PROFILE %s calls=%u us=%llu peak=%u samples=%u\n",
            __names[index], stat->calls, stat->micros, stat->peak, stat->samples);
    }

    __window = sceKernelGetProcessTimeWide();
    profile_buffers();
}

static void __control(void)
{
    if(__settle) {
        --__settle;
        return;
    }

    if(__pending) {
        char path[160];
        snprintf(path, sizeof(path), MAILBOX "%04u.rgba", __pending);
        bool saved = debug_capture_display_frame(path);
        snprintf(path, sizeof(path), MAILBOX "%04u.ack", __pending);
        FILE* file = fopen(path, "w");
        if(file) {
            fprintf(file, "frame=%u saved=%d\n", __frames, saved);
            fclose(file);
        }

        __pending = 0;
        return;
    }

    if(__frames % 30) {
        return;
    }

    char path[160];
    snprintf(path, sizeof(path), MAILBOX "%04u.cmd", __command);
    FILE* file = fopen(path, "r");
    if(!file) {
        return;
    }

    char op[16] = {0};
    unsigned x = 0, y = 0, wait = 120;
    int count = fscanf(file, "%15s %u %u %u", op, &x, &y, &wait);
    fclose(file);
    if(count < 1) {
        return;
    }

    if(!strcmp(op, "quit")) {
        __report();
        sceKernelExitProcess(0);
        return;
    }

    if(!strcmp(op, "tap") && count == 4) {
        __x = x * 2;
        __y = y * 2;
        __touch = 3;
    }

    __pending = __command++;
    __settle = wait;
}

int __wrap_sceTouchPeek(SceUInt32 port, SceTouchData* data, SceUInt32 count)
{
    if(!data || !count || port != SCE_TOUCH_PORT_FRONT) {
        return -1;
    }

    memset(data, 0, sizeof(*data));
    data->timeStamp = sceKernelGetProcessTimeWide();
    if(__touch) {
        --__touch;
        data->reportNum = 1;
        data->report[0].x = __x;
        data->report[0].y = __y;
    }

    return 1;
}

vita_frame_state_t __real_vita_frame_step(void);

vita_frame_state_t __wrap_vita_frame_step(void)
{
    __control();
    if(!__window) {
        __window = sceKernelGetProcessTimeWide();
    }

    uint64_t now = sceKernelGetProcessTimeWide();
    vita_frame_state_t result = __real_vita_frame_step();
    __time(STEP, now);
    if(++__frames % 300 == 0) {
        __report();
    }

    return result;
}

void __real_vglSwapBuffers(GLboolean wait);

void __wrap_vglSwapBuffers(GLboolean wait)
{
    uint64_t now = sceKernelGetProcessTimeWide();
    __real_vglSwapBuffers(wait);
    __time(SWAP, now);
}

bool __real_vita_audio_render(audio_t* state, int16_t* output, uint32_t frames);

bool __wrap_vita_audio_render(audio_t* state, int16_t* output, uint32_t frames)
{
    uint64_t now = sceKernelGetProcessTimeWide();
    bool result = __real_vita_audio_render(state, output, frames);
    __time(AUDIO, now);
    return result;
}

#define COUNTED(name, slot, args, call) \
    void __real_##name args; \
    void __wrap_##name args \
    { \
        bool sample = (__stats[slot].calls & 63) == 0; \
        uint64_t start = sample ? sceKernelGetProcessTimeWide() : 0; \
        __real_##name call; \
        if(sample) { \
            __time(slot, start); \
        } else { \
            ++__stats[slot].calls; \
        } \
    }

COUNTED(glDrawArrays, DRAW, (GLenum mode, GLint first, GLsizei count), (mode, first, count))
COUNTED(glDrawElements, DRAW, (GLenum mode, GLsizei count, GLenum type, const void* indices), (mode, count, type, indices))
COUNTED(bridge_gl_bind_buffer, BIND, (GLenum target, GLuint name), (target, name))
COUNTED(glBufferData, DATA, (GLenum target, GLsizeiptr size, const void* data, GLenum usage), (target, size, data, usage))
COUNTED(glBufferSubData, SUBDATA, (GLenum target, GLintptr offset, GLsizeiptr size, const void* data), (target, offset, size, data))
COUNTED(bridge_gl_vertex_attrib_pointer, ATTRIB,
    (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer),
    (index, size, type, normalized, stride, pointer))
COUNTED(glUniformMatrix4fv, UNIFORM, (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value),
    (location, count, transpose, value))

GLenum __real_bridge_gl_get_error(void);

GLenum __wrap_bridge_gl_get_error(void)
{
    ++__stats[ERROR_CHECK].calls;
    return __real_bridge_gl_get_error();
}

long __real_ov_read(OggVorbis_File* stream, char* data, int size, int bigendian, int word, int sign, int* link);

long __wrap_ov_read(OggVorbis_File* stream, char* data, int size, int bigendian, int word, int sign, int* link)
{
    uint64_t start = sceKernelGetProcessTimeWide();
    long result = __real_ov_read(stream, data, size, bigendian, word, sign, link);
    __time(VORBIS, start);
    return result;
}
