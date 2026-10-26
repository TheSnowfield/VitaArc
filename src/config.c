#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "config.h"

char PATH_TO_PACKAGE[VITAARC_PATH_CAPACITY] = PATH_TO_APP "package/";
char PATH_TO_LIBRARY[VITAARC_PATH_CAPACITY] = PATH_TO_APP "package/lib/armeabi-v7a/";
char PATH_TO_ASSETS[VITAARC_PATH_CAPACITY] = PATH_TO_APP "package/assets";
char PATH_TO_OBB[VITAARC_PATH_CAPACITY] = PATH_TO_APP "package/obb/";
char LIBRARY_LIBCOCOS2DCPP[VITAARC_PATH_CAPACITY] = PATH_TO_APP "package/lib/armeabi-v7a/libcocos2dcpp.so";

static char __selected[256];

static bool __safe_name(const char* name)
{
    if(!name || !name[0] || name[0] == '.' || strlen(name) >= sizeof(__selected))
        return false;
    for(const unsigned char* cursor = (const unsigned char*)name; *cursor; ++cursor) {
        if(*cursor < 32 || *cursor == 127 || strchr("/\\:<>\"|?*", *cursor))
            return false;
    }

    size_t length = strlen(name);
    return name[length - 1] != ' ' && name[length - 1] != '.';
}

static bool __path(char* output, const char* name, const char* suffix)
{
    int written = snprintf(output, VITAARC_PATH_CAPACITY, "%s%s/%s", PATH_TO_VERSIONS, name, suffix);
    if(written < 0 || written >= VITAARC_PATH_CAPACITY) {
        errno = ENAMETOOLONG;
        return false;
    }

    return true;
}

static bool __entry(const char* path, bool directory)
{
    struct stat info;
    if(lstat(path, &info) != 0)
        return false;
    if(S_ISLNK(info.st_mode)) {
        errno = ELOOP;
        return false;
    }
    if(directory ? !S_ISDIR(info.st_mode) : !S_ISREG(info.st_mode)) {
        errno = directory ? ENOTDIR : EINVAL;
        return false;
    }

    return true;
}

static bool __valid_package(const char* name)
{
    if(!__safe_name(name)) {
        errno = EINVAL;
        return false;
    }

    char path[VITAARC_PATH_CAPACITY];
    if(!__path(path, name, ""))
        return false;
    /* lstat must see the final entry itself, without a trailing slash. */
    path[strlen(path) - 1] = 0;
    if(!__entry(path, true))
        return false;
    const char* suffixes[] = {"assets", "lib", "lib/armeabi-v7a", "lib/armeabi-v7a/libcocos2dcpp.so"};
    for(unsigned index = 0; index < sizeof(suffixes) / sizeof(suffixes[0]); ++index) {
        if(!__path(path, name, suffixes[index]) || !__entry(path, index < 3))
            return false;
    }

    /* APKs without an expansion file can omit obb, but an existing entry must be a real directory. */
    if(!__path(path, name, "obb"))
        return false;
    return __entry(path, true) || errno == ENOENT;
}

void vita_versions_free(vita_versions_t* versions)
{
    if(!versions)
        return;
    for(unsigned index = 0; index < versions->count; ++index)
        free(versions->names[index]);
    free(versions->names);
    *versions = (vita_versions_t){0};
}

static int __compare(const void* first, const void* second)
{
    return strcmp(*(const char* const*)first, *(const char* const*)second);
}

bool vita_versions_scan(vita_versions_t* versions)
{
    if(!versions) {
        errno = EINVAL;
        return false;
    }

    *versions = (vita_versions_t){0};
    DIR* directory = opendir(PATH_TO_VERSIONS);
    if(!directory)
        return errno == ENOENT;

    int error = 0;
    for(;;) {
        errno = 0;
        struct dirent* entry = readdir(directory);
        if(!entry) {
            error = errno;
            break;
        }

        if(!__valid_package(entry->d_name)) {
            if(errno != ENOENT && errno != ENOTDIR && errno != EINVAL && errno != ELOOP) {
                error = errno;
                break;
            }
            continue;
        }
        size_t capacity = (size_t)versions->count + 1;
        if(versions->count == UINT_MAX || capacity > SIZE_MAX / sizeof(char*)) {
            error = ENOMEM;
            break;
        }

        char* name = malloc(strlen(entry->d_name) + 1);
        if(!name) {
            error = ENOMEM;
            break;
        }

        strcpy(name, entry->d_name);
        char** names = realloc(versions->names, capacity * sizeof(*names));
        if(!names) {
            free(name);
            error = ENOMEM;
            break;
        }

        versions->names = names;
        names[versions->count++] = name;
    }

    if(closedir(directory) != 0 && !error)
        error = errno;
    if(error) {
        vita_versions_free(versions);
        errno = error;
        return false;
    }

    if(versions->count > 1)
        qsort(versions->names, versions->count, sizeof(*versions->names), __compare);
    return true;
}

bool vita_version_select(const char* name)
{
    if(!__safe_name(name)) {
        errno = EINVAL;
        return false;
    }
    if(!__valid_package(name)) {
        return false;
    }

    /* All derived paths fit if the longest suffix fits; commit them together. */
    char library[VITAARC_PATH_CAPACITY];
    if(!__path(library, name, "lib/armeabi-v7a/libcocos2dcpp.so"))
        return false;
    memmove(__selected, name, strlen(name) + 1);
    __path(PATH_TO_PACKAGE, __selected, "");
    __path(PATH_TO_LIBRARY, __selected, "lib/armeabi-v7a/");
    __path(PATH_TO_ASSETS, __selected, "assets");
    __path(PATH_TO_OBB, __selected, "obb/");
    strcpy(LIBRARY_LIBCOCOS2DCPP, library);
    return true;
}

const char* vita_version_name(void)
{
    return __selected;
}

bool vita_version_read_metadata(char* version, size_t capacity)
{
    if(!version || capacity < 2) {
        errno = EINVAL;
        return false;
    }

    char path[VITAARC_PATH_CAPACITY];
    int written = snprintf(path, sizeof(path), "%svitaarc-apk-version.txt", PATH_TO_PACKAGE);
    if(written < 0 || written >= (int)sizeof(path)) {
        errno = ENAMETOOLONG;
        return false;
    }

    if(!__entry(path, false) && errno != ENOENT)
        return false;
    FILE* file = fopen(path, "rb");
    if(!file && !__selected[0] && errno == ENOENT)
        file = fopen(PATH_TO_APP "vitaarc-apk-version.txt", "rb");
    if(!file) {
        if(errno != ENOENT)
            return false;
        const char* fallback = __selected[0] ? __selected : "1.6.1c";
        if(strlen(fallback) >= capacity) {
            errno = EOVERFLOW;
            return false;
        }

        strcpy(version, fallback);
        return true;
    }

    size_t length = fread(version, 1, capacity - 1, file);
    int extra = fgetc(file);
    /* A maximum-length version still fits when its optional line ending does not. */
    if(extra == '\r')
        extra = fgetc(file);
    if(extra == '\n')
        extra = fgetc(file);
    bool valid = !ferror(file) && extra == EOF;
    fclose(file);
    while(length && (version[length - 1] == '\n' || version[length - 1] == '\r'))
        --length;
    version[length] = 0;
    if(!length || version[0] < '0' || version[0] > '9')
        valid = false;
    for(size_t index = 0; index < length; ++index) {
        char c = version[index];
        if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '-' ||
               c == '_'))
            valid = false;
    }

    if(!valid)
        errno = EINVAL;
    return valid;
}
