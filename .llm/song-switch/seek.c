#include <psp2/audiodec.h>
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <stdint.h>
#include <string.h>

#include <bridges/audio/mp3.h>
#include <bridges/audio/feed.h>
#include <bridges/audio/backend.h>

#define WINDOW 4096

static unsigned __calls;
static atomic_int __codecs;
static atomic_int __workers;
static int16_t __reference[WINDOW * 2];

int __real_sceAudiodecDecode(SceAudiodecCtrl* control);

int __wrap_sceAudiodecDecode(SceAudiodecCtrl* control)
{
    ++__calls;
    int result = __real_sceAudiodecDecode(control);
    if(__calls <= 4) {
        sceClibPrintf("SEEK codec input=%u output=%u result=%x\n", control->inputEsSize,
            control->outputPcmSize, result);
    }

    return result;
}

int __real_sceAudiodecCreateDecoder(SceAudiodecCtrl* control, unsigned codec);
int __real_sceAudiodecDeleteDecoder(SceAudiodecCtrl* control);

int __wrap_sceAudiodecCreateDecoder(SceAudiodecCtrl* control, unsigned codec)
{
    int result = __real_sceAudiodecCreateDecoder(control, codec);
    if(result >= 0) {
        atomic_fetch_add(&__codecs, 1);
    }

    return result;
}

int __wrap_sceAudiodecDeleteDecoder(SceAudiodecCtrl* control)
{
    int result = __real_sceAudiodecDeleteDecoder(control);
    if(result >= 0) {
        atomic_fetch_sub(&__codecs, 1);
    }

    return result;
}

int __real_pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*entry)(void*), void* arg);
int __real_pthread_join(pthread_t thread, void** value);

int __wrap_pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*entry)(void*), void* arg)
{
    int result = __real_pthread_create(thread, attr, entry, arg);
    if(result == 0) {
        atomic_fetch_add(&__workers, 1);
    }

    return result;
}

int __wrap_pthread_join(pthread_t thread, void** value)
{
    int result = __real_pthread_join(thread, value);
    if(result == 0) {
        atomic_fetch_sub(&__workers, 1);
    }

    return result;
}

static bool __compare(mp3_stream_t* stream, uint64_t target, uint32_t skip)
{
    unsigned before = __calls;
    uint64_t start = sceKernelGetProcessTimeWide();
    bool ready = vita_mp3_seek_frames(stream, target);
    unsigned elapsed = (unsigned)(sceKernelGetProcessTimeWide() - start);
    unsigned calls = __calls - before;
    if(!ready || vita_mp3_position_frames(stream) != target) {
        sceClibPrintf("SEEK failed target=%llu position=%llu\n", target, vita_mp3_position_frames(stream));
        return false;
    }

    uint32_t mismatches = 0;
    unsigned peak = 0;
    uint64_t square = 0;
    for(uint32_t frame = skip; frame < WINDOW; ++frame) {
        int16_t left, right;
        if(!vita_mp3_next_sample(stream, false, &left, &right)) {
            return false;
        }

        if(left != __reference[frame * 2] || right != __reference[frame * 2 + 1]) {
            if(!mismatches) {
                sceClibPrintf("SEEK mismatch frame=%u got=%d,%d expected=%d,%d\n", frame,
                    left, right, __reference[frame * 2], __reference[frame * 2 + 1]);
            }

            ++mismatches;
        }

        int diff[2] = {left - __reference[frame * 2], right - __reference[frame * 2 + 1]};
        for(unsigned ch = 0; ch < 2; ++ch) {
            unsigned delta = diff[ch] < 0 ? (unsigned)-diff[ch] : (unsigned)diff[ch];
            if(delta > peak) {
                peak = delta;
            }

            square += delta * delta;
        }
    }

    sceClibPrintf("SEEK target=%llu us=%u decodes=%u compared=%u mismatches=%u peak=%u square=%llu\n",
        target, elapsed, calls, WINDOW - skip, mismatches, peak, square);
    /* The fixed-point decoder retains rounding residue across synthesis calls. */
    return peak <= 1;
}

static bool __song(const char* path, uint64_t target)
{
    sceClibPrintf("SEEK song=%s\n", path);
    mp3_stream_t* reference = vita_mp3_open(path);
    mp3_stream_t* stream = vita_mp3_open(path);
    bool result = false;
    if(!reference || !stream) {
        goto done;
    }

    int16_t left, right;
    if(!vita_mp3_next_sample(stream, false, &left, &right)) {
        goto done;
    }

    for(uint64_t frame = 0; frame < target + WINDOW; ++frame) {
        int16_t left, right;
        if(!vita_mp3_next_sample(reference, false, &left, &right)) {
            goto done;
        }

        if(frame >= target) {
            uint32_t slot = (uint32_t)(frame - target) * 2;
            __reference[slot] = left;
            __reference[slot + 1] = right;
        }
    }

    /* Forward, nearby backward, reset with a retained index, and exact repeat. */
    if(!__compare(stream, target, 0) || !__compare(stream, target + 17, 17)) {
        goto done;
    }

    if(!vita_mp3_reset(stream) || !__compare(stream, target + 1, 1) || !__compare(stream, target, 0)) {
        goto done;
    }

    result = true;
done:
    vita_mp3_close(reference);
    vita_mp3_close(stream);
    return result;
}

