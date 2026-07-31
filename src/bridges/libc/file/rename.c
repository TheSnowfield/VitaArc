#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef __vita__
#include <sys/uio.h>
#endif

#ifdef __vita__
#include <pthread.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/rng.h>
#include <psp2/rtc.h>
#include <platform/newlib.h>
#include <logcat/logcat.h>
#endif

#include "../file.h"
#include "../posix.h"

#include "shared.h"

#ifdef __vita__
static pthread_mutex_t __rename_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t __rename_sequence;

static int __empty_directory(const char* path)
{
    int uid = sceIoDopen(path);
    if(uid < 0) {
        return __native_error(uid);
    }

    int result;
    SceIoDirent entry;
    do {
        memset(&entry, 0, sizeof(entry));
        result = sceIoDread(uid, &entry);
    } while(result > 0 && (!strcmp(entry.d_name, ".") || !strcmp(entry.d_name, "..")));

    int closed = sceIoDclose(uid);
    if(result < 0) {
        return __native_error(result);
    }

    if(result > 0) {
        errno = ENOTEMPTY;
        return -1;
    }

    return closed < 0 ? __native_error(closed) : 0;
}

static int __rename_file(const char* old_path, const char* new_path)
{
    SceIoStat source, destination;
    int result = sceIoGetstat(old_path, &source);
    if(result < 0) {
        return __native_error(result);
    }

    if(strcmp(old_path, new_path) == 0) {
        return 0;
    }

    size_t old_drive = strcspn(old_path, ":"), new_drive = strcspn(new_path, ":");
    if(old_drive != new_drive || strncmp(old_path, new_path, old_drive) != 0) {
        errno = EXDEV;
        return -1;
    }

    bool directory = SCE_S_ISDIR(source.st_mode);
    size_t old_length = strlen(old_path);
    if(directory && !strncmp(old_path, new_path, old_length) && new_path[old_length] == '/') {
        errno = EINVAL;
        return -1;
    }

    result = sceIoGetstat(new_path, &destination);
    if(result < 0) {
        if(__vita_sce_errno_to_errno(result, 0) != ENOENT) {
            return __native_error(result);
        }

        result = sceIoRename(old_path, new_path);
        return result < 0 ? __native_error(result) : 0;
    }

    bool target_directory = SCE_S_ISDIR(destination.st_mode);
    if(directory != target_directory) {
        errno = target_directory ? EISDIR : ENOTDIR;
        return -1;
    }

    if(target_directory && __empty_directory(new_path) < 0) {
        return -1;
    }

    /* Use a single native replacement if the filesystem supports it. Vita's
     * usual backend instead returns EEXIST without modifying either entry. */
    result = sceIoRename(old_path, new_path);
    if(result >= 0) {
        return 0;
    }

    if(__vita_sce_errno_to_errno(result, 0) != EEXIST) {
        return __native_error(result);
    }

    const char* separator = strrchr(new_path, '/');
    if(!separator) {
        separator = strchr(new_path, ':');
    }

    size_t prefix = separator ? (size_t)(separator - new_path + 1) : 0;
    char backup[1024];
    for(unsigned attempt = 0; attempt < 64; ++attempt) {
        int length = snprintf(backup,
            sizeof(backup),
            "%.*s.vitaarc-rename-%08lx.bak",
            (int)prefix,
            new_path,
            (unsigned long)++__rename_sequence);
        if(length < 0 || (size_t)length >= sizeof(backup)) {
            errno = ENAMETOOLONG;
            return -1;
        }

        if(strcmp(old_path, backup) == 0) {
            continue;
        }

        /* No unlink: an existing backup or a racing writer must survive. */
        result = sceIoRename(new_path, backup);
        if(result >= 0) {
            break;
        }

        if(__vita_sce_errno_to_errno(result, 0) != EEXIST) {
            return __native_error(result);
        }
    }

    if(result < 0) {
        return __native_error(result);
    }

    result = sceIoRename(old_path, new_path);
    if(result < 0) {
        int error = __vita_sce_errno_to_errno(result, 0);
        int restored = sceIoRename(backup, new_path);
        if(restored < 0) {
            /* Preserve the old destination in the backup even after rollback fails. */
            log_e(TAG,
                "rename %s -> %s rollback failed; old destination retained at %s (0x%08x)",
                old_path,
                new_path,
                backup,
                restored);
            error = EIO;
        }

        errno = error;
        return -1;
    }

    int removed = target_directory ? sceIoRmdir(backup) : sceIoRemove(backup);
    if(removed < 0) {
        /* Replacement has committed. Returning failure would invite callers to
         * retry a source that no longer exists. Keep the old data for recovery. */
        log_w(TAG,
            "rename %s -> %s committed; old destination retained at %s (0x%08x)",
            old_path,
            new_path,
            backup,
            removed);
    }

    return 0;
}

#endif

int bridge_rename(const char* old_path, const char* new_path)
{
    char old_buffer[1024], new_buffer[1024];
    const char* old_resolved = bridge_resolve_path(old_path, old_buffer, sizeof(old_buffer));
    const char* new_resolved = bridge_resolve_path(new_path, new_buffer, sizeof(new_buffer));
    if(!old_resolved || !new_resolved) {
        return -1;
    }

#ifdef __vita__
    char* old_canonical = __realpath(old_resolved);
    if(!old_canonical) {
        return -1;
    }

    char* new_canonical = __realpath(new_resolved);
    if(!new_canonical) {
        int error = errno;
        free(old_canonical);
        errno = error;
        return -1;
    }

    pthread_mutex_lock(&__rename_mutex);
    int result = __rename_file(old_canonical, new_canonical);
    int error = errno;
    pthread_mutex_unlock(&__rename_mutex);
    free(old_canonical);
    free(new_canonical);
    errno = error;
    return result;

#else
    /* Some host-backed filesystems skip lookup for identical names. Match the
     * target's source-existence check even when the host optimizes that case. */
    struct stat source;
    if(lstat(old_resolved, &source) < 0) {
        return -1;
    }

    return rename(old_resolved, new_resolved);

#endif
}
