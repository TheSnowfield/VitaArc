#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vorbis/vorbisfile.h>

#include <audio/backend.h>
#include <audio/mp3.h>
#include <audio/provider.h>
#include <audio/stream.h>
#include <audio/wav.h>
#include "mocks/audio.h"
#include "vorbis_fixture.h"

static int __vorbis_seek_failures;
static int __vorbis_read_failure;
static int __vorbis_holes;
int __real_ov_pcm_seek(OggVorbis_File* file, ogg_int64_t position);
long __real_ov_read(
    OggVorbis_File* file, char* buffer, int length, int big_endian, int word_size, int is_signed, int* link);

int __wrap_ov_pcm_seek(OggVorbis_File* file, ogg_int64_t position)
{
    if(__vorbis_seek_failures > 0) {
        --__vorbis_seek_failures;
        return OV_EREAD;
    }

    return __real_ov_pcm_seek(file, position);
}

long __wrap_ov_read(
    OggVorbis_File* file, char* buffer, int length, int big_endian, int word_size, int is_signed, int* link)
{
    if(__vorbis_read_failure) {
        __vorbis_read_failure = 0;
        return OV_EREAD;
    }

    if(__vorbis_holes > 0) {
        --__vorbis_holes;
        return OV_HOLE;
    }

    return __real_ov_read(file, buffer, length, big_endian, word_size, is_signed, link);
}

static void __fixture(const char* path, uint32_t rate, bool mono)
{
    FILE* file = fopen(path, "wb");
    assert(file != NULL);
    uint32_t version = rate >= 32000 ? 3 : (rate >= 16000 ? 2 : 0);
    uint32_t rate_index = rate == 48000 ? 1 : (rate == 32000 ? 2 : 0);
    const unsigned char id3[10] = {'I', 'D', '3', 4, 0, 0, 0, 0, 0, 0};

    assert(fwrite(id3, 1, sizeof(id3), file) == sizeof(id3));
    for(uint32_t index = 0; index < 20; ++index) {
        uint32_t bitrate_index = version == 3 ? 9 + index % 2 : 8 + index % 2;
        uint32_t bitrate = version == 3 ? (index % 2 ? 160000 : 128000) : (index % 2 ? 80000 : 64000);
        uint32_t header = 0xffe00000 | version << 19 | 1 << 17 | 1 << 16 | bitrate_index << 12 | rate_index << 10 |
            (mono ? 3 << 6 : 0);
        uint32_t size = (version == 3 ? 144 : 72) * bitrate / rate;
        unsigned char frame[1441] = {0};

        assert(size <= sizeof(frame));
        frame[0] = header >> 24;
        frame[1] = header >> 16;
        frame[2] = header >> 8;
        frame[3] = header;
        memcpy(frame + 4, &index, sizeof(index));
        assert(fwrite(frame, 1, size, file) == size);
    }

    assert(fclose(file) == 0);
}

static void __test_seek(const char* path, uint32_t rate)
{
    mp3_stream_t* sequential = vita_mp3_open(path);
    assert(sequential != NULL);
    uint64_t input_frames = 20 * (rate >= 32000 ? 1152 : 576);
    size_t expected = (size_t)((input_frames * 48000 + rate - 1) / rate);
    int16_t* reference = calloc(expected * 2, sizeof(*reference));
    assert(reference != NULL);
    size_t count = 0;
    int16_t left, right;
    while(vita_mp3_next_sample(sequential, false, &left, &right)) {
        assert(count < expected);
        reference[count * 2] = left;
        reference[count * 2 + 1] = right;
        ++count;
    }

    assert(count == expected && vita_mp3_position_frames(sequential) == count);
    vita_mp3_close(sequential);
    mp3_stream_t* stream = vita_mp3_open(path);
    assert(stream != NULL);
    size_t positions[] = {1000, 1001, 4000, 0, 1, expected - 5, expected + 100, 17};

    for(size_t index = 0; index < sizeof(positions) / sizeof(positions[0]); ++index) {
        size_t position = positions[index] < expected ? positions[index] : expected;
        assert(vita_mp3_seek_frames(stream, positions[index]));
        assert(vita_mp3_position_frames(stream) == position);
        for(size_t offset = 0; offset < 128 && position + offset < expected; ++offset) {
            assert(vita_mp3_next_sample(stream, false, &left, &right));
            assert(left == reference[(position + offset) * 2]);
            assert(right == reference[(position + offset) * 2 + 1]);
        }
    }

    assert(vita_mp3_seek_frames(stream, 100));
    mock_audio_decode_failure = 1;
    assert(!vita_mp3_seek_frames(stream, 10000));
    assert(vita_mp3_position_frames(stream) == 100);
    assert(vita_mp3_next_sample(stream, false, &left, &right));
    assert(left == reference[200] && right == reference[201]);
    assert(vita_mp3_reset(stream));
    for(size_t index = 0; index < expected + 48; ++index) {
        assert(vita_mp3_next_sample(stream, true, &left, &right));
        assert(left == reference[(index % expected) * 2]);
    }

    assert(vita_mp3_position_frames(stream) == 48);
    vita_mp3_close(stream);
    free(reference);
}

