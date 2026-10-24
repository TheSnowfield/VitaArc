#ifndef VITAARC_AUDIO_FEED_H
#define VITAARC_AUDIO_FEED_H

#include <stdbool.h>
#include <stdint.h>

typedef struct audio_feed_t audio_feed_t;

audio_feed_t* audio_feed_open(const char* path, bool has_mp3);
audio_feed_t* audio_feed_retain(audio_feed_t* feed);
void audio_feed_close(audio_feed_t* feed);

/**
 * Park or reopen a detached feed that has exactly one owner.
 * A stream-open failure leaves an empty feed for closing or reopening again.
 */
bool audio_feed_park(audio_feed_t* feed);
bool audio_feed_reopen(audio_feed_t* feed, const char* path, bool has_mp3);

/* Control calls may wait for the decoder, without holding the mixer lock. */
bool audio_feed_seek(audio_feed_t* feed, uint64_t position, bool loop);

/**
 * Start from the consumed position and apply the requested loop mode.
 * Reuse prepared PCM when the mode already matches.
 */
bool audio_feed_start(audio_feed_t* feed, bool loop);
uint64_t audio_feed_position(audio_feed_t* feed);

/* Copies available PCM without waiting for disk or decoding. Empty is not EOF. */
uint32_t audio_feed_read(audio_feed_t* feed, int16_t* pcm, uint32_t frames, bool* ended);

#endif
