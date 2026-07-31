#define _GNU_SOURCE

#include <errno.h>
#include <stddef.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __vita__
#include <psp2/io/dirent.h>
#include <platform/newlib.h>
#else
#include <dirent.h>
#endif

#include "directory.h"
#include "file.h"

_Static_assert(offsetof(bridge_dirent_t, inode) == 0, "Bionic dirent inode");
_Static_assert(offsetof(bridge_dirent_t, offset) == 8, "Bionic dirent offset");
_Static_assert(offsetof(bridge_dirent_t, record_length) == 16, "Bionic dirent record length");
_Static_assert(offsetof(bridge_dirent_t, type) == 18, "Bionic dirent type");
_Static_assert(offsetof(bridge_dirent_t, name) == 19, "Bionic dirent name");
_Static_assert(sizeof(bridge_dirent_t) == 280, "Bionic dirent size");

struct bridge_directory {
    int fd;
    int32_t position;
    bridge_dirent_t entry;
#ifndef __vita__
    DIR* native;
#endif
};

/* Also serialize descriptor UID replacement against reads through a dup'ed
 * directory fd. No guest callback runs while this mutex is held. */
static pthread_mutex_t __directory_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef __vita__
static int __native_error(int result)
{
    errno = __vita_sce_errno_to_errno(result, 0);
    return -1;
}

static vita_newlib_descriptor_t* __descriptor(int fd)
{
    vita_newlib_descriptor_t* descriptor = __vita_fd_grab(fd);
    if(!descriptor) {
        errno = EBADF;
        return NULL;
    }

    if(descriptor->type != 1) {
        __vita_fd_drop(descriptor);
        errno = ENOTDIR;
        return NULL;
    }

    return descriptor;
}

static uint8_t __type(unsigned mode)
{
    if(SCE_S_ISDIR(mode)) {
        return 4;
    }

    if(SCE_S_ISREG(mode)) {
        return 8;
    }

    if(SCE_S_ISLNK(mode)) {
        return 10;
    }

    return 0;
}

static uint64_t __inode(const char* directory, const char* name)
{
    /* Match file.c's path-derived inode approximation for regular child names. */
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t length = strlen(directory);
    if(!strcmp(name, ".")) {
        name = "";
    } else if(!strcmp(name, "..")) {
        const char* colon = strchr(directory, ':');
        size_t root = colon ? (size_t)(colon - directory) + 1 : 0;
        if(directory[root] == '/') {
            ++root;
        }

        while(length > root && directory[length - 1] == '/') {
            --length;
        }

        while(length > root && directory[length - 1] != '/') {
            --length;
        }

        if(length > root && directory[length - 1] == '/') {
            --length;
        }

        name = "";
    }

    for(size_t index = 0; index < length; ++index) {
        hash = (hash ^ (unsigned char)directory[index]) * UINT64_C(1099511628211);
    }

    if(*name && length && directory[length - 1] != '/') {
        hash = (hash ^ '/') * UINT64_C(1099511628211);
    }

    while(*name) {
        hash = (hash ^ (unsigned char)*name++) * UINT64_C(1099511628211);
    }

    return hash;
}

#endif

bridge_directory_t* bridge_fdopendir(int fd)
{
#ifdef __vita__
    vita_newlib_descriptor_t* descriptor = __descriptor(fd);
    if(!descriptor) {
        return NULL;
    }

    __vita_fd_drop(descriptor);
#endif
    bridge_directory_t* directory = calloc(1, sizeof(*directory));
    if(!directory) {
        return NULL;
    }

    directory->fd = fd;
#ifndef __vita__
    directory->native = fdopendir(fd);
    if(!directory->native) {
        free(directory);
        return NULL;
    }

#endif
    return directory;
}

bridge_directory_t* bridge_opendir(const char* path)
{
    int fd = bridge_open(path, BIONIC_O_RDONLY | BIONIC_O_DIRECTORY | BIONIC_O_CLOEXEC);
    if(fd < 0) {
        return NULL;
    }

    bridge_directory_t* directory = bridge_fdopendir(fd);
    if(!directory) {
        int error = errno;
        bridge_close(fd);
        errno = error;
    }

    return directory;
}

