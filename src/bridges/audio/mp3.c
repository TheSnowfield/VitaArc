#include <malloc.h>
#include <psp2/audiodec.h>
#include <psp2/sysmodule.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../logcat/logcat.h"

#include "mp3.h"

#define VITA_AUDIO_OUTPUT_RATE 48000

typedef struct mp3_frame_t {
    uint32_t frame_size;
    uint32_t sample_rate;
    uint32_t version;
    uint32_t channels;
} mp3_frame_t;

struct mp3_stream_t {
    FILE* file;
    long first_frame_offset;
    uint8_t* encoded;
    int16_t* decoded;
    uint32_t decoded_frames;
    uint32_t decoded_position;
    uint32_t resample_phase;
    uint32_t sample_rate;
    uint32_t channels;
    SceAudiodecCtrl control;
    SceAudiodecInfo info;
    bool decoder_created;
    bool library_reference;
    bool failed;
    uint64_t output_position;
};

static pthread_mutex_t __library_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t __library_references;
static uint32_t __stream_references;
static bool __library_initialized;
static bool __module_owned;

static void __release_library(void)
{
    if(__library_initialized && !__library_references && !__stream_references) {
        sceAudiodecTermLibrary(SCE_AUDIODEC_TYPE_MP3);
        __library_initialized = false;
        if(__module_owned) {
            sceSysmoduleUnloadModule(SCE_SYSMODULE_AUDIOCODEC);
            __module_owned = false;
        }
    }
}

static uint32_t read_be32(const uint8_t* data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | (uint32_t)data[3];
}

static bool parse_mp3_header(const uint8_t data[4], mp3_frame_t* frame)
{
    static const uint16_t mpeg1_bitrates[16] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};

    static const uint16_t mpeg2_bitrates[16] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0};

    static const uint32_t sample_rates[3] = {44100, 48000, 32000};

    uint32_t header = read_be32(data);
    if((header & 0xFFE00000) != 0xFFE00000) {
        return false;
    }

    uint32_t version = (header >> 19) & 3;
    uint32_t layer = (header >> 17) & 3;
    uint32_t bitrate_index = (header >> 12) & 15;
    uint32_t sample_rate_index = (header >> 10) & 3;
    uint32_t padding = (header >> 9) & 1;
    if(version == SCE_AUDIODEC_MP3_MPEG_VERSION_RESERVED) {
        return false;
    }

    if(layer != 1) {
        return false;
    }

    if(bitrate_index == 0 || bitrate_index == 15) {
        return false;
    }

    if(sample_rate_index == 3) {
        return false;
    }

    uint32_t sample_rate = sample_rates[sample_rate_index];
    const uint16_t* bitrate_table = mpeg1_bitrates;
    switch(version) {
        case SCE_AUDIODEC_MP3_MPEG_VERSION_2:
            sample_rate /= 2;
            bitrate_table = mpeg2_bitrates;
            break;

        case SCE_AUDIODEC_MP3_MPEG_VERSION_2_5:
            sample_rate /= 4;
            bitrate_table = mpeg2_bitrates;
            break;
    }

    uint32_t bitrate = (uint32_t)bitrate_table[bitrate_index] * 1000;
    uint32_t coefficient = version == SCE_AUDIODEC_MP3_MPEG_VERSION_1 ? 144 : 72;
    uint32_t frame_size = coefficient * bitrate / sample_rate + padding;
    if(frame_size < 4 || frame_size > SCE_AUDIODEC_MP3_MAX_ES_SIZE) {
        return false;
    }

    frame->frame_size = frame_size;
    frame->sample_rate = sample_rate;
    frame->version = version;
    frame->channels = ((header >> 6) & 3) == 3 ? 1 : 2;
    return true;
}

static long skip_id3(FILE* file)
{
    uint8_t header[10];
    if(fread(header, 1, sizeof(header), file) != sizeof(header)) {
        return 0;
    }

    if(memcmp(header, "ID3", 3) != 0) {
        fseek(file, 0, SEEK_SET);
        return 0;
    }

    uint32_t size = ((uint32_t)(header[6] & 0x7F) << 21) | ((uint32_t)(header[7] & 0x7F) << 14) |
        ((uint32_t)(header[8] & 0x7F) << 7) | (uint32_t)(header[9] & 0x7F);
    long offset = 10 + (long)size;
    if(header[5] & 0x10) {
        offset += 10;
    }

    fseek(file, offset, SEEK_SET);
    return offset;
}

static bool find_mp3_frame(FILE* file, mp3_frame_t* frame, long* frame_offset)
{
    uint8_t header[4];
    while(fread(header, 1, sizeof(header), file) == sizeof(header)) {
        long offset = ftell(file) - (long)sizeof(header);
        if(parse_mp3_header(header, frame)) {
            *frame_offset = offset;
            fseek(file, offset, SEEK_SET);
            return true;
        }

        fseek(file, offset + 1, SEEK_SET);
    }

    return false;
}

