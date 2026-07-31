#ifndef __BRIDGES_AUDIO_MP3_H
#define __BRIDGES_AUDIO_MP3_H

#include <stdbool.h>
#include <stdint.h>

typedef struct mp3_stream_t mp3_stream_t;

bool vita_mp3_library_init(uint32_t stream_count);
void vita_mp3_library_term(void);

mp3_stream_t* vita_mp3_open(const char* path);
void vita_mp3_close(mp3_stream_t* stream);
bool vita_mp3_reset(mp3_stream_t* stream);
bool vita_mp3_seek_frames(mp3_stream_t* stream, uint64_t position);
uint64_t vita_mp3_position_frames(const mp3_stream_t* stream);
bool vita_mp3_next_sample(mp3_stream_t* stream, bool loop, int16_t* left, int16_t* right);

#endif /* __BRIDGES_AUDIO_MP3_H */