static void __test_vorbis(const char* path, unsigned rate, unsigned channels, unsigned frames)
{
    make_vorbis_fixture(path, rate, channels, frames);
    // A valid MP3 header elsewhere in a container must never choose the MP3 SDK.
    assert(vita_mp3_open(path) == NULL);
    audio_stream_t* stream = vita_audio_stream_open(path, false);
    assert(stream);
    size_t expected = ((uint64_t)frames * 48000 + rate - 1) / rate;
    int16_t* reference = calloc(expected * 2, sizeof(*reference));
    assert(reference);
    int16_t left, right;
    size_t count = 0, nonzero = 0;
    while(vita_audio_stream_next_sample(stream, false, &left, &right)) {
        assert(count < expected);
        reference[count * 2] = left;
        reference[count * 2 + 1] = right;
        nonzero += left != 0;
        if(channels == 1) {
            assert(left == right);
        }

        ++count;
    }

    assert(count == expected && nonzero > expected / 2);
    assert(vita_audio_stream_position_frames(stream) == expected);
    uint64_t positions[] = {0, 1, 1001, 4000, expected - 3, expected + 100, UINT64_MAX};

    for(size_t index = 0; index < sizeof(positions) / sizeof(positions[0]); ++index) {
        uint64_t position = positions[index] < expected ? positions[index] : expected;
        assert(vita_audio_stream_seek_frames(stream, positions[index]));
        assert(vita_audio_stream_position_frames(stream) == position);
        for(size_t offset = 0; offset < 128 && position + offset < expected; ++offset) {
            assert(vita_audio_stream_next_sample(stream, false, &left, &right));
            assert(abs(left - reference[(position + offset) * 2]) <= 1);
            assert(abs(right - reference[(position + offset) * 2 + 1]) <= 1);
        }
    }

    assert(vita_audio_stream_reset(stream));
    assert(!vita_audio_stream_next_sample(stream, false, NULL, &right));
    assert(vita_audio_stream_position_frames(stream) == 0);
    for(size_t index = 0; index < expected * 2 + 37; ++index) {
        assert(vita_audio_stream_next_sample(stream, true, &left, &right));
        assert(abs(left - reference[(index % expected) * 2]) <= 1);
        assert(abs(right - reference[(index % expected) * 2 + 1]) <= 1);
    }

    assert(vita_audio_stream_position_frames(stream) == 37);
    assert(vita_audio_stream_seek_frames(stream, 100));
    __vorbis_seek_failures = 1;
    assert(!vita_audio_stream_seek_frames(stream, 1000));
    assert(vita_audio_stream_position_frames(stream) == 100);
    assert(vita_audio_stream_next_sample(stream, false, &left, &right));
    assert(abs(left - reference[200]) <= 1 && abs(right - reference[201]) <= 1);
    __vorbis_seek_failures = 2;
    assert(!vita_audio_stream_seek_frames(stream, 500));
    assert(!vita_audio_stream_next_sample(stream, true, &left, &right));
    assert(vita_audio_stream_reset(stream));
    __vorbis_read_failure = 1;
    assert(!vita_audio_stream_next_sample(stream, true, &left, &right));
    assert(vita_audio_stream_reset(stream));
    __vorbis_holes = 17;
    assert(!vita_audio_stream_next_sample(stream, true, &left, &right));
    assert(__vorbis_holes == 0);
    assert(vita_audio_stream_reset(stream));
    __vorbis_holes = 1;
    assert(vita_audio_stream_next_sample(stream, false, &left, &right));
    assert(abs(left - reference[0]) <= 1 && abs(right - reference[1]) <= 1);
    vita_audio_stream_close(stream);
    free(reference);
}

