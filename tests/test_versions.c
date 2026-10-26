#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <android/asset_cache.h>
#include <android/assetmgr.h>
#include <audio/state.h>
#include <logcat/logcat.h>
#include "paths.h"

static int __read_failure = -1;
static int __allocation_failure;
static int __open_error;
static int __close_error;
static int __stat_error;
DIR* __real_opendir(const char* path);
struct dirent* __real_readdir(DIR* directory);
int __real_closedir(DIR* directory);
int __real_lstat(const char* path, struct stat* info);
void* __real_malloc(size_t size);
void* __real_realloc(void* address, size_t size);

DIR* __wrap_opendir(const char* path)
{
    if(__open_error) {
        errno = __open_error;
        return NULL;
    }
    return __real_opendir(path);
}

int __wrap_closedir(DIR* directory)
{
    int result = __real_closedir(directory);
    if(__close_error) {
        errno = __close_error;
        return -1;
    }
    return result;
}

int __wrap_lstat(const char* path, struct stat* info)
{
    if(__stat_error && strstr(path, "3.0.0c")) {
        errno = __stat_error;
        return -1;
    }
    return __real_lstat(path, info);
}

struct dirent* __wrap_readdir(DIR* directory)
{
    if(__read_failure == 0) {
        errno = EIO;
        return NULL;
    }
    if(__read_failure > 0)
        --__read_failure;
    return __real_readdir(directory);
}

void* __wrap_malloc(size_t size)
{
    if(__allocation_failure == 1) {
        errno = ENOMEM;
        return NULL;
    }
    return __real_malloc(size);
}

void* __wrap_realloc(void* address, size_t size)
{
    if(__allocation_failure == 2) {
        errno = ENOMEM;
        return NULL;
    }
    return __real_realloc(address, size);
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
}

static void __write(const char* path, const char* contents)
{
    test_dirs(path);
    FILE* file = fopen(path, "wb");
    size_t length = strlen(contents);
    assert(file && fwrite(contents, 1, length, file) == length && fclose(file) == 0);
}

static void __fixture(const char* name, const char* payload)
{
    char path[VITAARC_PATH_CAPACITY];
    assert(snprintf(path, sizeof(path), "%s%s/assets/audio/fixture.bin", PATH_TO_VERSIONS, name) < (int)sizeof(path));
    __write(path, payload);
    assert(snprintf(path, sizeof(path), "%s%s/lib/armeabi-v7a/libcocos2dcpp.so", PATH_TO_VERSIONS, name) <
        (int)sizeof(path));
    __write(path, payload);
    assert(snprintf(path, sizeof(path), "%s%s/obb/main.obb", PATH_TO_VERSIONS, name) < (int)sizeof(path));
    __write(path, payload);
}

static void __expect_file(const char* path, const char* expected)
{
    FILE* file = fopen(path, "rb");
    char contents[64] = {0};
    assert(file && fread(contents, 1, sizeof(contents) - 1, file) == strlen(expected));
    assert(!strcmp(contents, expected) && fclose(file) == 0);
}

static void __expect_resources(a_asset_manager_t* manager, const char* expected)
{
    a_asset_t* asset = a_asset_manager_open(manager, "audio/fixture.bin", AASSET_MODE_RANDOM);
    char contents[64] = {0};
    assert(asset && a_asset_read(asset, contents, sizeof(contents) - 1) == (int)strlen(expected));
    assert(!strcmp(contents, expected));
    a_asset_close(asset);
    __expect_file(LIBRARY_LIBCOCOS2DCPP, expected);
    char path[VITAARC_PATH_CAPACITY];
    assert(snprintf(path, sizeof(path), "%smain.obb", PATH_TO_OBB) < (int)sizeof(path));
    __expect_file(path, expected);
    const char* aliases[] = {"assets/audio/fixture.bin", "audio/fixture.bin", "file:///android_asset/audio/fixture.bin"};
    for(unsigned index = 0; index < sizeof(aliases) / sizeof(aliases[0]); ++index) {
        assert(audio_path(aliases[index], path, sizeof(path)));
        assert(strstr(path, vita_version_name()));
        __expect_file(path, expected);
    }
    assert(!audio_path("audio/fixture.bin", path, 2));
}

