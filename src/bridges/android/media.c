#include <stdlib.h>
#include <logcat/logcat.h>

#include "media.h"

enum {
    MEDIA_UNSUPPORTED = -10002,
    MEDIA_INVALID_OBJECT = -10003,
    MEDIA_INVALID_PARAMETER = -10004,
};

const char* a_media_key_duration = "durationUs";
const char* a_media_key_height = "height";
const char* a_media_key_mime = "mime";
const char* a_media_key_width = "width";

/* The Android video backend is optional. No Android codec or Java Surface
 * exists on Vita; callers receive NDK errors rather than an inert decoder. */
void* a_media_codec_new(const char* mime)
{
    log_w(TAG, "Android video codec unavailable: %s", mime ? mime : "(null)");
    return NULL;
}

int a_media_codec_delete(void* codec)
{
    (void)codec;
    return MEDIA_INVALID_OBJECT;
}

int a_media_codec_configure(void* codec, const void* format, void* window, void* crypto, uint32_t flags)
{
    (void)codec;
    (void)format;
    (void)window;
    (void)crypto;
    (void)flags;
    return MEDIA_INVALID_OBJECT;
}

int a_media_codec_start(void* codec)
{
    (void)codec;
    return MEDIA_INVALID_OBJECT;
}

int a_media_codec_stop(void* codec)
{
    (void)codec;
    return MEDIA_INVALID_OBJECT;
}

int a_media_codec_flush(void* codec)
{
    (void)codec;
    return MEDIA_INVALID_OBJECT;
}

ssize_t a_media_codec_input(void* codec, int64_t timeout)
{
    (void)codec;
    (void)timeout;
    return MEDIA_INVALID_OBJECT;
}

ssize_t a_media_codec_output(void* codec, void* info, int64_t timeout)
{
    (void)codec;
    (void)info;
    (void)timeout;
    return MEDIA_INVALID_OBJECT;
}

uint8_t* a_media_codec_buffer(void* codec, size_t index, size_t* size)
{
    (void)codec;
    (void)index;
    if(size) {
        *size = 0;
    }

    return NULL;
}

void* a_media_codec_format(void* codec)
{
    (void)codec;
    return NULL;
}

int a_media_codec_queue(void* codec, size_t index, off_t offset, size_t size, uint64_t time, uint32_t flags)
{
    (void)codec;
    (void)index;
    (void)offset;
    (void)size;
    (void)time;
    (void)flags;
    return MEDIA_INVALID_OBJECT;
}

int a_media_codec_release(void* codec, size_t index, bool render)
{
    (void)codec;
    (void)index;
    (void)render;
    return MEDIA_INVALID_OBJECT;
}

void* a_media_extractor_new(void)
{
    return calloc(1, 1);
}

int a_media_extractor_delete(void* extractor)
{
    free(extractor);
    return 0;
}

int a_media_extractor_source(void* extractor, int fd, int64_t offset, int64_t size)
{
    if(!extractor) {
        return MEDIA_INVALID_OBJECT;
    }

    if(fd < 0 || offset < 0 || size < 0) {
        return MEDIA_INVALID_PARAMETER;
    }

    log_w(TAG, "Android video extraction is unavailable");
    return MEDIA_UNSUPPORTED;
}

size_t a_media_extractor_count(void* extractor)
{
    (void)extractor;
    return 0;
}

void* a_media_extractor_format(void* extractor, size_t track)
{
    (void)extractor;
    (void)track;
    return NULL;
}

int a_media_extractor_select(void* extractor, size_t track)
{
    (void)track;
    return extractor ? MEDIA_INVALID_PARAMETER : MEDIA_INVALID_OBJECT;
}

ssize_t a_media_extractor_read(void* extractor, uint8_t* buffer, size_t size)
{
    (void)extractor;
    (void)buffer;
    (void)size;
    return -1;
}

int64_t a_media_extractor_time(void* extractor)
{
    (void)extractor;
    return -1;
}

bool a_media_extractor_advance(void* extractor)
{
    (void)extractor;
    return false;
}

int a_media_extractor_seek(void* extractor, int64_t time, int mode)
{
    (void)time;
    (void)mode;
    return extractor ? MEDIA_UNSUPPORTED : MEDIA_INVALID_OBJECT;
}

int a_media_format_delete(void* format)
{
    (void)format;
    return MEDIA_INVALID_OBJECT;
}

bool a_media_format_int32(void* format, const char* key, int32_t* value)
{
    (void)format;
    (void)key;
    (void)value;
    return false;
}

bool a_media_format_int64(void* format, const char* key, int64_t* value)
{
    (void)format;
    (void)key;
    (void)value;
    return false;
}

bool a_media_format_string(void* format, const char* key, const char** value)
{
    (void)format;
    (void)key;
    (void)value;
    return false;
}

void* a_native_window_from_surface(void* env, void* surface)
{
    (void)env;
    (void)surface;
    return NULL;
}