static bool __feed(void)
{
    uint64_t target = 2135712;
    bool passed = false;
    audio_feed_t* feed = audio_feed_open("app0:package/assets/songs/sayonarahatsukoi/base.mp3", true);
    if(!feed) {
        return false;
    }

    if(!audio_feed_seek(feed, target, false) || !audio_feed_start(feed, false) ||
        audio_feed_position(feed) != target) {
        goto done;
    }

    if(!audio_feed_start(feed, true) || audio_feed_position(feed) != target) {
        goto done;
    }

    int16_t pcm[WINDOW * 2];
    bool ended;
    if(audio_feed_read(feed, pcm, WINDOW, &ended) != WINDOW || ended ||
        audio_feed_position(feed) != target + WINDOW) {
        goto done;
    }

    for(unsigned sample = 0; sample < WINDOW * 2; ++sample) {
        int diff = pcm[sample] - __reference[sample];
        if(diff < -1 || diff > 1) {
            goto done;
        }
    }

    if(!audio_feed_start(feed, false) || audio_feed_position(feed) != target + WINDOW) {
        goto done;
    }

    passed = audio_feed_seek(feed, 0, false) && audio_feed_position(feed) == 0;
done:
    sceClibPrintf("SEEK feed seek/start/loop/consume/rewind=%d\n", passed);
    audio_feed_close(feed);
    return passed;
}

static bool __reuse(void)
{
    const char* path = "app0:package/assets/songs/sayonarahatsukoi/base.mp3";
    audio_feed_t* feed = audio_feed_open(path, true);
    if(!feed) {
        return false;
    }

    bool passed = false;
    audio_feed_t* retained = audio_feed_retain(feed);
    bool guarded = !audio_feed_park(feed) && !audio_feed_reopen(feed, path, true);
    audio_feed_close(retained);
    if(!guarded || !audio_feed_park(feed)) {
        goto done;
    }

    if(!audio_feed_reopen(feed, "app0:package/assets/songs/fairytale/base.mp3", true) ||
        audio_feed_position(feed) != 0 || !audio_feed_seek(feed, 1737120, false)) {
        goto done;
    }

    if(audio_feed_reopen(feed, "app0:missing.mp3", true)) {
        goto done;
    }

    int16_t pcm[WINDOW * 2];
    bool ended = false;
    if(audio_feed_read(feed, pcm, WINDOW, &ended) != 0 || !ended) {
        goto done;
    }

    if(!audio_feed_reopen(feed, path, true) || !audio_feed_seek(feed, 2135712, false) ||
        !audio_feed_start(feed, false) || audio_feed_read(feed, pcm, WINDOW, &ended) != WINDOW || ended) {
        goto done;
    }

    for(unsigned sample = 0; sample < WINDOW * 2; ++sample) {
        int diff = pcm[sample] - __reference[sample];
        if(diff < -1 || diff > 1) {
            goto done;
        }
    }

    passed = true;
done:
    audio_feed_close(feed);
    sceClibPrintf("SEEK reuse ownership/switch/failure/recovery/PCM=%d\n", passed);
    return passed;
}

static bool __backend(void)
{
    static const char* const paths[] = {
        "app0:package/assets/songs/fairytale/base.mp3",
        "app0:package/assets/songs/infinityheaven/base.mp3",
        "app0:package/assets/songs/sayonarahatsukoi/base.mp3",
        "app0:package/assets/audio/in_full.ogg",
    };

    audio_t* state = vita_audio_create(0, 0);
    if(!state) {
        return false;
    }

    bool passed = false;
    for(unsigned index = 0; index < 12; ++index) {
        if(!vita_audio_load_bgm(state, paths[index % 4], 0) || !vita_audio_seek_bgm(state, 500, 0)) {
            goto done;
        }

        vita_audio_play_bgm(state, false, 0, INT_MAX);
        if(vita_audio_get_bgm_position(state, 0) != 500) {
            goto done;
        }

        vita_audio_unload_bgm(state, 0);
        if(vita_audio_get_bgm_position(state, 0) != -1) {
            goto done;
        }
    }

    if(!vita_audio_load_bgm(state, paths[0], 0) || !vita_audio_seek_bgm(state, 1000, 0) ||
        !vita_audio_load_bgm(state, paths[1], 1)) {
        goto done;
    }

    vita_audio_unload_bgm(state, 1);
    if(vita_audio_load_bgm(state, "app0:missing.mp3", 0) || vita_audio_get_bgm_position(state, 0) != 1000) {
        goto done;
    }

    vita_audio_unload_bgm(state, 0);
    passed = true;
done:
    vita_audio_destroy(state);
    sceClibPrintf("SEEK backend reuse/MP3/Vorbis/failed-replace/idle-teardown=%d\n", passed);
    return passed;
}

int main(void)
{
    bool passed = vita_mp3_library_init(2);
    if(passed) {
        passed = __song("app0:package/assets/songs/journey/base.mp3", 1440017) &&
            __song("app0:package/assets/songs/fairytale/base.mp3", 1737120) &&
            __song("app0:package/assets/songs/infinityheaven/base.mp3", 2376000) &&
            __song("app0:package/assets/songs/sayonarahatsukoi/base.mp3", 2135712) && __feed() &&
            __reuse() && __backend();
        vita_mp3_library_term();
    }

    sceClibPrintf("SEEK remaining codecs=%d workers=%d\n", atomic_load(&__codecs), atomic_load(&__workers));
    passed = passed && atomic_load(&__codecs) == 0 && atomic_load(&__workers) == 0;
    sceClibPrintf("SEEK exit=%d\n", passed ? 0 : 1);
    sceKernelExitProcess(passed ? 0 : 1);
    return passed ? 0 : 1;
}
