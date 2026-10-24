#define __wrap_vita_frame_step profile_frame_base
#include "../profiling/probe.c"
#undef __wrap_vita_frame_step

#include <psp2/audiodec.h>
#include <stdatomic.h>
#include <bridges/audio/feed.h>
#include <bridges/audio/mp3.h>
#include <bridges/audio/mp3_index.h>

static atomic_uint __decodes;
static unsigned __loads;
static unsigned __uploads;
static uint64_t __upload_us;

vita_frame_state_t __wrap_vita_frame_step(void)
{
    uint64_t start = sceKernelGetProcessTimeWide();
    unsigned loads = __loads;
    __uploads = 0;
    __upload_us = 0;
    vita_frame_state_t result = profile_frame_base();
    unsigned elapsed = (unsigned)(sceKernelGetProcessTimeWide() - start);
    if(elapsed > 50000) {
        sceClibPrintf("SWITCH slow-frame frame=%u us=%u\n", __frames, elapsed);
    }

    if(loads != __loads) {
        sceClibPrintf("SWITCH load-frame frame=%u us=%u uploads=%u upload_us=%llu\n",
            __frames, elapsed, __uploads, __upload_us);
    }

    return result;
}

int __real_sceAudiodecDecode(SceAudiodecCtrl* control);

int __wrap_sceAudiodecDecode(SceAudiodecCtrl* control)
{
    atomic_fetch_add(&__decodes, 1);
    return __real_sceAudiodecDecode(control);
}

bool __real_vita_mp3_seek_frames(mp3_stream_t* stream, uint64_t position);

bool __wrap_vita_mp3_seek_frames(mp3_stream_t* stream, uint64_t position)
{
    unsigned before = atomic_load(&__decodes);
    uint64_t start = sceKernelGetProcessTimeWide();
    bool result = __real_vita_mp3_seek_frames(stream, position);
    unsigned elapsed = (unsigned)(sceKernelGetProcessTimeWide() - start);
    unsigned count = atomic_load(&__decodes) - before;
    sceClibPrintf("SWITCH mp3-seek target=%llu us=%u decodes=%u result=%d\n", position, elapsed, count, result);
    return result;
}

mp3_stream_t* __real_vita_mp3_open(const char* path);

mp3_stream_t* __wrap_vita_mp3_open(const char* path)
{
    uint64_t start = sceKernelGetProcessTimeWide();
    mp3_stream_t* stream = __real_vita_mp3_open(path);
    sceClibPrintf("SWITCH mp3-open path=%s us=%u\n", path, (unsigned)(sceKernelGetProcessTimeWide() - start));
    return stream;
}

bool __real_audio_feed_seek(audio_feed_t* feed, uint64_t position, bool loop);

bool __wrap_audio_feed_seek(audio_feed_t* feed, uint64_t position, bool loop)
{
    uint64_t start = sceKernelGetProcessTimeWide();
    bool result = __real_audio_feed_seek(feed, position, loop);
    sceClibPrintf("SWITCH feed-seek target=%llu us=%u result=%d\n", position,
        (unsigned)(sceKernelGetProcessTimeWide() - start), result);
    return result;
}

void __real_audio_feed_close(audio_feed_t* feed);

void __wrap_audio_feed_close(audio_feed_t* feed)
{
    uint64_t start = sceKernelGetProcessTimeWide();
    __real_audio_feed_close(feed);
    unsigned elapsed = (unsigned)(sceKernelGetProcessTimeWide() - start);
    if(elapsed > 1000) {
        sceClibPrintf("SWITCH feed-close us=%u\n", elapsed);
    }
}

bool __real_vita_audio_load_bgm(audio_t* state, const char* path, int channel);

bool __wrap_vita_audio_load_bgm(audio_t* state, const char* path, int channel)
{
    ++__loads;
    uint64_t start = sceKernelGetProcessTimeWide();
    bool result = __real_vita_audio_load_bgm(state, path, channel);
    sceClibPrintf("SWITCH bgm-load channel=%d path=%s us=%u result=%d\n", channel, path,
        (unsigned)(sceKernelGetProcessTimeWide() - start), result);
    return result;
}

void __real_vita_audio_play_bgm(audio_t* state, bool loop, int channel, int delay);

void __wrap_vita_audio_play_bgm(audio_t* state, bool loop, int channel, int delay)
{
    int before = vita_audio_get_bgm_position(state, channel);
    uint64_t start = sceKernelGetProcessTimeWide();
    __real_vita_audio_play_bgm(state, loop, channel, delay);
    sceClibPrintf("SWITCH bgm-play channel=%d before=%d after=%d us=%u\n", channel, before,
        vita_audio_get_bgm_position(state, channel), (unsigned)(sceKernelGetProcessTimeWide() - start));
}

bool __real_mp3_index_find(mp3_index_t* index, FILE* file, const mp3_frame_t* format,
    uint32_t target, long* offset, uint32_t* frame);

bool __wrap_mp3_index_find(mp3_index_t* index, FILE* file, const mp3_frame_t* format,
    uint32_t target, long* offset, uint32_t* frame)
{
    uint64_t start = sceKernelGetProcessTimeWide();
    bool result = __real_mp3_index_find(index, file, format, target, offset, frame);
    sceClibPrintf("SWITCH index target=%u us=%u result=%d\n", target,
        (unsigned)(sceKernelGetProcessTimeWide() - start), result);
    return result;
}

int __real_sceAudiodecCreateDecoder(SceAudiodecCtrl* control, unsigned codec);

int __wrap_sceAudiodecCreateDecoder(SceAudiodecCtrl* control, unsigned codec)
{
    uint64_t start = sceKernelGetProcessTimeWide();
    int result = __real_sceAudiodecCreateDecoder(control, codec);
    sceClibPrintf("SWITCH codec-create us=%u result=%d\n", (unsigned)(sceKernelGetProcessTimeWide() - start), result);
    return result;
}

int __real_sceAudiodecDeleteDecoder(SceAudiodecCtrl* control);

int __wrap_sceAudiodecDeleteDecoder(SceAudiodecCtrl* control)
{
    uint64_t start = sceKernelGetProcessTimeWide();
    int result = __real_sceAudiodecDeleteDecoder(control);
    sceClibPrintf("SWITCH codec-delete us=%u result=%d\n", (unsigned)(sceKernelGetProcessTimeWide() - start), result);
    return result;
}

int __real_pthread_join(pthread_t thread, void** result);

int __wrap_pthread_join(pthread_t thread, void** value)
{
    uint64_t start = sceKernelGetProcessTimeWide();
    int result = __real_pthread_join(thread, value);
    sceClibPrintf("SWITCH join us=%u result=%d\n", (unsigned)(sceKernelGetProcessTimeWide() - start), result);
    return result;
}

void __real_bridge_gl_tex_image_2d(GLenum target, GLint level, GLint internal, GLsizei width,
    GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);

void __wrap_bridge_gl_tex_image_2d(GLenum target, GLint level, GLint internal, GLsizei width,
    GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels)
{
    uint64_t start = sceKernelGetProcessTimeWide();
    __real_bridge_gl_tex_image_2d(target, level, internal, width, height, border, format, type, pixels);
    __upload_us += sceKernelGetProcessTimeWide() - start;
    ++__uploads;
}
