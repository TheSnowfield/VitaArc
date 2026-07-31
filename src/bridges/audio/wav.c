#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "wav.h"

typedef struct wav_fmt_t {
    uint16_t format;
    uint16_t channels;
    uint32_t sample_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} wav_fmt_t;

static uint16_t read_u16(const uint8_t* data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t read_u32(const uint8_t* data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static int32_t read_pcm_sample(const uint8_t* data, uint16_t bits_per_sample)
{
    if(bits_per_sample == 8) {
        return ((int32_t)data[0] - 128) << 8;
    }

    if(bits_per_sample == 16) {
        return (int16_t)read_u16(data);
    }

    if(bits_per_sample == 24) {
        int32_t value = (int32_t)data[0] | ((int32_t)data[1] << 8) | ((int32_t)data[2] << 16);
        if(value & 0x00800000) {
            value |= (int32_t)0xFF000000;
        }

        return value >> 8;
    }

    if(bits_per_sample == 32) {
        return (int32_t)read_u32(data) >> 16;
    }

    return 0;
}

static int16_t sample_channel(const uint8_t* pcm, uint32_t frame, const wav_fmt_t* format, uint16_t channel)
{
    uint32_t bytes_per_sample = format->bits_per_sample / 8;
    uint16_t source_channel = channel;
    if(format->channels == 1) {
        source_channel = 0;
    }

    const uint8_t* sample = pcm + frame * format->block_align + source_channel * bytes_per_sample;
    if(format->format == 3) {
        uint32_t bits = read_u32(sample);
        float value;
        memcpy(&value, &bits, sizeof(value));
        if(isnan(value))
            return 0;

        if(value >= 1.0f)
            return INT16_MAX;

        if(value <= -1.0f)
            return INT16_MIN;

        return (int16_t)(value * 32768.0f);
    }

    return (int16_t)read_pcm_sample(sample, format->bits_per_sample);
}

bool vita_wav_load(const char* path, uint32_t output_rate, audio_pcm_t* buffer)
{
    if(!path || !buffer || output_rate == 0) {
        return false;
    }

    memset(buffer, 0, sizeof(*buffer));

    FILE* file = fopen(path, "rb");
    if(!file) {
        return false;
    }

    uint8_t riff_header[12];
    if(fread(riff_header, 1, sizeof(riff_header), file) != sizeof(riff_header) || memcmp(riff_header, "RIFF", 4) != 0 ||
        memcmp(riff_header + 8, "WAVE", 4) != 0) {
        fclose(file);
        return false;
    }

    wav_fmt_t format;
    memset(&format, 0, sizeof(format));
    uint8_t* pcm = NULL;
    uint32_t pcm_size = 0;

    for(;;) {
        uint8_t chunk_header[8];
        if(fread(chunk_header, 1, sizeof(chunk_header), file) != sizeof(chunk_header)) {
            break;
        }

        uint32_t chunk_size = read_u32(chunk_header + 4);
        if(memcmp(chunk_header, "fmt ", 4) == 0) {
            uint8_t format_data[40];
            if(chunk_size < 16 || chunk_size > sizeof(format_data) ||
                fread(format_data, 1, chunk_size, file) != chunk_size) {
                break;
            }

            format.format = read_u16(format_data);
            format.channels = read_u16(format_data + 2);
            format.sample_rate = read_u32(format_data + 4);
            format.block_align = read_u16(format_data + 12);
            format.bits_per_sample = read_u16(format_data + 14);
        } else if(memcmp(chunk_header, "data", 4) == 0) {
            pcm = malloc(chunk_size);
            if(!pcm || fread(pcm, 1, chunk_size, file) != chunk_size) {
                free(pcm);
                pcm = NULL;
                break;
            }

            pcm_size = chunk_size;
        } else if(fseek(file, chunk_size, SEEK_CUR) != 0) {
            break;
        }

        if(chunk_size & 1) {
            fseek(file, 1, SEEK_CUR);
        }

        if(pcm && format.sample_rate != 0) {
            break;
        }
    }

    fclose(file);

    uint32_t bytes_per_sample = format.bits_per_sample / 8;
    if(!pcm || (format.format != 1 && format.format != 3) || (format.channels != 1 && format.channels != 2) ||
        (format.format == 3 ? format.bits_per_sample != 32
                            : (format.bits_per_sample != 8 && format.bits_per_sample != 16 &&
                                  format.bits_per_sample != 24 && format.bits_per_sample != 32)) ||
        format.block_align != bytes_per_sample * format.channels || format.sample_rate == 0 ||
        pcm_size < format.block_align) {
        free(pcm);
        return false;
    }

    uint32_t source_frames = pcm_size / format.block_align;
    uint64_t scaled_frames = (uint64_t)source_frames * output_rate;
    uint32_t output_frames = (uint32_t)((scaled_frames + format.sample_rate - 1) / format.sample_rate);
    int16_t* output = malloc((size_t)output_frames * 2 * sizeof(int16_t));
    if(!output) {
        free(pcm);
        return false;
    }

    for(uint32_t i = 0; i < output_frames; ++i) {
        uint64_t source_position = (uint64_t)i * format.sample_rate;
        uint32_t source_index = (uint32_t)(source_position / output_rate);
        uint32_t fraction = (uint32_t)(source_position % output_rate);
        if(source_index >= source_frames) {
            source_index = source_frames - 1;
            fraction = 0;
        }

        uint32_t next_index = source_index + 1;
        if(next_index >= source_frames) {
            next_index = source_index;
        }

        for(uint16_t channel = 0; channel < 2; ++channel) {
            int32_t first = sample_channel(pcm, source_index, &format, channel);
            int32_t second = sample_channel(pcm, next_index, &format, channel);
            int32_t value = first + (int32_t)(((int64_t)(second - first) * fraction) / output_rate);
            output[i * 2 + channel] = (int16_t)value;
        }
    }

    free(pcm);
    buffer->samples = output;
    buffer->frame_count = output_frames;
    return true;
}

void vita_wav_free(audio_pcm_t* buffer)
{
    if(!buffer) {
        return;
    }

    free(buffer->samples);
    memset(buffer, 0, sizeof(*buffer));
}