int main(void)
{
    vita_versions_t versions = {0};
    assert(vita_versions_scan(&versions) && versions.count == 0 && !versions.names);
    assert(!vita_versions_scan(NULL) && errno == EINVAL);
    vita_versions_free(NULL);
    assert(!vita_version_select(NULL) && errno == EINVAL);
    assert(!vita_version_select("missing") && errno == ENOENT);
    assert(!strcmp(PATH_TO_PACKAGE, "app0:package/"));

    __fixture("3.0.0c", "version three");
    __fixture("1.9.0c", "version one");
    __fixture("Current build", "current build");
    __fixture(".unfinished", "hidden");
    __fixture("bad:name", "invalid");
    __fixture("trailing.", "invalid");
    __write(PATH_TO_VERSIONS "missing-library/assets/payload", "invalid");
    __write(PATH_TO_VERSIONS "missing-assets/lib/armeabi-v7a/libcocos2dcpp.so", "invalid");
    __write(PATH_TO_VERSIONS "regular-file", "invalid");
    test_dirs(PATH_TO_VERSIONS "library-directory/assets/");
    test_dirs(PATH_TO_VERSIONS "library-directory/lib/armeabi-v7a/libcocos2dcpp.so/");
    /* Symlinked roots and resource/library directories cannot escape the selected package. */
    assert(symlink("1.9.0c", PATH_TO_VERSIONS "root-link") == 0);
    __fixture("assets-link", "invalid");
    assert(rename(PATH_TO_VERSIONS "assets-link/assets", PATH_TO_VERSIONS "assets-link/moved-assets") == 0);
    assert(symlink("moved-assets", PATH_TO_VERSIONS "assets-link/assets") == 0);
    __fixture("library-link", "invalid");
    assert(rename(PATH_TO_VERSIONS "library-link/lib", PATH_TO_VERSIONS "library-link/moved-lib") == 0);
    assert(symlink("moved-lib", PATH_TO_VERSIONS "library-link/lib") == 0);
    __fixture("arm-link", "invalid");
    assert(rename(PATH_TO_VERSIONS "arm-link/lib/armeabi-v7a", PATH_TO_VERSIONS "arm-link/lib/moved-arm") == 0);
    assert(symlink("moved-arm", PATH_TO_VERSIONS "arm-link/lib/armeabi-v7a") == 0);
    __fixture("file-link", "invalid");
    assert(rename(PATH_TO_VERSIONS "file-link/lib/armeabi-v7a/libcocos2dcpp.so",
               PATH_TO_VERSIONS "file-link/lib/armeabi-v7a/moved.so") == 0);
    assert(symlink("moved.so", PATH_TO_VERSIONS "file-link/lib/armeabi-v7a/libcocos2dcpp.so") == 0);
    __fixture("obb-link", "invalid");
    assert(rename(PATH_TO_VERSIONS "obb-link/obb", PATH_TO_VERSIONS "obb-link/moved-obb") == 0);
    assert(symlink("moved-obb", PATH_TO_VERSIONS "obb-link/obb") == 0);
    assert(vita_versions_scan(&versions));
    assert(versions.count == 3);
    assert(!strcmp(versions.names[0], "1.9.0c") && !strcmp(versions.names[1], "3.0.0c") &&
        !strcmp(versions.names[2], "Current build"));
    vita_versions_free(&versions);
    assert(!versions.names && !versions.count);
    vita_versions_free(&versions);

    __open_error = EACCES;
    assert(!vita_versions_scan(&versions) && errno == EACCES && !versions.names && !versions.count);
    __open_error = 0;
    __close_error = EIO;
    assert(!vita_versions_scan(&versions) && errno == EIO && !versions.names && !versions.count);
    __close_error = 0;
    __stat_error = EACCES;
    assert(!vita_versions_scan(&versions) && errno == EACCES && !versions.names && !versions.count);
    assert(!vita_version_select("3.0.0c") && errno == EACCES);
    __stat_error = 0;

    __read_failure = 4;
    assert(!vita_versions_scan(&versions) && errno == EIO && !versions.names && !versions.count);
    __read_failure = -1;
    for(int failure = 1; failure <= 2; ++failure) {
        __allocation_failure = failure;
        assert(!vita_versions_scan(&versions) && errno == ENOMEM && !versions.names && !versions.count);
    }
    __allocation_failure = 0;
    assert(vita_versions_scan(&versions) && versions.count == 3);
    vita_versions_free(&versions);

    a_asset_manager_t* manager = a_asset_manager_from_java(NULL, NULL);
    assert(vita_version_select("1.9.0c"));
    __expect_resources(manager, "version one");
    char version[64];
    assert(vita_version_read_metadata(version, sizeof(version)) && !strcmp(version, "1.9.0c"));
    char maximum[66];
    memset(maximum, '1', 63);
    maximum[63] = '\r';
    maximum[64] = '\n';
    maximum[65] = 0;
    __write(PATH_TO_VERSIONS "1.9.0c/vitaarc-apk-version.txt", maximum);
    assert(vita_version_read_metadata(version, sizeof(version)) && strlen(version) == 63);
    maximum[63] = '1';
    __write(PATH_TO_VERSIONS "1.9.0c/vitaarc-apk-version.txt", maximum);
    assert(!vita_version_read_metadata(version, sizeof(version)) && errno == EINVAL);
    __write(PATH_TO_VERSIONS "1.9.0c/vitaarc-apk-version.txt", "1.9.0c\r\n");
    assert(vita_version_read_metadata(version, sizeof(version)) && !strcmp(version, "1.9.0c"));
    const char* invalid[] = {"", "garbage\n", "2.0.0c\nextra", "2.0.0c/../\n"};
    for(unsigned index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        __write(PATH_TO_VERSIONS "1.9.0c/vitaarc-apk-version.txt", invalid[index]);
        assert(!vita_version_read_metadata(version, sizeof(version)) && errno == EINVAL);
    }
    assert(!vita_version_read_metadata(NULL, 64) && errno == EINVAL);
    assert(!vita_version_read_metadata(version, 1) && errno == EINVAL);
    const char* unsafe[] = {"", ".unfinished", "../Current build", "bad:name", "a/b", "a\\b", "trailing.", "tab\t"};
    for(unsigned index = 0; index < sizeof(unsafe) / sizeof(unsafe[0]); ++index)
        assert(!vita_version_select(unsafe[index]) && errno == EINVAL);
    const char* links[] = {"root-link", "assets-link", "library-link", "arm-link", "file-link", "obb-link"};
    for(unsigned index = 0; index < sizeof(links) / sizeof(links[0]); ++index)
        assert(!vita_version_select(links[index]) && errno == ELOOP);
    assert(!vita_version_select("missing-library") && !strcmp(vita_version_name(), "1.9.0c"));
    __expect_resources(manager, "version one");

    assert(vita_version_select("Current build"));
    __expect_resources(manager, "current build");
    assert(vita_version_read_metadata(version, sizeof(version)) && !strcmp(version, "Current build"));
    assert(!vita_version_read_metadata(version, 5) && errno == EOVERFLOW);
    __write(PATH_TO_VERSIONS "Current build/vitaarc-apk-version.txt", "2.0.0c\n");
    assert(vita_version_read_metadata(version, sizeof(version)) && !strcmp(version, "2.0.0c"));
    assert(rename(PATH_TO_VERSIONS "Current build/vitaarc-apk-version.txt",
               PATH_TO_VERSIONS "Current build/version-target.txt") == 0);
    assert(symlink("version-target.txt", PATH_TO_VERSIONS "Current build/vitaarc-apk-version.txt") == 0);
    assert(!vita_version_read_metadata(version, sizeof(version)) && errno == ELOOP);
    assert(vita_version_select("3.0.0c"));
    __expect_resources(manager, "version three");
    a_asset_cache_clear();
    return 0;
}
