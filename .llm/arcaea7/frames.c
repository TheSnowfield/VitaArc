#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/touch.h>
#include <vitaGL.h>
#include <platform/loop.h>

#define ROOT "ux0:data/vitaarc/profile/"

static unsigned frame, serial = 1, wait, touch, x, y;
static int capture;
static unsigned touch_id = 1;
static unsigned release;
void arc_asset_profile(void);

void* __real_malloc(size_t size);
void* __real_calloc(size_t count, size_t size);

static void __allocation(size_t size)
{
    struct mallinfo heap = mallinfo();
    sceClibPrintf("ARCAE7 allocation failed bytes=%u used=%u free=%u arena=%u\n", size,
        (unsigned)heap.uordblks, (unsigned)heap.fordblks, (unsigned)heap.arena);
}

void* __wrap_malloc(size_t size)
{
    void* result = __real_malloc(size);
    if(!result && size) __allocation(size);
    return result;
}

void* __wrap_calloc(size_t count, size_t size)
{
    void* result = __real_calloc(count, size);
    if(!result && count && size) __allocation(count * size);
    return result;
}

vita_frame_state_t __real_vita_frame_step(void);

vita_frame_state_t __wrap_vita_frame_step(void)
{
    if(frame < 10) sceClibPrintf("ARCAE7 frame-enter %u\n", frame);
    char path[128];
    if(!wait && !touch && !release && !capture) {
        snprintf(path, sizeof(path), ROOT "%04u.cmd", serial);
        FILE* input = fopen(path, "r");
        if(input) {
            char command[20];
            unsigned delay;
            if(fscanf(input, "%19s %u %u %u", command, &x, &y, &delay) == 4) {
                wait = delay;
                capture = 1;
                if(!strncmp(command, "tap", 3)) {
                    touch = 3;
                    touch_id = command[3] ? strtoul(command + 3, NULL, 10) : 1;
                }
                if(!strcmp(command, "quit")) sceKernelExitProcess(0);
            }
            fclose(input);
        }
    }
    vita_frame_state_t state = __real_vita_frame_step();
    if(frame < 10 || frame % 300 == 0) sceClibPrintf("ARCAE7 frame-exit %u %d\n", frame, state);
    ++frame;
    if(touch) --touch;
    if(wait) --wait;
    if(capture && !touch && !wait) {
        unsigned char* rgba = malloc(960 * 544 * 4);
        glReadPixels(0, 0, 960, 544, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        snprintf(path, sizeof(path), ROOT "%04u.rgba", serial);
        FILE* output = fopen(path, "wb");
        if(output) { fwrite(rgba, 1, 960 * 544 * 4, output); fclose(output); }
        free(rgba);
        snprintf(path, sizeof(path), ROOT "%04u.ack", serial);
        output = fopen(path, "w");
        if(output) { fprintf(output, "%u\n", frame); fclose(output); }
        sceClibPrintf("ARCAE7 capture %u frame=%u\n", serial, frame);
        arc_asset_profile();
        ++serial;
        capture = 0;
    }
    return state;
}

int __real_sceTouchPeek(SceUInt32 port, SceTouchData* data, SceUInt32 count);

int __wrap_sceTouchPeek(SceUInt32 port, SceTouchData* data, SceUInt32 count)
{
    int result = __real_sceTouchPeek(port, data, count);
    if(!port && data && count && touch) {
        memset(data, 0, sizeof(*data));
        data->reportNum = 1;
        data->report[0].id = touch_id;
        data->report[0].x = x * 2;
        data->report[0].y = y * 2;
        data->report[0].force = 128;
        release = 1;
        return 1;
    }

    if(!port && data && count && release) {
        memset(data, 0, sizeof(*data));
        release = 0;
        return 1;
    }

    return result;
}
