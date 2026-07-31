#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vorbis/vorbisfile.h>

#include "mp3.h"
#include "stream.h"

#define OUTPUT_RATE 48000u

struct audio_stream_t {
    mp3_stream_t* mp3;
    OggVorbis_File vorbis;
    bool vorbis_open;
    bool failed;
    uint32_t rate;
    uint32_t channels;
    uint32_t phase;
    uint32_t count;
    uint32_t index;
    uint64_t total_source_frames;
    uint64_t duration;
    uint64_t position;
    int16_t pcm[2048];
};

audio_stream_t* vita_audio_stream_open(const char* path, bool has_mp3)
{
    if(!path) {
        return NULL;
    }

    FILE* file = fopen(path, "rb");
    if(!file) {
        return NULL;
    }

    unsigned char magic[4];
    bool ogg = fread(magic, 1, sizeof(magic), file) == sizeof(magic) && memcmp(magic, "OggS", sizeof(magic)) == 0;
    fclose(file);
    if(!ogg && !has_mp3) {
        return NULL;
    }

    audio_stream_t* stream = calloc(1, sizeof(*stream));
    if(!stream) {
        return NULL;
    }

    if(!ogg) {
        stream->mp3 = vita_mp3_open(path);
        if(!stream->mp3) {
            free(stream);
            return NULL;
        }

        return stream;
    }

    if(ov_fopen(path, &stream->vorbis) != 0) {
        free(stream);
        return NULL;
    }

    stream->vorbis_open = true;
    vorbis_info* info = ov_info(&stream->vorbis, 0);
    ogg_int64_t total = ov_pcm_total(&stream->vorbis, -1);
    if(!info || info->rate <= 0 || info->rate > 192000 || info->channels < 1 || info->channels > 2 ||
        !ov_seekable(&stream->vorbis) || total < 0 || (uint64_t)total > UINT64_MAX / OUTPUT_RATE) {
        vita_audio_stream_close(stream);
        return NULL;
    }

    stream->rate = (uint32_t)info->rate;
    stream->channels = (uint32_t)info->channels;
    // Chained streams may change format; this mixer requires one fixed format.
    for(long link = 1; link < ov_streams(&stream->vorbis); ++link) {
        info = ov_info(&stream->vorbis, (int)link);
        if(!info || info->rate != (long)stream->rate || info->channels != (int)stream->channels) {
            vita_audio_stream_close(stream);
            return NULL;
        }
    }

    stream->total_source_frames = (uint64_t)total;
    uint64_t scaled = (uint64_t)total * OUTPUT_RATE;
    stream->duration = scaled / stream->rate + (scaled % stream->rate != 0);
    return stream;
}

void vita_audio_stream_close(audio_stream_t* stream)
{
    if(!stream) {
        return;
    }

    vita_mp3_close(stream->mp3);
    if(stream->vorbis_open) {
        ov_clear(&stream->vorbis);
    }

    free(stream);
}

static bool __seek_vorbis(audio_stream_t* stream, uint64_t position)
{
    if(position > stream->duration) {
        position = stream->duration;
    }

    uint64_t source = position == stream->duration
        ? stream->total_source_frames
        : position / OUTPUT_RATE * stream->rate + position % OUTPUT_RATE * stream->rate / OUTPUT_RATE;
    if(ov_pcm_seek(&stream->vorbis, (ogg_int64_t)source) != 0) {
        return false;
    }

    stream->phase = (uint32_t)(position % OUTPUT_RATE * stream->rate % OUTPUT_RATE);
    stream->position = position;
    stream->index = stream->count = 0;
    stream->failed = false;
    return true;
}

bool vita_audio_stream_seek_frames(audio_stream_t* stream, uint64_t position)
{
    if(!stream) {
        return false;
    }

    if(stream->mp3) {
        return vita_mp3_seek_frames(stream->mp3, position);
    }

    uint64_t previous = stream->position;
    if(__seek_vorbis(stream, position)) {
        return true;
    }

    if(!__seek_vorbis(stream, previous)) {
        stream->failed = true;
    }

    return false;
}

bool vita_audio_stream_reset(audio_stream_t* stream)
{
    return vita_audio_stream_seek_frames(stream, 0);
}

uint64_t vita_audio_stream_position_frames(const audio_stream_t* stream)
{
    return stream ? (stream->mp3 ? vita_mp3_position_frames(stream->mp3) : stream->position) : 0;
}

static bool __ensure_vorbis(audio_stream_t* stream, bool loop)
{
    if(stream->failed) {
        return false;
    }

    unsigned holes = 0, resets = 0;
    while(stream->index >= stream->count) {
        if(stream->failed) {
            return false;
        }

        int link;
        long bytes = ov_read(&stream->vorbis, (char*)stream->pcm, sizeof(stream->pcm), 0, 2, 1, &link);
        if(bytes == OV_HOLE && ++holes <= 16) {
            continue;
        }

        if(bytes < 0 || bytes % (stream->channels * sizeof(int16_t))) {
            stream->failed = true;
            return false;
        }

        if(bytes > 0) {
            stream->count = (uint32_t)bytes / (stream->channels * sizeof(int16_t));
            stream->index = 0;
            return true;
        }

        if(!loop || resets++ || !__seek_vorbis(stream, 0)) {
            return false;
        }
    }

    return true;
}

bool vita_audio_stream_next_sample(audio_stream_t* stream, bool loop, int16_t* left, int16_t* right)
{
    if(!stream || !left || !right) {
        return false;
    }

    if(stream->mp3) {
        return vita_mp3_next_sample(stream->mp3, loop, left, right);
    }

    if(!__ensure_vorbis(stream, loop)) {
        return false;
    }

    uint32_t index = stream->index * stream->channels;
    *left = stream->pcm[index];
    *right = stream->channels == 1 ? *left : stream->pcm[index + 1];
    ++stream->position;
    stream->phase += stream->rate;
    while(stream->phase >= OUTPUT_RATE) {
        stream->phase -= OUTPUT_RATE;
        ++stream->index;
        if(!__ensure_vorbis(stream, loop)) {
            break;
        }
    }

    return true;
}