static void __append_file(FILE* output, const char* path)
{
    FILE* input = fopen(path, "rb");
    assert(input);
    char bytes[4096];
    size_t count;
    while((count = fread(bytes, 1, sizeof(bytes), input)) != 0) {
        assert(fwrite(bytes, 1, count, output) == count);
    }

    assert(!ferror(input) && fclose(input) == 0);
}

static void __test_container_selection(void)
{
    FILE* file = fopen("invalid.ogg", "wb");
    assert(file && fwrite("OggS", 1, 4, file) == 4);
    __append_file(file, "fixture48.mp3");
    assert(fclose(file) == 0);
    assert(vita_audio_stream_open("invalid.ogg", true) == NULL);
    assert(vita_mp3_open("invalid.ogg") == NULL);
    assert(vita_audio_stream_open("fixture48.mp3", false) == NULL);
    file = fopen("changed-format.ogg", "wb");
    assert(file);
    __append_file(file, "vorbis48.ogg");
    __append_file(file, "vorbis44.mp3");
    assert(fclose(file) == 0);
    assert(vita_audio_stream_open("changed-format.ogg", true) == NULL);
    make_vorbis_fixture("surround.ogg", 48000, 3, 12000);
    assert(vita_audio_stream_open("surround.ogg", true) == NULL);
    assert(mock_audio_decoders == 0);
}

static void __render(audio_t* state, uint32_t frames)
{
    int16_t output[VITA_AUDIO_BLOCK_FRAMES * 2];
    while(frames) {
        uint32_t step = frames > VITA_AUDIO_BLOCK_FRAMES ? VITA_AUDIO_BLOCK_FRAMES : frames;
        assert(vita_audio_render(state, output, step));
        frames -= step;

        /* The hardware output call supplies this clock in the application. */
        struct timespec tick = {.tv_nsec = (long)((uint64_t)step * 1000000000 / VITA_AUDIO_SAMPLE_RATE)};
        nanosleep(&tick, NULL);
    }
}

static void __test_vorbis_backend(void)
{
    mock_audio_module_failure = 1;
    audio_t* state = vita_audio_create(1.0f, 1.0f);
    assert(state && mock_audio_decoders == 0);
    assert(vita_audio_load_bgm(state, "vorbis44.mp3", 0));
    assert(mock_audio_decoders == 0);
    vita_audio_play_bgm(state, true, 0, 0);
    __render(state, 4800);
    assert(vita_audio_get_bgm_position(state, 0) == 100);
    assert(vita_audio_seek_bgm(state, 250, 0));
    __render(state, 480);
    assert(vita_audio_get_bgm_position(state, 0) == 260);
    vita_audio_set_bgm_paused(state, true, 0);
    __render(state, 480);
    assert(vita_audio_get_bgm_position(state, 0) == 260);
    vita_audio_unload_bgm(state, 0);
    assert(vita_audio_get_bgm_position(state, 0) == -1);
    vita_audio_destroy(state);
    mock_audio_module_failure = 0;
}

