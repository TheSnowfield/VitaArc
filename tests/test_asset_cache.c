#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include <android/asset_cache.h>
#include <android/assetmgr.h>
#include <logcat/logcat.h>
#include "paths.h"

/* This suite exercises the unchanged standalone-probe default. */
#define ASSET_ROOT PATH_TO_APP "package/assets"

static unsigned __dirs;
static unsigned __opens;
static unsigned __errors;
static int __dir_error;
static int __read_error;
static int __file_error;

DIR* __real_opendir(const char* path);
struct dirent* __real_readdir(DIR* stream);
FILE* __real_fopen(const char* path, const char* mode);

DIR* __wrap_opendir(const char* path)
{
    ++__dirs;
    if(__dir_error) {
        errno = __dir_error;
        return NULL;
    }

    return __real_opendir(path);
}

struct dirent* __wrap_readdir(DIR* stream)
{
    if(__read_error) {
        errno = __read_error;
        return NULL;
    }

    return __real_readdir(stream);
}

FILE* __wrap_fopen(const char* path, const char* mode)
{
    ++__opens;
    if(__file_error) {
        errno = __file_error;
        return NULL;
    }

    return __real_fopen(path, mode);
}

void log_v(const char* tag, const char* format, ...)
{
    (void)tag;
    (void)format;
}

void log_e(const char* tag, const char* format, ...)
{
    (void)tag;
    (void)format;
    ++__errors;
}

static void* __reader(void* unused)
{
    (void)unused;
    for(unsigned index = 0; index < 1000; ++index) {
        assert(!a_asset_cached(ASSET_ROOT "/char/absent.png"));
    }

    return NULL;
}

static void __fixture(const char* path)
{
    test_dirs(path);
    FILE* file = fopen(path, "wb");
    assert(file && fwrite("pixel", 1, 5, file) == 5 && fclose(file) == 0);
}

int main(void)
{
    __fixture(ASSET_ROOT "/char/0_icon.png");
    __fixture(ASSET_ROOT "/fault/ok.bin");
    a_asset_manager_t* manager = a_asset_manager_from_java(NULL, NULL);
    __opens = 0;

    /* Thousands of optional variants need one directory query, no file opens. */
    for(unsigned index = 0; index < 2000; ++index) {
        assert(!a_asset_manager_open(manager, "char/1080/0_icon.png", AASSET_MODE_RANDOM));
    }

    assert(__dirs == 1 && __opens == 0 && __errors == 0 && errno == ENOENT);
    a_asset_t* asset = a_asset_manager_open(manager, "char/0_icon.png", AASSET_MODE_RANDOM);
    assert(asset && a_asset_get_length(asset) == 5);
    char pixels[5];
    assert(a_asset_read(asset, pixels, sizeof(pixels)) == 5 && !memcmp(pixels, "pixel", 5));
    a_asset_close(asset);
    assert(__dirs == 2 && __opens == 1);
    assert(a_asset_cached(ASSET_ROOT "/char/0_ICON.PNG"));

    pthread_t readers[4];
    for(unsigned index = 0; index < 4; ++index) {
        assert(pthread_create(&readers[index], NULL, __reader, NULL) == 0);
    }

    for(unsigned index = 0; index < 4; ++index) {
        assert(pthread_join(readers[index], NULL) == 0);
    }

    assert(__dirs == 2 && __opens == 1);
    __file_error = EACCES;
    assert(!a_asset_manager_open(manager, "char/0_icon.png", AASSET_MODE_RANDOM));
    assert(errno == EACCES && __errors == 1);
    __file_error = 0;

    /* Incomplete or inaccessible snapshots must fall back to the file stream. */
    __read_error = EIO;
    for(unsigned index = 0; index < 2; ++index) {
        asset = a_asset_manager_open(manager, "fault/ok.bin", AASSET_MODE_RANDOM);
        assert(asset);
        a_asset_close(asset);
    }

    assert(__dirs == 4);
    __read_error = 0;
    __dir_error = EACCES;
    asset = a_asset_manager_open(manager, "fault/ok.bin", AASSET_MODE_RANDOM);
    assert(asset);
    a_asset_close(asset);
    __dir_error = 0;

    a_asset_cache_clear();
    __dirs = 0;
    char path[128];
    for(unsigned index = 0; index < 17; ++index) {
        snprintf(path, sizeof(path), ASSET_ROOT "/cache%u/item", index);
        __fixture(path);
        assert(a_asset_cached(path));
    }

    assert(__dirs == 17);
    assert(a_asset_cached(ASSET_ROOT "/cache0/item") && __dirs == 18);
    a_asset_cache_clear();
    return 0;
}
