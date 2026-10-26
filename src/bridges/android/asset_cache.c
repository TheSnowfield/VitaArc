#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "asset_cache.h"

#define CACHE_FOLDERS 16
#define CACHE_NAMES 4096
#define CACHE_BYTES (64 * 1024)

typedef struct asset_folder_t {
    char* path;
    char** names;
    size_t count;
    size_t capacity;
    size_t bytes;
    uint64_t used;
} asset_folder_t;

/* The selected version is fixed while running. Keep only a bounded set of directory
 * names; reading image/audio data continues through the regular file stream. */
static asset_folder_t* __folders[CACHE_FOLDERS];
static pthread_mutex_t __mutex = PTHREAD_MUTEX_INITIALIZER;
static uint64_t __serial;

static void __free(asset_folder_t* folder)
{
    if(!folder) {
        return;
    }

    for(size_t index = 0; index < folder->count; ++index) {
        free(folder->names[index]);
    }

    free(folder->names);
    free(folder->path);
    free(folder);
}

static bool __append(asset_folder_t* folder, const char* name)
{
    size_t bytes = strlen(name) + 1;
    if(folder->count == CACHE_NAMES || bytes > CACHE_BYTES - folder->bytes) {
        return false;
    }

    if(folder->count == folder->capacity) {
        size_t capacity = folder->capacity ? folder->capacity * 2 : 16;
        char** names = realloc(folder->names, capacity * sizeof(*names));
        if(!names) {
            return false;
        }

        folder->names = names;
        folder->capacity = capacity;
    }

    char* copy = strdup(name);
    if(!copy) {
        return false;
    }

    folder->names[folder->count++] = copy;
    folder->bytes += bytes;
    return true;
}

static int __compare(const void* first, const void* second)
{
#ifdef __vita__
    return strcasecmp(*(const char* const*)first, *(const char* const*)second);
#else
    return strcmp(*(const char* const*)first, *(const char* const*)second);
#endif
}

static asset_folder_t* __load(const char* path)
{
    asset_folder_t* folder = calloc(1, sizeof(*folder));
    if(!folder) {
        return NULL;
    }

    folder->path = strdup(path);
    if(!folder->path) {
        __free(folder);
        return NULL;
    }

    DIR* stream = opendir(path);
    if(!stream) {
        if(errno == ENOENT || errno == ENOTDIR) {
            return folder;
        }

        __free(folder);
        return NULL;
    }

    bool complete = true;
    for(;;) {
        errno = 0;
        struct dirent* entry = readdir(stream);
        if(!entry) {
            complete = errno == 0;
            break;
        }

        if(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
            continue;
        }

        if(!__append(folder, entry->d_name)) {
            complete = false;
            break;
        }
    }

    if(closedir(stream)) {
        complete = false;
    }

    if(!complete) {
        __free(folder);
        return NULL;
    }

    if(folder->count > 1) {
        qsort(folder->names, folder->count, sizeof(*folder->names), __compare);
    }

    return folder;
}

static asset_folder_t* __find(const char* path)
{
    for(unsigned index = 0; index < CACHE_FOLDERS; ++index) {
        asset_folder_t* folder = __folders[index];
        if(folder && !strcmp(folder->path, path)) {
            return folder;
        }
    }

    return NULL;
}

static bool __contains(asset_folder_t* folder, const char* name)
{
    folder->used = ++__serial;
    if(!folder->count) {
        return false;
    }

    return bsearch(&name, folder->names, folder->count, sizeof(*folder->names), __compare) != NULL;
}

static unsigned __slot(void)
{
    unsigned oldest = 0;
    for(unsigned index = 0; index < CACHE_FOLDERS; ++index) {
        if(!__folders[index]) {
            return index;
        }

        if(__folders[index]->used < __folders[oldest]->used) {
            oldest = index;
        }
    }

    return oldest;
}

bool a_asset_cached(const char* path)
{
    const char* slash = strrchr(path, '/');
    if(!slash || !slash[1]) {
        return true;
    }

    size_t length = slash - path;
    char parent[1024];
    if(!length || length >= sizeof(parent)) {
        return true;
    }

    memcpy(parent, path, length);
    parent[length] = 0;
    pthread_mutex_lock(&__mutex);
    asset_folder_t* folder = __find(parent);
    if(folder) {
        bool found = __contains(folder, slash + 1);
        pthread_mutex_unlock(&__mutex);
        return found;
    }

    pthread_mutex_unlock(&__mutex);
    asset_folder_t* loaded = __load(parent);
    if(!loaded) {
        return true;
    }

    /* Directory I/O runs outside the lock. Another reader may have installed
     * the same folder before this load completed. Publish or discard atomically. */
    asset_folder_t* retired = NULL;
    pthread_mutex_lock(&__mutex);
    folder = __find(parent);
    if(!folder) {
        unsigned slot = __slot();
        retired = __folders[slot];
        folder = __folders[slot] = loaded;
        loaded = NULL;
    }

    bool found = __contains(folder, slash + 1);
    pthread_mutex_unlock(&__mutex);
    __free(loaded);
    __free(retired);
    return found;
}

void a_asset_cache_clear(void)
{
    asset_folder_t* folders[CACHE_FOLDERS];
    pthread_mutex_lock(&__mutex);
    memcpy(folders, __folders, sizeof(folders));
    memset(__folders, 0, sizeof(__folders));
    __serial = 0;
    pthread_mutex_unlock(&__mutex);

    for(unsigned index = 0; index < CACHE_FOLDERS; ++index) {
        __free(folders[index]);
    }
}