static void __test_backend(void)
{
    audio_t* state = vita_audio_create(1.0f, 1.0f);
    assert(state != NULL);
    assert(vita_audio_get_bgm_position(state, 0) == -1);
    for(int channel = 0; channel < VITA_AUDIO_BGM_CHANNELS; ++channel) {
        assert(vita_audio_load_bgm(state, "fixture48.mp3", channel));
        assert(vita_audio_get_bgm_position(state, channel) == 0);
    }

    vita_audio_play_bgm(state, false, 0, 0);
    __render(state, 4800);
    assert(vita_audio_get_bgm_position(state, 0) == 100);
    assert(vita_audio_get_bgm_position(state, 1) == 0);
    vita_audio_set_bgm_paused(state, true, 0);
    __render(state, 4800);
    assert(vita_audio_get_bgm_position(state, 0) == 100);
    assert(vita_audio_seek_bgm(state, 150, 0));
    assert(vita_audio_get_bgm_position(state, 0) == 150);
    __render(state, 48);
    assert(vita_audio_get_bgm_position(state, 0) == 150);
    vita_audio_play_bgm(state, false, 0, 0);
    assert(vita_audio_get_bgm_position(state, 0) == 150);
    int16_t output[96];
    assert(vita_audio_render(state, output, 48));
    assert(output[0] == 7300 && output[1] == -7300);
    assert(vita_audio_get_bgm_position(state, 0) == 151);
    vita_audio_suspend(state);
    __render(state, 4800);
    assert(vita_audio_get_bgm_position(state, 0) == 151);
    vita_audio_resume(state);
    assert(!vita_audio_load_bgm(state, "missing.mp3", 0));
    assert(vita_audio_get_bgm_position(state, 0) == 151);
    __render(state, 48);
    assert(vita_audio_get_bgm_position(state, 0) == 152);
    assert(!vita_audio_seek_bgm(state, -1, 0));
    assert(!vita_audio_seek_bgm(state, 1, 3));
    assert(vita_audio_get_bgm_position(state, -1) == -1);
    assert(vita_audio_seek_bgm(state, INT_MAX, 0));
    assert(vita_audio_get_bgm_position(state, 0) == 480);
    assert(vita_audio_seek_bgm(state, 0, 0));
    vita_audio_play_bgm(state, true, 0, 10);
    __render(state, 480);
    assert(vita_audio_get_bgm_position(state, 0) == 0);
    __render(state, 23040);
    assert(vita_audio_get_bgm_position(state, 0) == 0);
    __render(state, 480);
    assert(vita_audio_get_bgm_position(state, 0) == 10);
    vita_audio_play_bgm(state, true, 0, INT_MAX);
    __render(state, 4800);
    assert(vita_audio_get_bgm_position(state, 0) == 10);
    assert(vita_audio_seek_bgm(state, 100, 0));
    __render(state, 48);
    assert(vita_audio_get_bgm_position(state, 0) == 101);
    vita_audio_set_channel_bgm_volume(state, 0.0f, 0);
    assert(vita_audio_render(state, output, 48) && output[0] == 0);
    vita_audio_play_bgm(state, false, 2, 0);
    assert(vita_audio_render(state, output, 48) && output[0] == 100);
    assert(vita_audio_get_bgm_position(state, 2) == 1);
    vita_audio_unload_bgm(state, 2);
    assert(vita_audio_get_bgm_position(state, 2) == -1);
    assert(!vita_audio_render(NULL, output, 48));
    assert(!vita_audio_render(state, output, VITA_AUDIO_BLOCK_FRAMES + 1));
    vita_audio_destroy(state);
}

static void __test_provider(void)
{
    void* provider = audio_provider_construct();
    assert(provider != NULL && audio_provider_init(provider, 1.0f, 1.0f));
    audio_provider_load_bgm(provider, "fixture48.mp3", 0);
    uintptr_t* table = *(uintptr_t**)provider;
    typedef void (*seek_t)(void*, int, int);
    typedef int (*position_t)(void*, int);
    ((seek_t)table[8])(provider, 125, 0);
    assert(((position_t)table[7])(provider, 0) == 125);
    assert(((position_t)table[7])(provider, 1) == -1);
    audio_provider_delete(provider);
    assert(audio_provider_get_bgm_position(NULL, 0) == -1);
}

