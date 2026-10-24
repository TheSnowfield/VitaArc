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

static void __schedule(audio_t* state, uint32_t frames)
{
    audio_track_t* main = &state->bgm[0];
    if(!main->feed || !main->playing || main->paused || main->delay >= frames) {
        return;
    }

    uint32_t delay = main->delay;
    uint64_t position = audio_feed_position(main->feed);
    uint64_t end = position + frames - delay;
    while(state->event_next < state->event_count) {
        audio_event_t* event = &state->events[state->event_next];
        if(event->played) {
            ++state->event_next;
            continue;
        }

        if(event->frame >= end) {
            break;
        }

        audio_voice_t* voice = audio_sfx_start(state, event->asset, 0, true, -1.0f);
        voice->delay = delay;
        if(event->frame > position) {
            voice->delay += event->frame - position;
        }

        event->played = true;
        ++state->event_next;
    }
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

        uint32_t delay = voice->delay > frames ? frames : voice->delay;
        voice->delay -= delay;
        for(uint32_t frame = delay; frame < frames; ++frame) {
            uint32_t end = voice->loops && voice->end ? voice->end : voice->asset->pcm.frame_count;
            if(voice->position >= end) {
                if(voice->loops) {
                    if(voice->loops > 0) {
                        --voice->loops;
                    }

                    voice->position = voice->begin;
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
    for(int channel_index = 0; channel_index < state->bgm_count; ++channel_index) {
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
        float master = channel->keep_master ? channel->master : state->bgm_volume;
        int32_t gain = (int32_t)(audio_gain(master * channel->volume) * 32768.0f);

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
        __schedule(state, frames);
        mix_bgm(state, mix, frames);
        mix_sfx(state, mix, frames);
    }

    pthread_mutex_unlock(&state->mutex);
    for(uint32_t index = 0; index < frames * VITA_AUDIO_CHANNELS; ++index) {
        output[index] = clamp_sample(mix[index]);
    }

    return true;
}