static bridge_dirent_t* __read(bridge_directory_t* directory)
{
    int saved = errno;
    if(directory->position == INT32_MAX) {
        errno = EOVERFLOW;
        return NULL;
    }

    bridge_dirent_t entry = {0};

#ifdef __vita__
    vita_newlib_descriptor_t* descriptor = __descriptor(directory->fd);
    if(!descriptor) {
        return NULL;
    }

    SceIoDirent native;
    int result;
    do {
        memset(&native, 0, sizeof(native));
        result = sceIoDread(descriptor->uid, &native);
    } while(result < 0 && __vita_sce_errno_to_errno(result, 0) == EINTR);

    if(result <= 0) {
        __vita_fd_drop(descriptor);
        if(result < 0) {
            __native_error(result);
        } else {
            errno = saved;
        }

        return NULL;
    }

    ++directory->position;
    size_t length = strnlen(native.d_name, sizeof(native.d_name));
    if(length == sizeof(native.d_name)) {
        __vita_fd_drop(descriptor);
        errno = ENAMETOOLONG;
        return NULL;
    }

    entry.inode = __inode(descriptor->path ? descriptor->path : "", native.d_name);
    entry.type = __type(native.d_stat.st_mode);
    memcpy(entry.name, native.d_name, length + 1);
    __vita_fd_drop(descriptor);
#else
    struct dirent* native = readdir(directory->native);
    if(!native) {
        return NULL;
    }

    ++directory->position;
    size_t length = strnlen(native->d_name, sizeof(entry.name));
    if(length == sizeof(entry.name)) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    entry.inode = native->d_ino;
    entry.type = native->d_type;
    memcpy(entry.name, native->d_name, length + 1);
#endif
    entry.offset = directory->position;
    entry.record_length = (offsetof(bridge_dirent_t, name) + length + 1 + 7) & ~7u;
    directory->entry = entry;
    errno = saved;
    return &directory->entry;
}

bridge_dirent_t* bridge_readdir(bridge_directory_t* directory)
{
    if(!directory) {
        errno = EBADF;
        return NULL;
    }

    pthread_mutex_lock(&__directory_mutex);
    bridge_dirent_t* entry = __read(directory);
    pthread_mutex_unlock(&__directory_mutex);
    return entry;
}

int bridge_readdir_r(bridge_directory_t* directory, bridge_dirent_t* entry, bridge_dirent_t** result)
{
    if(!result || !entry || !directory) {
        if(result) {
            *result = NULL;
        }

        return EINVAL;
    }

    int saved = errno;
    *result = NULL;
    pthread_mutex_lock(&__directory_mutex);
    errno = 0;
    bridge_dirent_t* next = __read(directory);
    int error = next ? 0 : errno;
    if(next) {
        *entry = *next;
        *result = entry;
    }

    pthread_mutex_unlock(&__directory_mutex);
    errno = saved;
    return error;
}

int bridge_dirfd(bridge_directory_t* directory)
{
    if(!directory) {
        errno = EBADF;
        return -1;
    }

    return directory->fd;
}

int bridge_closedir(bridge_directory_t* directory)
{
    if(!directory) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&__directory_mutex);
#ifdef __vita__
    int result = bridge_close(directory->fd);
#else
    int result = closedir(directory->native);
#endif
    free(directory);
    pthread_mutex_unlock(&__directory_mutex);
    return result;
}

static int __seek(bridge_directory_t* directory, int32_t position)
{
    if(position < 0) {
        errno = EINVAL;
        return -1;
    }

    int saved = errno;
#ifdef __vita__
    vita_newlib_descriptor_t* descriptor = __descriptor(directory->fd);
    if(!descriptor) {
        return -1;
    }

    if(!descriptor->path) {
        __vita_fd_drop(descriptor);
        errno = EBADF;
        return -1;
    }

    int candidate = sceIoDopen(descriptor->path);
    if(candidate < 0) {
        __vita_fd_drop(descriptor);
        return __native_error(candidate);
    }

    int result = 1;
    for(int32_t index = 0; index < position && result > 0; ++index) {
        SceIoDirent ignored = {0};

        do {
            result = sceIoDread(candidate, &ignored);
        } while(result < 0 && __vita_sce_errno_to_errno(result, 0) == EINTR);
    }

    int error = 0;
    if(result > 0) {
        result = sceIoDclose(descriptor->uid);
        if(result >= 0) {
            descriptor->uid = candidate;
        } else {
            error = __vita_sce_errno_to_errno(result, 0);
        }
    } else {
        error = result < 0 ? __vita_sce_errno_to_errno(result, 0) : EINVAL;
    }

    if(error) {
        sceIoDclose(candidate);
    }

    __vita_fd_drop(descriptor);
    if(error) {
        errno = error;
        return -1;
    }

#else
    long old = telldir(directory->native);
    if(old < 0) {
        return -1;
    }

    rewinddir(directory->native);
    for(int32_t index = 0; index < position; ++index) {
        errno = 0;
        if(!readdir(directory->native)) {
            int error = errno ? errno : EINVAL;
            seekdir(directory->native, old);
            errno = error;
            return -1;
        }
    }

#endif
    directory->position = position;
    errno = saved;
    return 0;
}

void bridge_seekdir(bridge_directory_t* directory, int32_t position)
{
    if(!directory) {
        errno = EBADF;
        return;
    }

    pthread_mutex_lock(&__directory_mutex);
    __seek(directory, position);
    pthread_mutex_unlock(&__directory_mutex);
}

void bridge_rewinddir(bridge_directory_t* directory)
{
    bridge_seekdir(directory, 0);
}

int32_t bridge_telldir(bridge_directory_t* directory)
{
    if(!directory) {
        errno = EBADF;
        return -1;
    }

    pthread_mutex_lock(&__directory_mutex);
    int32_t position = directory->position;
    pthread_mutex_unlock(&__directory_mutex);
    return position;
}