bool vita_mp3_library_init(uint32_t stream_count)
{
    if(stream_count == 0 || stream_count > SCE_AUDIODEC_MP3_MAX_STREAMS) {
        return false;
    }

    pthread_mutex_lock(&__library_mutex);
    if(__library_initialized) {
        ++__library_references;
        pthread_mutex_unlock(&__library_mutex);
        return true;
    }

    __module_owned = sceSysmoduleIsLoaded(SCE_SYSMODULE_AUDIOCODEC) != SCE_SYSMODULE_LOADED;
    if(__module_owned) {
        int result = sceSysmoduleLoadModule(SCE_SYSMODULE_AUDIOCODEC);
        if(result < 0) {
            log_e(TAG, "Audio codec module load failed: 0x%08x", (unsigned int)result);
            __module_owned = false;
            pthread_mutex_unlock(&__library_mutex);
            return false;
        }
    }

    SceAudiodecInitParam parameter;
    memset(&parameter, 0, sizeof(parameter));
    parameter.mp3.size = sizeof(parameter.mp3);
    parameter.mp3.totalStreams = SCE_AUDIODEC_MP3_MAX_STREAMS;
    int result = sceAudiodecInitLibrary(SCE_AUDIODEC_TYPE_MP3, &parameter);
    if(result < 0) {
        log_e(TAG, "MP3 library initialization failed: 0x%08x", (unsigned int)result);
        if(__module_owned) {
            sceSysmoduleUnloadModule(SCE_SYSMODULE_AUDIOCODEC);
            __module_owned = false;
        }

        pthread_mutex_unlock(&__library_mutex);
        return false;
    }

    __library_initialized = true;
    __library_references = 1;
    pthread_mutex_unlock(&__library_mutex);
    return true;
}

void vita_mp3_library_term(void)
{
    pthread_mutex_lock(&__library_mutex);
    if(__library_references) {
        --__library_references;
    }

    __release_library();
    pthread_mutex_unlock(&__library_mutex);
}

mp3_stream_t* vita_mp3_open(const char* path)
{
    if(!path) {
        return NULL;
    }

    mp3_stream_t* stream = calloc(1, sizeof(*stream));
    if(!stream) {
        return NULL;
    }

    pthread_mutex_lock(&__library_mutex);
    if(!__library_initialized || !__library_references || __stream_references == SCE_AUDIODEC_MP3_MAX_STREAMS) {
        pthread_mutex_unlock(&__library_mutex);
        free(stream);
        return NULL;
    }

    ++__stream_references;
    stream->library_reference = true;
    pthread_mutex_unlock(&__library_mutex);

    stream->file = fopen(path, "rb");
    if(!stream->file) {
        vita_mp3_close(stream);
        return NULL;
    }

    unsigned char magic[4];
    if(fread(magic, 1, sizeof(magic), stream->file) == sizeof(magic) &&
        (memcmp(magic, "OggS", sizeof(magic)) == 0 || memcmp(magic, "RIFF", sizeof(magic)) == 0)) {
        vita_mp3_close(stream);
        return NULL;
    }

    rewind(stream->file);

    skip_id3(stream->file);
    mp3_frame_t first_frame;
    if(!find_mp3_frame(stream->file, &first_frame, &stream->first_frame_offset)) {
        vita_mp3_close(stream);
        return NULL;
    }

    stream->encoded = memalign(SCE_AUDIODEC_ALIGNMENT_SIZE, SCE_AUDIODEC_MP3_MAX_ES_SIZE);
    stream->decoded = memalign(SCE_AUDIODEC_ALIGNMENT_SIZE, SCE_AUDIODEC_MP3_MAX_SAMPLES * 2 * sizeof(int16_t));
    if(!stream->encoded || !stream->decoded) {
        vita_mp3_close(stream);
        return NULL;
    }

    memset(&stream->control, 0, sizeof(stream->control));
    memset(&stream->info, 0, sizeof(stream->info));
    stream->info.mp3.size = sizeof(stream->info.mp3);
    stream->info.mp3.ch = first_frame.channels;
    stream->info.mp3.version = first_frame.version;
    stream->control.size = sizeof(stream->control);
    stream->control.pEs = stream->encoded;
    stream->control.maxEsSize = SCE_AUDIODEC_MP3_MAX_ES_SIZE;
    stream->control.pPcm = stream->decoded;
    stream->control.maxPcmSize = SCE_AUDIODEC_MP3_MAX_SAMPLES * 2 * sizeof(int16_t);
    stream->control.wordLength = SCE_AUDIODEC_WORD_LENGTH_16BITS;
    stream->control.pInfo = &stream->info;
    stream->sample_rate = first_frame.sample_rate;
    stream->channels = first_frame.channels;

    int result = sceAudiodecCreateDecoder(&stream->control, SCE_AUDIODEC_TYPE_MP3);
    if(result < 0) {
        vita_mp3_close(stream);
        return NULL;
    }

    stream->decoder_created = true;
    fseek(stream->file, stream->first_frame_offset, SEEK_SET);
    return stream;
}