static void __test_float_wav(void)
{
    unsigned char header[] = {'R',
        'I',
        'F',
        'F',
        76,
        0,
        0,
        0,
        'W',
        'A',
        'V',
        'E',
        'f',
        'm',
        't',
        ' ',
        16,
        0,
        0,
        0,
        3,
        0,
        2,
        0,
        192,
        93,
        0,
        0,
        0,
        238,
        2,
        0,
        8,
        0,
        32,
        0,
        'd',
        'a',
        't',
        'a',
        40,
        0,
        0,
        0};

    const uint32_t bits[] = {
        0, 0x3f000000, 0xbf000000, 0x3f800000, 0xbf800000, 0x40000000, 0x7fc00000, 0x7f800000, 0xff800000, 0x00000001};

    FILE* file = fopen("float.wav", "wb");
    assert(file && fwrite(header, 1, sizeof(header), file) == sizeof(header));
    for(unsigned i = 0; i < sizeof(bits) / sizeof(*bits); ++i) {
        unsigned char sample[4];
        for(unsigned j = 0; j < 4; ++j)
            sample[j] = bits[i] >> (j * 8);
        assert(fwrite(sample, 1, 4, file) == 4);
    }

    assert(fclose(file) == 0);
    audio_pcm_t pcm;
    const int16_t expected[] = {0, 16384, -16384, 32767, -32768, 32767, 0, 32767, -32768, 0};

    assert(vita_wav_load("float.wav", 24000, &pcm));
    assert(pcm.frame_count == 5 && !memcmp(pcm.samples, expected, sizeof(expected)));
    vita_wav_free(&pcm);
    assert(!pcm.samples && !pcm.frame_count);
    assert(vita_wav_load("float.wav", 48000, &pcm) && pcm.frame_count == 10);
    for(unsigned frame = 0; frame < 5; ++frame)
        for(unsigned channel = 0; channel < 2; ++channel) {
            int first = expected[frame * 2 + channel];
            int second = expected[(frame < 4 ? frame + 1 : frame) * 2 + channel];
            assert(pcm.samples[frame * 4 + channel] == first);
            assert(pcm.samples[frame * 4 + 2 + channel] == first + (second - first) / 2);
        }

    vita_wav_free(&pcm);
    /* Integer PCM32 is still interpreted as integer; float64 is unsupported. */
    file = fopen("float.wav", "r+b");
    assert(file && fseek(file, 20, SEEK_SET) == 0 && fputc(1, file) == 1 && fclose(file) == 0);
    assert(vita_wav_load("float.wav", 24000, &pcm));
    assert(pcm.samples[1] == 0x3f00 && pcm.samples[2] == (int16_t)0xbf00);
    vita_wav_free(&pcm);
    file = fopen("float.wav", "r+b");
    assert(file && fseek(file, 20, SEEK_SET) == 0 && fputc(3, file) == 3);
    assert(fseek(file, 34, SEEK_SET) == 0 && fputc(64, file) == 64 && fclose(file) == 0);
    assert(!vita_wav_load("float.wav", 24000, &pcm) && !pcm.samples);
}

static void __test_sfx_versions(void)
{
    const unsigned char header[] = {'R',
        'I',
        'F',
        'F',
        52,
        0,
        0,
        0,
        'W',
        'A',
        'V',
        'E',
        'f',
        'm',
        't',
        ' ',
        16,
        0,
        0,
        0,
        1,
        0,
        2,
        0,
        128,
        187,
        0,
        0,
        0,
        238,
        2,
        0,
        4,
        0,
        16,
        0,
        'd',
        'a',
        't',
        'a',
        16,
        0,
        0,
        0};

    int16_t samples[] = {8000, -8000, 8000, -8000, 8000, -8000, 8000, -8000};

    FILE* file = fopen("sfx.wav", "wb");
    assert(file && fwrite(header, 1, sizeof(header), file) == sizeof(header));
    assert(fwrite(samples, 1, sizeof(samples), file) == sizeof(samples) && fclose(file) == 0);
    audio_t* state = vita_audio_create(.5f, 1.f);
    assert(state);
    int16_t output[24];
    assert(vita_audio_play_sfx_ex(state, "sfx.wav", 1, false, .25f) > 0);
    assert(vita_audio_render(state, output, 12));
    for(unsigned i = 0; i < 24; ++i)
        assert(output[i] == (i < 16 ? (i % 2 ? -2000 : 2000) : 0));
    assert(vita_audio_play_sfx_ex(state, "sfx.wav", -1, false, .25f) > 0);
    assert(vita_audio_play_sfx_ex(state, "sfx.wav", -1, false, .5f) > 0);
    assert(vita_audio_get_sfx_path_volume(state, "sfx.wav") == .5f);
    assert(vita_audio_render(state, output, 12));
    for(unsigned i = 0; i < 24; ++i)
        assert(output[i] == (i % 2 ? -6000 : 6000));
    vita_audio_set_sfx_path_volume(state, "sfx.wav", .125f);
    assert(vita_audio_get_sfx_path_volume(state, "sfx.wav") == .125f);
    assert(vita_audio_render(state, output, 12));
    for(unsigned i = 0; i < 24; ++i)
        assert(output[i] == (i % 2 ? -3000 : 3000));
    vita_audio_stop_sfx_path(state, "sfx.wav");
    assert(vita_audio_render(state, output, 12));
    for(unsigned i = 0; i < 24; ++i)
        assert(output[i] == (i % 2 ? -2000 : 2000));
    vita_audio_unload_sfx(state, "sfx.wav");
    assert(vita_audio_get_sfx_path_volume(state, "sfx.wav") == 0.f);
    assert(vita_audio_render(state, output, 12));
    for(unsigned i = 0; i < 24; ++i)
        assert(output[i] == 0);
    assert(vita_audio_play_sfx(state, "sfx.wav", 0, false) > 0);
    vita_audio_set_sfx_volume(state, .125f);
    assert(vita_audio_render(state, output, 4));
    for(unsigned i = 0; i < 8; ++i)
        assert(output[i] == (i % 2 ? -1000 : 1000));
    vita_audio_destroy(state);
}

