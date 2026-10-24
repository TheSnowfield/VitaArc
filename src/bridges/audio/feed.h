#ifndef VITAARC_AUDIO_FEED_H
#define VITAARC_AUDIO_FEED_H

#include <stdbool.h>
#include <stdint.h>

typedef struct audio_feed_t audio_feed_t;

audio_feed_t* audio_feed_open(const char* path, bool has_mp3);
audio_feed_t* audio_feed_retain(audio_feed_t* feed);
void audio_feed_close(audio_feed_t* feed);

/* Control calls may wait for the decoder, without holding the mixer lock. */
bool audio_feed_seek(audio_feed_t* feed, uint64_t position, bool loop);
uint64_t audio_feed_position(audio_feed_t* feed);

/* Copies available PCM without waiting for disk or decoding. Empty is not EOF. */
uint32_t audio_feed_read(audio_feed_t* feed, int16_t* pcm, uint32_t frames, bool* ended);

#endif