void vita_mp3_close(mp3_stream_t* stream)
{
    if(!stream) {
        return;
    }

    if(stream->decoder_created) {
        sceAudiodecDeleteDecoder(&stream->control);
    }

    if(stream->file) {
        fclose(stream->file);
    }

    free(stream->encoded);
    free(stream->decoded);
    if(stream->library_reference) {
        pthread_mutex_lock(&__library_mutex);
        --__stream_references;
        __release_library();
        pthread_mutex_unlock(&__library_mutex);
    }

    free(stream);
}

bool vita_mp3_reset(mp3_stream_t* stream)
{
    if(!stream || !stream->file || !stream->decoder_created) {
        return false;
    }

    if(sceAudiodecClearContext(&stream->control) < 0 ||
        fseek(stream->file, stream->first_frame_offset, SEEK_SET) != 0) {
        return false;
    }

    stream->decoded_frames = 0;
    stream->decoded_position = 0;
    stream->resample_phase = 0;
    stream->output_position = 0;
    stream->failed = false;
    return true;
}

static bool decode_next_frame(mp3_stream_t* stream)
{
    for(uint32_t attempt = 0; attempt < 16; ++attempt) {
        mp3_frame_t frame;
        long offset = 0;
        if(!find_mp3_frame(stream->file, &frame, &offset)) {
            stream->failed = ferror(stream->file) != 0;
            return false;
        }

        if(fread(stream->encoded, 1, frame.frame_size, stream->file) != frame.frame_size) {
            stream->failed = true;
            return false;
        }

        stream->control.pEs = stream->encoded;
        stream->control.inputEsSize = frame.frame_size;
        stream->control.pPcm = stream->decoded;
        stream->control.outputPcmSize = 0;
        int result = sceAudiodecDecode(&stream->control);
        if(result < 0 || stream->control.outputPcmSize > stream->control.maxPcmSize ||
            stream->control.outputPcmSize % (frame.channels * sizeof(int16_t))) {
            stream->failed = true;
            return false;
        }

        if(result >= 0 && stream->control.outputPcmSize >= frame.channels * sizeof(int16_t)) {
            stream->sample_rate = frame.sample_rate;
            stream->channels = frame.channels;
            stream->decoded_frames = stream->control.outputPcmSize / (frame.channels * sizeof(int16_t));
            stream->decoded_position = 0;
            return true;
        }

        /* A successful decoder call can buffer a frame without producing PCM. */
    }

    stream->failed = true;
    return false;
}

static bool ensure_decoded_sample(mp3_stream_t* stream, bool loop)
{
    while(stream->decoded_position >= stream->decoded_frames) {
        if(decode_next_frame(stream)) {
            return true;
        }

        if(stream->failed || !loop || !vita_mp3_reset(stream) || !decode_next_frame(stream)) {
            return false;
        }
    }

    return true;
}

bool vita_mp3_next_sample(mp3_stream_t* stream, bool loop, int16_t* left, int16_t* right)
{
    if(!stream || !left || !right || !ensure_decoded_sample(stream, loop)) {
        return false;
    }

    uint32_t position = stream->decoded_position * stream->channels;
    *left = stream->decoded[position];
    *right = stream->channels == 1 ? *left : stream->decoded[position + 1];
    ++stream->output_position;
    stream->resample_phase += stream->sample_rate;
    while(stream->resample_phase >= VITA_AUDIO_OUTPUT_RATE) {
        stream->resample_phase -= VITA_AUDIO_OUTPUT_RATE;
        ++stream->decoded_position;
        if(!ensure_decoded_sample(stream, loop)) {
            break;
        }
    }

    return true;
}

uint64_t vita_mp3_position_frames(const mp3_stream_t* stream)
{
    return stream ? stream->output_position : 0;
}

static bool __skip_to(mp3_stream_t* stream, uint64_t position)
{
    while(stream->output_position < position) {
        if(!ensure_decoded_sample(stream, false)) {
            return !stream->failed;
        }

        uint64_t source_frames = stream->decoded_frames - stream->decoded_position;
        uint64_t remaining_phase = source_frames * VITA_AUDIO_OUTPUT_RATE - stream->resample_phase;
        uint64_t output_frames = (remaining_phase + stream->sample_rate - 1) / stream->sample_rate;
        uint64_t step = position - stream->output_position;
        if(step > output_frames) {
            step = output_frames;
        }

        uint64_t phase = stream->resample_phase + step * stream->sample_rate;
        stream->decoded_position += (uint32_t)(phase / VITA_AUDIO_OUTPUT_RATE);
        stream->resample_phase = (uint32_t)(phase % VITA_AUDIO_OUTPUT_RATE);
        stream->output_position += step;
    }

    return true;
}

bool vita_mp3_seek_frames(mp3_stream_t* stream, uint64_t position)
{
    if(!stream || !stream->decoder_created) {
        return false;
    }

    uint64_t previous = stream->output_position;
    if(position < previous || stream->failed) {
        if(!vita_mp3_reset(stream)) {
            return false;
        }
    }

    if(__skip_to(stream, position)) {
        return true;
    }

    /* Restore decoder/resampler state when a seek encounters a decoding error. */
    if(vita_mp3_reset(stream)) {
        __skip_to(stream, previous);
    }

    return false;
}