static void __test_audio_v7(void)
{
    uint32_t header[] = {
        0x46464952, 36 + 144 * 4, 0x45564157, 0x20746d66, 16,
        0x00020001, 48000, 192000, 0x00100004, 0x61746164, 144 * 4,
    };

    int16_t samples[288];
    for(unsigned frame = 0; frame < 144; ++frame) {
        samples[frame * 2] = samples[frame * 2 + 1] = (frame / 48 + 1) * 1000;
    }

    FILE* file = fopen("range.wav", "wb");
    assert(file && fwrite(header, 1, sizeof(header), file) == sizeof(header));
    assert(fwrite(samples, 1, sizeof(samples), file) == sizeof(samples));
    assert(fclose(file) == 0);

    audio_t* state = vita_audio_create_channels(1, 0, VITA_AUDIO_MAX_BGM);
    assert(state && vita_audio_is_active(state));
    assert(vita_audio_load_bgm(state, "fixture48.mp3", 9));
    vita_audio_play_bgm(state, false, 9, 0);
    __render(state, 48);
    assert(vita_audio_get_bgm_position(state, 9) == 1);
    vita_audio_unload_bgm(state, 9);
    assert(vita_audio_get_sfx_volume(state) == 1);
    int16_t output[512];
    assert(vita_audio_play_sfx_range(state, "range.wav", true, false, 1, 1, 2) > 0);
    assert(vita_audio_render(state, output, 192));
    for(unsigned frame = 0; frame < 192; ++frame) {
        assert(output[frame * 2] == (frame < 48 ? 1000 : 2000));
    }

    vita_audio_stop_sfx_path(state, "range.wav");
    assert(vita_audio_load_bgm(state, "fixture48.mp3", 0));
    assert(vita_audio_schedule_sfx(state, "range.wav", 2));
    assert(vita_audio_schedule_sfx(state, "range.wav", 1));
    vita_audio_play_bgm(state, false, 0, 1);
    assert(vita_audio_render(state, output, 256));
    for(unsigned frame = 0; frame < 256; ++frame) {
        int expected = 0;
        if(frame >= 96 && frame < 240) {
            expected += (((frame - 96) / 48 + 1) * 1000 * 26214) >> 15;
        }

        if(frame >= 144) {
            expected += (((frame - 144) / 48 + 1) * 1000 * 26214) >> 15;
        }

        assert(output[frame * 2] == expected && output[frame * 2 + 1] == expected);
    }

    vita_audio_unload_sfx(state, "range.wav");
    vita_audio_set_master_volume(state, 1, false);
    assert(vita_audio_render(state, output, 48));
    for(unsigned index = 0; index < 96; ++index) {
        assert(output[index] == 0);
    }

    vita_audio_play_bgm(state, false, 0, 0);
    assert(vita_audio_render(state, output, 48));
    bool audible = false;
    for(unsigned index = 0; index < 96; ++index) {
        audible |= output[index] != 0;
    }

    assert(audible);
    vita_audio_set_master_volume(state, 0, true);
    assert(vita_audio_render(state, output, 48));
    for(unsigned index = 0; index < 96; ++index) {
        assert(output[index] == 0);
    }

    vita_audio_suspend(state);
    assert(!vita_audio_is_active(state));
    vita_audio_resume(state);
    assert(vita_audio_is_active(state));
    vita_audio_destroy(state);
}

