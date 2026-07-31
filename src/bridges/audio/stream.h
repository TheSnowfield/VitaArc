#ifndef __BRIDGES_AUDIO_STREAM_H
#define __BRIDGES_AUDIO_STREAM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct audio_stream_t audio_stream_t;

/* PCM positions are output frames at 48 kHz. Format is detected from content. */
audio_stream_t* vita_audio_stream_open(const char* path, bool has_mp3);
void vita_audio_stream_close(audio_stream_t* stream);
bool vita_audio_stream_reset(audio_stream_t* stream);
bool vita_audio_stream_seek_frames(audio_stream_t* stream, uint64_t position);
uint64_t vita_audio_stream_position_frames(const audio_stream_t* stream);
bool vita_audio_stream_next_sample(audio_stream_t* stream, bool loop, int16_t* left, int16_t* right);

#endif /* __BRIDGES_AUDIO_STREAM_H */
