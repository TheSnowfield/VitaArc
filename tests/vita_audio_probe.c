/* Runs the production MP3 stream against actual Vita/Vita3K SDK endpoints.
 * No graphics, JNI or audio decoder mocks are involved. PCM is inspected and
 * the real output worker runs muted; audible output is not established. */
#include <stdint.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include "bridges/audio/mp3.h"
#include "bridges/audio/backend.h"
#include "bridges/audio/stream.h"

#ifdef VITAARC_VORBIS_PROBE
#define PROBE_NAME "Vorbis"
#define PROBE_ASSET "app0:package/assets/audio/in_full.ogg"
typedef audio_stream_t ProbeStream;
#define probe_library_init(count) ((count) > 0)
#define probe_library_term() ((void)0)
#define probe_open(path) vita_audio_stream_open(path, false)
#define probe_close vita_audio_stream_close
#define probe_reset vita_audio_stream_reset
#define probe_seek_frames vita_audio_stream_seek_frames
#define probe_position_frames vita_audio_stream_position_frames
#define probe_next_sample vita_audio_stream_next_sample
#else
#define PROBE_NAME "MP3"
#define PROBE_ASSET "app0:package/assets/audio/calibrate.mp3"
typedef mp3_stream_t ProbeStream;
#define probe_library_init vita_mp3_library_init
#define probe_library_term vita_mp3_library_term
#define probe_open vita_mp3_open
#define probe_close vita_mp3_close
#define probe_reset vita_mp3_reset
#define probe_seek_frames vita_mp3_seek_frames
#define probe_position_frames vita_mp3_position_frames
#define probe_next_sample vita_mp3_next_sample
#endif

static bool check_output_worker(void)
{
    bool passed = false;
    audio_t* state = vita_audio_create(0.0f, 0.0f);
    if(!state) {
        return false;
    }

    if(!vita_audio_load_bgm(state, PROBE_ASSET, 0)) {
        goto done;
    }

    vita_audio_play_bgm(state, false, 0, 0);
    int initial = 0;
    for(int attempt = 0; attempt < 10 && initial <= 0; ++attempt) {
        sceKernelDelayThread(50000);
        initial = vita_audio_get_bgm_position(state, 0);
    }

    vita_audio_set_bgm_paused(state, true, 0);
    int paused = vita_audio_get_bgm_position(state, 0);
    sceKernelDelayThread(50000);
    if(initial <= 0 || vita_audio_get_bgm_position(state, 0) != paused) {
        goto done;
    }

    if(!vita_audio_seek_bgm(state, 500, 0) || vita_audio_get_bgm_position(state, 0) != 500) {
        goto done;
    }

    vita_audio_set_bgm_paused(state, false, 0);
    int resumed = 500;
    for(int attempt = 0; attempt < 10 && resumed <= 500; ++attempt) {
        sceKernelDelayThread(50000);
        resumed = vita_audio_get_bgm_position(state, 0);
    }

    sceClibPrintf(
        "VitaArc " PROBE_NAME " probe: muted queue initial=%d paused=%d resumed=%d ms\n", initial, paused, resumed);
    passed = resumed > 500;
done:
    vita_audio_destroy(state);
    return passed;
}

int main(void)
{
    int result = 1;
    sceClibPrintf("VitaArc " PROBE_NAME " probe: production decoder initialization\n");
    if(!probe_library_init(1)) {
        sceClibPrintf("VitaArc " PROBE_NAME " probe: library initialization failed\n");
        goto done;
    }

    ProbeStream* stream = probe_open(PROBE_ASSET);
    if(!stream) {
        sceClibPrintf("VitaArc " PROBE_NAME " probe: open/decoder creation failed\n");
        goto release_library;
    }

    uint32_t frames = 0, nonzero = 0;
    for(; frames < 48000; ++frames) {
        int16_t left, right;
        if(!probe_next_sample(stream, false, &left, &right)) {
            break;
        }

        nonzero += left != 0 || right != 0;
    }

    sceClibPrintf("VitaArc " PROBE_NAME " probe: frames=%u nonzero=%u position=%llu\n",
        frames,
        nonzero,
        (unsigned long long)probe_position_frames(stream));
    if(frames != 48000 || !nonzero || probe_position_frames(stream) != frames) {
        goto close_stream;
    }

    if(!probe_seek_frames(stream, 24000) || probe_position_frames(stream) != 24000) {
        sceClibPrintf("VitaArc " PROBE_NAME " probe: seek failed\n");
        goto close_stream;
    }

    int16_t left, right;
    if(!probe_next_sample(stream, false, &left, &right) || probe_position_frames(stream) != 24001 ||
        !probe_reset(stream) || probe_position_frames(stream) != 0) {
        sceClibPrintf("VitaArc " PROBE_NAME " probe: seek/read/reset failed\n");
        goto close_stream;
    }

    sceClibPrintf("VitaArc " PROBE_NAME " probe: decode/position/seek/reset passed\n");
    result = 0;
close_stream:
    probe_close(stream);
release_library:
    probe_library_term();
    if(result == 0) {
        result = check_output_worker() ? 0 : 1;
        sceClibPrintf("VitaArc " PROBE_NAME " probe: muted output worker/position/pause/seek/teardown %s\n",
            result == 0 ? "passed" : "failed");
    }

done:
    sceClibPrintf("VitaArc " PROBE_NAME " probe: exit=%d\n", result);
    sceKernelExitProcess(result);
    return result;
}