static void __test_lifetime(void)
{
    int initialized = mock_audio_library_inits;
    int terminated = mock_audio_library_terms;
    assert(vita_mp3_library_init(3) && vita_mp3_library_init(3));
    assert(mock_audio_library_inits == initialized + 1);
    mp3_stream_t* stream = vita_mp3_open("fixture48.mp3");
    assert(stream != NULL);
    vita_mp3_library_term();
    vita_mp3_library_term();
    assert(mock_audio_library_terms == terminated);
    assert(vita_mp3_open("fixture48.mp3") == NULL);
    int16_t left, right;
    assert(vita_mp3_next_sample(stream, false, &left, &right));
    vita_mp3_close(stream);
    assert(mock_audio_library_terms == terminated + 1);
    audio_t* first = vita_audio_create(1.0f, 1.0f);
    audio_t* second = vita_audio_create(1.0f, 1.0f);
    assert(first && second);
    assert(vita_audio_load_bgm(second, "fixture48.mp3", 0));
    vita_audio_destroy(first);
    vita_audio_play_bgm(second, false, 0, 0);
    __render(second, 480);
    assert(vita_audio_get_bgm_position(second, 0) == 10);
    vita_audio_destroy(second);
    mock_audio_start_failure = 1;
    assert(vita_audio_create(1.0f, 1.0f) == NULL);
    mock_audio_start_failure = 0;
    mock_audio_library_failure = 1;
    assert(!vita_mp3_library_init(3));
    mock_audio_library_failure = 0;
    mock_audio_module_failure = 1;
    assert(!vita_mp3_library_init(3));
    mock_audio_module_failure = 0;
}

static void __test_sfx_cache(void)
{
    uint32_t header[] = {
        0x46464952, 40, 0x45564157, 0x20746d66, 16,
        0x00020001, 48000, 192000, 0x00100004, 0x61746164, 4,
    };

    audio_t* state = vita_audio_create(1, 0);
    assert(state);
    char path[32];
    for(unsigned index = 0; index < 48; ++index) {
        snprintf(path, sizeof(path), "cache-%u.wav", index);
        int16_t samples[] = {1000 + index, -1000 - index};
        FILE* file = fopen(path, "wb");
        assert(file && fwrite(header, 1, sizeof(header), file) == sizeof(header));
        assert(fwrite(samples, 1, sizeof(samples), file) == sizeof(samples));
        assert(fclose(file) == 0);
        assert(vita_audio_preload_sfx(state, path));
    }

    vita_audio_unload_sfx(state, "cache-0.wav");
    assert(vita_audio_play_sfx_ex(state, "cache-47.wav", 0, false, 1) > 0);
    int16_t output[2];
    assert(vita_audio_render(state, output, 1));
    assert(output[0] == 1047 && output[1] == -1047);
    assert(vita_audio_preload_sfx(state, "cache-0.wav"));
    vita_audio_destroy(state);
}

int main(void)
{
    __fixture("fixture48.mp3", 48000, false);
    __fixture("fixture44.mp3", 44100, false);
    __fixture("fixture32.mp3", 32000, true);
    __fixture("fixture22.mp3", 22050, false);
    __fixture("fixture11.mp3", 11025, true);
    assert(vita_mp3_library_init(3));
    __test_seek("fixture48.mp3", 48000);
    __test_seek("fixture44.mp3", 44100);
    __test_seek("fixture32.mp3", 32000);
    __test_seek("fixture22.mp3", 22050);
    __test_seek("fixture11.mp3", 11025);
    __test_vorbis("vorbis44.mp3", 44100, 2, 22050);
    __test_vorbis("vorbis48.ogg", 48000, 1, 24000);
    __test_vorbis("vorbis24.ogg", 24000, 2, 12000);
    __test_vorbis("vorbis192.ogg", 192000, 1, 12000);
    __test_container_selection();
    vita_mp3_library_term();
    __test_vorbis_backend();
    __test_backend();
    __test_provider();
    __test_float_wav();
    __test_sfx_versions();
    __test_audio_v7();
    __test_sfx_cache();
    __test_lifetime();
    assert(mock_audio_decoders == 0 && mock_audio_ports == 0 && mock_audio_threads == 0);
    assert(mock_audio_library_inits == mock_audio_library_terms);
    assert(mock_audio_module_loads == mock_audio_module_unloads);
    return 0;
}
