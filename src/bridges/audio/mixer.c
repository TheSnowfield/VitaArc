#include <limits.h>
#include "state.h"

static int16_t clamp_sample(int32_t sample)
{
    if(sample > INT16_MAX) {
        return INT16_MAX;
    }

    if(sample < INT16_MIN) {
        return INT16_MIN;
    }

    return (int16_t)sample;
}

static void mix_sfx(audio_t* state, int32_t* mix, uint32_t frames)
{
    for(uint32_t voice_index = 0; voice_index < VITA_AUDIO_MAX_VOICES; ++voice_index) {
        audio_voice_t* voice = &state->voices[voice_index];
        if(!voice->active || !voice->asset || voice->asset->pcm.frame_count == 0) {
            continue;
        }

        float volume = voice->volume < 0 ? state->sfx_volume : voice->volume;
        int32_t gain = (int32_t)(audio_gain(volume) * 32768.0f);

        for(uint32_t frame = 0; frame < frames; ++frame) {
            if(voice->position >= voice->asset->pcm.frame_count) {
                if(voice->loops) {
                    if(voice->loops > 0)
                        --voice->loops;
                    voice->position = 0;
                } else {
                    voice->active = false;
                    break;
                }
            }

            uint32_t source = voice->position * VITA_AUDIO_CHANNELS;
            uint32_t output = frame * VITA_AUDIO_CHANNELS;
            mix[output] += (voice->asset->pcm.samples[source] * gain) >> 15;
            mix[output + 1] += (voice->asset->pcm.samples[source + 1] * gain) >> 15;
            ++voice->position;
        }
    }
}

static void mix_bgm(audio_t* state, int32_t* mix, uint32_t frames)
{
    for(uint32_t channel_index = 0; channel_index < VITA_AUDIO_BGM_CHANNELS; ++channel_index) {
        audio_track_t* channel = &state->bgm[channel_index];
        if(!channel->feed || !channel->playing || channel->paused) {
            continue;
        }

        uint32_t delay = channel->delay > frames ? frames : (uint32_t)channel->delay;
        channel->delay -= delay;
        if(delay == frames) {
            continue;
        }

        int16_t pcm[VITA_AUDIO_GRAIN * VITA_AUDIO_CHANNELS];
        bool ended = false;
        uint32_t count = audio_feed_read(channel->feed, pcm, frames - delay, &ended);
        int32_t gain = (int32_t)(audio_gain(state->bgm_volume * channel->volume) * 32768.0f);

        for(uint32_t frame = 0; frame < count; ++frame) {
            uint32_t source = frame * VITA_AUDIO_CHANNELS;
            uint32_t output = (frame + delay) * VITA_AUDIO_CHANNELS;
            mix[output] += (pcm[source] * gain) >> 15;
            mix[output + 1] += (pcm[source + 1] * gain) >> 15;
        }

        if(ended) {
            channel->playing = false;
        }
    }
}

bool vita_audio_render(audio_t* state, int16_t* output, uint32_t frames)
{
    if(!state || !output || !frames || frames > VITA_AUDIO_GRAIN) {
        return false;
    }

    int32_t mix[VITA_AUDIO_GRAIN * VITA_AUDIO_CHANNELS] = {0};

    pthread_mutex_lock(&state->mutex);
    if(!state->suspended) {
        mix_bgm(state, mix, frames);
        mix_sfx(state, mix, frames);
    }

    pthread_mutex_unlock(&state->mutex);
    for(uint32_t index = 0; index < frames * VITA_AUDIO_CHANNELS; ++index) {
        output[index] = clamp_sample(mix[index]);
    }

    return true;
}
