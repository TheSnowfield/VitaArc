#ifndef __BRIDGES_AUDIO_WAV_H
#define __BRIDGES_AUDIO_WAV_H

#include <stdbool.h>
#include <stdint.h>

typedef struct audio_pcm_t {
    int16_t* samples;
    uint32_t frame_count;
} audio_pcm_t;

bool vita_wav_load(const char* path, uint32_t output_rate, audio_pcm_t* buffer);
void vita_wav_free(audio_pcm_t* buffer);

#endif /* __BRIDGES_AUDIO_WAV_H */
