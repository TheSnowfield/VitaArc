#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <psp2/audiodec.h>
#include <psp2/audioout.h>
#include <psp2/sysmodule.h>
#include <psp2/kernel/threadmgr.h>
#include "audio.h"

int mock_audio_library_inits;
int mock_audio_library_terms;
int mock_audio_module_loads;
int mock_audio_module_unloads;
int mock_audio_decoders;
int mock_audio_ports;
int mock_audio_threads;
int mock_audio_decode_failure;
int mock_audio_module_failure;
int mock_audio_library_failure;
int mock_audio_start_failure;
static int __module_loaded;
static int __library_loaded;

int sceSysmoduleIsLoaded(int module)
{
    assert(module == SCE_SYSMODULE_AUDIOCODEC);
    return __module_loaded ? SCE_SYSMODULE_LOADED : -1;
}

int sceSysmoduleLoadModule(int module)
{
    assert(module == SCE_SYSMODULE_AUDIOCODEC);
    if(mock_audio_module_failure) {
        return -1;
    }

    ++mock_audio_module_loads;
    __module_loaded = 1;
    return 0;
}

int sceSysmoduleUnloadModule(int module)
{
    assert(module == SCE_SYSMODULE_AUDIOCODEC && !__library_loaded);
    ++mock_audio_module_unloads;
    __module_loaded = 0;
    return 0;
}

int sceAudiodecInitLibrary(uint32_t type, SceAudiodecInitParam* parameter)
{
    assert(type == SCE_AUDIODEC_TYPE_MP3 && __module_loaded && !__library_loaded);
    assert(parameter->mp3.totalStreams == SCE_AUDIODEC_MP3_MAX_STREAMS);
    if(mock_audio_library_failure) {
        return -1;
    }

    __library_loaded = 1;
    ++mock_audio_library_inits;
    return 0;
}

int sceAudiodecTermLibrary(uint32_t type)
{
    assert(type == SCE_AUDIODEC_TYPE_MP3 && __library_loaded && !mock_audio_decoders);
    ++mock_audio_library_terms;
    __library_loaded = 0;
    return 0;
}

int sceAudiodecCreateDecoder(SceAudiodecCtrl* control, uint32_t type)
{
    assert(type == SCE_AUDIODEC_TYPE_MP3 && __library_loaded);
    assert((uintptr_t)control->pEs % 256 == 0 && (uintptr_t)control->pPcm % 256 == 0);
    ++mock_audio_decoders;
    control->handle = 1;
    return 0;
}

int sceAudiodecDeleteDecoder(SceAudiodecCtrl* control)
{
    assert(control->handle == 1 && mock_audio_decoders > 0);
    control->handle = 0;
    --mock_audio_decoders;
    return 0;
}

int sceAudiodecClearContext(SceAudiodecCtrl* control)
{
    assert(control->handle == 1);
    return 0;
}

int sceAudiodecDecode(SceAudiodecCtrl* control)
{
    assert(__library_loaded && control->handle == 1 && control->inputEsSize >= 8);
    if(mock_audio_decode_failure) {
        --mock_audio_decode_failure;
        return -1;
    }

    uint32_t index;
    memcpy(&index, control->pEs + 4, sizeof(index));
    uint32_t frames = control->pInfo->mp3.version == SCE_AUDIODEC_MP3_MPEG_VERSION_1 ? 1152 : 576;
    uint32_t channels = control->pInfo->mp3.ch;
    assert(frames * channels * sizeof(int16_t) <= control->maxPcmSize);
    int16_t* samples = control->pPcm;
    for(uint32_t frame = 0; frame < frames; ++frame) {
        int16_t value = (int16_t)(100 + (index * frames + frame) % 10000);
        samples[frame * channels] = value;
        if(channels == 2) {
            samples[frame * channels + 1] = -value;
        }
    }

    control->outputPcmSize = frames * channels * sizeof(int16_t);
    return 0;
}

int sceAudioOutOpenPort(int type, int grain, int rate, int mode)
{
    assert(type == SCE_AUDIO_OUT_PORT_TYPE_MAIN && grain == 1024 && rate == 48000 && mode == SCE_AUDIO_OUT_MODE_STEREO);
    ++mock_audio_ports;
    return 5;
}

int sceAudioOutOutput(int port, const void* output)
{
    assert(port == 5 && output);
    return 0;
}

int sceAudioOutReleasePort(int port)
{
    assert(port == 5 && mock_audio_ports > 0);
    --mock_audio_ports;
    return 0;
}

SceUID sceKernelCreateThread(const char* name, SceKernelThreadEntry entry, int priority, SceSize stack,
    unsigned int attributes, int affinity, const void* options)
{
    assert(name && entry && priority > 0 && stack >= 0x10000);
    (void)attributes;
    (void)affinity;
    (void)options;
    ++mock_audio_threads;
    return 6;
}

int sceKernelStartThread(SceUID thread, SceSize size, void* arguments)
{
    assert(thread == 6 && size == sizeof(void*) && arguments);
    /* Tests drive the production mixer explicitly, without a hardware clock. */
    return mock_audio_start_failure ? -1 : 0;
}

int sceKernelWaitThreadEnd(SceUID thread, int* status, void* timeout)
{
    assert(thread == 6);
    (void)status;
    (void)timeout;
    return 0;
}

int sceKernelDeleteThread(SceUID thread)
{
    assert(thread == 6 && mock_audio_threads > 0);
    --mock_audio_threads;
    return 0;
}

int sceKernelDelayThread(unsigned int microseconds)
{
    (void)microseconds;
    return 0;
}
