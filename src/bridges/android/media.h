#ifndef VITAARC_ANDROID_MEDIA_H
#define VITAARC_ANDROID_MEDIA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

extern const char* a_media_key_duration;
extern const char* a_media_key_height;
extern const char* a_media_key_mime;
extern const char* a_media_key_width;

void* a_media_codec_new(const char* mime);
int a_media_codec_delete(void* codec);
int a_media_codec_configure(void* codec, const void* format, void* window, void* crypto, uint32_t flags);
int a_media_codec_start(void* codec);
int a_media_codec_stop(void* codec);
int a_media_codec_flush(void* codec);
ssize_t a_media_codec_input(void* codec, int64_t timeout);
ssize_t a_media_codec_output(void* codec, void* info, int64_t timeout);
uint8_t* a_media_codec_buffer(void* codec, size_t index, size_t* size);
void* a_media_codec_format(void* codec);
int a_media_codec_queue(void* codec, size_t index, off_t offset, size_t size, uint64_t time, uint32_t flags);
int a_media_codec_release(void* codec, size_t index, bool render);

void* a_media_extractor_new(void);
int a_media_extractor_delete(void* extractor);
int a_media_extractor_source(void* extractor, int fd, int64_t offset, int64_t size);
size_t a_media_extractor_count(void* extractor);
void* a_media_extractor_format(void* extractor, size_t track);
int a_media_extractor_select(void* extractor, size_t track);
ssize_t a_media_extractor_read(void* extractor, uint8_t* buffer, size_t size);
int64_t a_media_extractor_time(void* extractor);
bool a_media_extractor_advance(void* extractor);
int a_media_extractor_seek(void* extractor, int64_t time, int mode);

int a_media_format_delete(void* format);
bool a_media_format_int32(void* format, const char* key, int32_t* value);
bool a_media_format_int64(void* format, const char* key, int64_t* value);
bool a_media_format_string(void* format, const char* key, const char** value);
void* a_native_window_from_surface(void* env, void* surface);

#endif
