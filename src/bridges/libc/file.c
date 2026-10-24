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
#include <platform/io.h>
#include <platform/newlib.h>
#include <logcat/logcat.h>
#endif

#include "file.h"
#include "posix.h"
#include "file/shared.h"

#ifdef __vita__
/* Descriptor flags are separate from the shared file status of dup'ed fds. */
static pthread_mutex_t __descriptor_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned char __descriptor_flags[256];
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(vita_newlib_descriptor_t) == 20, "Vita newlib descriptor size");
_Static_assert(offsetof(vita_newlib_descriptor_t, type) == 4, "Vita newlib descriptor type");
_Static_assert(offsetof(vita_newlib_descriptor_t, references) == 8, "Vita newlib descriptor references");
_Static_assert(offsetof(vita_newlib_descriptor_t, path) == 12, "Vita newlib descriptor path");
_Static_assert(offsetof(vita_newlib_descriptor_t, flags) == 16, "Vita newlib descriptor flags");
#endif
#endif

typedef struct {
    int guest;
    int native;
} open_flag_t;

static int __open_flags(int flags)
{
    const int known = 3 | BIONIC_O_CREAT | BIONIC_O_EXCL | BIONIC_O_NOCTTY | BIONIC_O_TRUNC | BIONIC_O_APPEND |
        BIONIC_O_NONBLOCK | BIONIC_O_DSYNC | BIONIC_O_DIRECTORY | BIONIC_O_NOFOLLOW | BIONIC_O_DIRECT |
        BIONIC_O_LARGEFILE | BIONIC_O_CLOEXEC | BIONIC_O_SYNC;
    if((flags & ~known) || (flags & 3) == 3) {
        errno = EINVAL;
        return -1;
    }

    int native = flags & 3;
    static const open_flag_t translations[] = {
        {BIONIC_O_CREAT, O_CREAT},
        {BIONIC_O_EXCL, O_EXCL},
        {BIONIC_O_NOCTTY, O_NOCTTY},
        {BIONIC_O_TRUNC, O_TRUNC},
        {BIONIC_O_APPEND, O_APPEND},
        {BIONIC_O_NONBLOCK, O_NONBLOCK},
        {BIONIC_O_DIRECTORY, O_DIRECTORY},
        {BIONIC_O_NOFOLLOW, O_NOFOLLOW},
        {BIONIC_O_DIRECT, O_DIRECT},
        {BIONIC_O_CLOEXEC, O_CLOEXEC},
    };

    for(size_t index = 0; index < sizeof(translations) / sizeof(translations[0]); ++index) {
        if(flags & translations[index].guest) {
            native |= translations[index].native;
        }
    }

    if(flags & (BIONIC_O_SYNC & ~BIONIC_O_DSYNC)) {
        native |= O_SYNC;
    } else if(flags & BIONIC_O_DSYNC) {
#ifdef O_DSYNC
        native |= O_DSYNC;
#else
        native |= O_SYNC;
#endif
    }

#ifdef __vita__
    if(flags & BIONIC_O_DIRECT) {
        errno = ENOTSUP;
        return -1;
    }

#endif
    return native;
}

#ifdef __vita__
static int __register_descriptor(int uid, int type, char* path, int flags)
{
    int fd = __vita_acquire_descriptor();
    if(fd < 0) {
        errno = EMFILE;
        return -1;
    }

    /* As in newlib's _open_r, publish fields before exposing the returned fd.
     * The allocator has already installed and reserved this descriptor slot. */
    vita_newlib_descriptor_t* descriptor = __vita_fdmap[fd];
    descriptor->uid = uid;
    descriptor->type = type;
    descriptor->path = path;
    descriptor->flags = flags;
    return fd;
}

static ssize_t __read_random(void* cookie, char* buffer, size_t size)
{
    (void)cookie;
    if(size > INT32_MAX || (!buffer && size)) {
        errno = !buffer && size ? EFAULT : EINVAL;
        return -1;
    }

    size_t done = 0;
    while(done < size) {
        size_t chunk = size - done > 64 ? 64 : size - done;
        int result = sceKernelGetRandomNumber(buffer + done, chunk);
        if(result < 0) {
            return done ? (ssize_t)done : __native_error(result);
        }

        done += chunk;
    }

    return (ssize_t)done;
}

#endif

static int __open_file(const char* path, int flags, unsigned int mode)
{
    int native_flags = __open_flags(flags);
    if(native_flags < 0) {
        return -1;
    }

    char buffer[1024];
    const char* resolved = bridge_resolve_path(path, buffer, sizeof(buffer));
    if(!resolved) {
        return -1;
    }

#ifdef __vita__
    if(strcmp(resolved, "/dev/urandom") == 0) {
        if((flags & (BIONIC_O_CREAT | BIONIC_O_EXCL)) == (BIONIC_O_CREAT | BIONIC_O_EXCL)) {
            errno = EEXIST;
            return -1;
        }

        if(flags & BIONIC_O_DIRECTORY) {
            errno = ENOTDIR;
            return -1;
        }

        if((flags & 3) != BIONIC_O_RDONLY) {
            errno = ENOTSUP;
            return -1;
        }

        return __register_descriptor(0, VITAARC_DESCRIPTOR_RANDOM, NULL, native_flags);
    }

    char* canonical = __realpath(resolved);
    if(!canonical) {
        return -1;
    }

    SceIoStat status;
    int found = sceIoGetstat(canonical, &status);
    bool directory = found >= 0 && SCE_S_ISDIR(status.st_mode);
    if(found >= 0 && SCE_S_ISLNK(status.st_mode) && flags & BIONIC_O_NOFOLLOW) {
        free(canonical);
        errno = ELOOP;
        return -1;
    }

    if((native_flags & O_DIRECTORY) && !directory) {
        free(canonical);
        if(found < 0) {
            return __native_error(found);
        }

        errno = ENOTDIR;
        return -1;
    }

    if(directory &&
        ((flags & 3) != BIONIC_O_RDONLY || flags & BIONIC_O_TRUNC ||
            (flags & (BIONIC_O_CREAT | BIONIC_O_EXCL)) == (BIONIC_O_CREAT | BIONIC_O_EXCL))) {
        free(canonical);
        errno = (flags & (BIONIC_O_CREAT | BIONIC_O_EXCL)) == (BIONIC_O_CREAT | BIONIC_O_EXCL) ? EEXIST : EISDIR;
        return -1;
    }

    unsigned perm = vita_io_mode(mode);
    int uid = directory ? sceIoDopen(canonical) : sceIoOpen(canonical, _fcntl2sony(native_flags), perm);
    if(uid < 0) {
        int error = __vita_sce_errno_to_errno(uid, 0);
        if((flags & 3) != BIONIC_O_RDONLY || flags & BIONIC_O_CREAT) {
            log_e(TAG,
                "Open failed: %s flags=0x%X mode=0%o vita=0%o error=0x%08X",
                canonical,
                (unsigned)flags,
                mode & 07777,
                perm,
                (unsigned)uid);
        }

        free(canonical);
        errno = error;
        return -1;
    }

    int fd = __register_descriptor(uid, directory ? 1 : 0, canonical, native_flags);
    if(fd < 0) {
        int error = errno;
        if(directory) {
            sceIoDclose(uid);
        } else {
            sceIoClose(uid);
        }

        free(canonical);
        errno = error;
    }

    return fd;

#else
    return open(resolved, native_flags, mode);

#endif
}

static int __open_tracked(const char* path, int flags, unsigned int mode)
{
#ifdef __vita__
    pthread_mutex_lock(&__descriptor_mutex);
#endif
    int fd = __open_file(path, flags, mode);
#ifdef __vita__
    int error = errno;
    if(fd >= 0) {
        __descriptor_flags[fd] = (flags & BIONIC_O_CLOEXEC) ? 1 : 0;
    }

    pthread_mutex_unlock(&__descriptor_mutex);
    errno = error;
#endif
    return fd;
}

int bridge_open(const char* path, int flags, ...)
{
    unsigned int mode = 0;
    if(flags & BIONIC_O_CREAT) {
        va_list values;
        va_start(values, flags);
        mode = va_arg(values, unsigned int);
        va_end(values);
    }

    return __open_tracked(path, flags, mode);
}

int bridge_open_2(const char* path, int flags)
{
    if(flags & BIONIC_O_CREAT) {
        errno = EINVAL;
        return -1;
    }

    return __open_tracked(path, flags, 0);
}

ssize_t bridge_read(int fd, void* buffer, size_t size)
{
    if(size > INT32_MAX || (!buffer && size)) {
        errno = !buffer && size ? EFAULT : EINVAL;
        return -1;
    }

#ifdef __vita__
    vita_newlib_descriptor_t* descriptor = __vita_fd_grab(fd);
    if(!descriptor) {
        errno = EBADF;
        return -1;
    }

    if(__random_descriptor(descriptor)) {
        ssize_t result = __read_random(NULL, buffer, size);
        int error = errno;
        __vita_fd_drop(descriptor);
        errno = error;
        return result;
    }

    if(descriptor->type == 0) {
        if((__atomic_load_n(&descriptor->flags, __ATOMIC_RELAXED) & O_ACCMODE) == O_WRONLY) {
            __vita_fd_drop(descriptor);
            errno = EBADF;
            return -1;
        }

        int result = size ? sceIoRead(descriptor->uid, buffer, size) : 0;
        __vita_fd_drop(descriptor);
        return result < 0 ? __native_error(result) : result;
    }

    bool directory = descriptor->type == 1;
    __vita_fd_drop(descriptor);
    if(directory) {
        errno = EISDIR;
        return -1;
    }

#endif
    return read(fd, buffer, size);
}

ssize_t bridge_write(int fd, const void* buffer, size_t size)
{
    if(size > INT32_MAX || (!buffer && size)) {
        errno = !buffer && size ? EFAULT : EINVAL;
        return -1;
    }

#ifdef __vita__
    vita_newlib_descriptor_t* descriptor = __vita_fd_grab(fd);
    if(!descriptor) {
        errno = EBADF;
        return -1;
    }

    bool random = __random_descriptor(descriptor);
    if(descriptor->type == 0) {
        int flags = __atomic_load_n(&descriptor->flags, __ATOMIC_RELAXED);
        if((flags & O_ACCMODE) == O_RDONLY) {
            __vita_fd_drop(descriptor);
            errno = EBADF;
            return -1;
        }

        int result = 0;
        if(size && flags & O_APPEND) {
            int64_t offset = sceIoLseek(descriptor->uid, 0, SEEK_END);
            if(offset < 0) {
                result = (int)offset;
            }
        }

        if(result >= 0 && size) {
            result = sceIoWrite(descriptor->uid, buffer, size);
        }

        if(result > 0 && flags & O_SYNC) {
            int synced = sceIoSyncByFd(descriptor->uid, 0);
            if(synced < 0) {
                result = synced;
            }
        }

        __vita_fd_drop(descriptor);
        return result < 0 ? __native_error(result) : result;
    }

    bool directory = descriptor->type == 1;
    __vita_fd_drop(descriptor);
    if(random) {
        errno = EBADF;
        return -1;
    }

    if(directory) {
        errno = EISDIR;
        return -1;
    }

#endif
    return write(fd, buffer, size);
}

int bridge_close(int fd)
{
#ifdef __vita__
    pthread_mutex_lock(&__descriptor_mutex);
#endif
    int result = close(fd);
#ifdef __vita__
    int error = errno;
    if(result == 0) {
        __descriptor_flags[fd] = 0;
    }

    pthread_mutex_unlock(&__descriptor_mutex);
    errno = error;
#endif
    return result;
}

int bridge_writev(int fd, const void* vectors, int count)
{
    if(count < 0 || count > 1024 || (!vectors && count)) {
        errno = !vectors && count > 0 ? EFAULT : EINVAL;
        return -1;
    }

    const bridge_io_vector_t* items = vectors;
    size_t requested = 0;
    for(int index = 0; index < count; ++index) {
        if(!items[index].base && items[index].length) {
            errno = EFAULT;
            return -1;
        }

        if(items[index].length > INT32_MAX - requested) {
            errno = EINVAL;
            return -1;
        }

        requested += items[index].length;
    }

    if(!requested) {
        return (int)bridge_write(fd, NULL, 0);
    }

#ifdef __vita__
    int total = 0;
    for(int index = 0; index < count; ++index) {
        if(!items[index].length) {
            continue;
        }

        ssize_t written = bridge_write(fd, items[index].base, items[index].length);
        if(written < 0) {
            return total ? total : -1;
        }

        total += (int)written;
        if((size_t)written != items[index].length) {
            break;
        }
    }

    return total;

#else
    _Static_assert(sizeof(bridge_io_vector_t) == sizeof(struct iovec), "host iovec layout");
    return (int)writev(fd, (const struct iovec*)vectors, count);

#endif
}

FILE* bridge_fdopen(int fd, const char* mode)
{
#ifdef __vita__
    vita_newlib_descriptor_t* descriptor = __vita_fd_grab(fd);
    if(!descriptor) {
        errno = EBADF;
        return NULL;
    }

    bool custom = __random_descriptor(descriptor) || (__atomic_load_n(&descriptor->flags, __ATOMIC_RELAXED) & O_SYNC);
    __vita_fd_drop(descriptor);
    if(custom) {
        /* Native streams bypass both the RNG reader and the synchronous writer. */
        errno = ENOTSUP;
        return NULL;
    }

#endif
    return fdopen(fd, mode);
}

FILE* bridge_fopen(const char* path, const char* mode)
{
    if(!mode || !*mode) {
        errno = EINVAL;
        return NULL;
    }

    char buffer[1024];
    const char* resolved = bridge_resolve_path(path, buffer, sizeof(buffer));
    if(!resolved) {
        return NULL;
    }

#ifdef __vita__
    if(strcmp(resolved, "/dev/urandom") == 0) {
        // C++ random_device uses stdio rather than the open/read descriptor path.
        // A native cookie stream keeps fread/fclose on the real Vita RNG as well.
        if(mode[0] != 'r' || strspn(mode + 1, "be") != strlen(mode + 1)) {
            errno = ENOTSUP;
            return NULL;
        }

        cookie_io_functions_t functions = {.read = __read_random};

        return fopencookie(NULL, "r", functions);
    }

    pthread_mutex_lock(&__descriptor_mutex);
#endif
    FILE* stream = fopen(resolved, mode);
#ifdef __vita__
    int error = errno;
    if(stream) {
        int fd = fileno(stream);
        if(fd >= 0 && fd < 256) {
            __descriptor_flags[fd] = strchr(mode, 'e') ? 1 : 0;
        }
    }

    pthread_mutex_unlock(&__descriptor_mutex);
    errno = error;
#endif
    return stream;
}

int bridge_fclose(FILE* stream)
{
#ifdef __vita__
    pthread_mutex_lock(&__descriptor_mutex);
    int fd = fileno(stream);
#endif
    int result = fclose(stream);
#ifdef __vita__
    int error = errno;
    if(fd >= 0 && fd < 256) {
        vita_newlib_descriptor_t* descriptor = __vita_fd_grab(fd);
        if(descriptor) {
            __vita_fd_drop(descriptor);
        } else {
            __descriptor_flags[fd] = 0;
        }
    }

    pthread_mutex_unlock(&__descriptor_mutex);
    errno = error;
#endif
    return result;
}

int32_t bridge_lseek(int fd, int32_t offset, int whence)
{
    int64_t result = bridge_lseek64(fd, offset, whence);
    if(result > INT32_MAX) {
        errno = EOVERFLOW;
        return -1;
    }

    return (int32_t)result;
}

int bridge_ftruncate(int fd, int32_t length)
{
    return bridge_ftruncate64(fd, length);
}

static int __status_flags(int flags)
{
    int guest = (flags & O_ACCMODE) | BIONIC_O_LARGEFILE;
    if(flags & O_APPEND) {
        guest |= BIONIC_O_APPEND;
    }

    if(flags & O_NONBLOCK) {
        guest |= BIONIC_O_NONBLOCK;
    }

    if((flags & O_SYNC) == O_SYNC) {
        guest |= BIONIC_O_SYNC;
    }

#ifdef O_DSYNC
    else if(flags & O_DSYNC) {
        guest |= BIONIC_O_DSYNC;
    }

#endif
    if(flags & O_DIRECT) {
        guest |= BIONIC_O_DIRECT;
    }

    if(flags & O_DIRECTORY) {
        guest |= BIONIC_O_DIRECTORY;
    }

    if(flags & O_NOFOLLOW) {
        guest |= BIONIC_O_NOFOLLOW;
    }

    return guest;
}

static int __fcntl(int fd, int command, int argument)
{
    int native;
    switch(command) {
        case 0:
            native = F_DUPFD;
            break;

        case 1:
            native = F_GETFD;
            break;

        case 2:
            native = F_SETFD;
            break;

        case 3:
            native = F_GETFL;
            break;

        case 4:
            native = F_SETFL;
            break;

        case 1030:
#ifdef __vita__
            native = F_DUPFD;
#else
            native = F_DUPFD_CLOEXEC;
#endif
            break;

        default:
            /* Linux flock payloads and inter-process locks are not newlib flock. */
            errno = ENOTSUP;
            return -1;
    }

    if(command == 4) {
        argument = __open_flags(argument & (BIONIC_O_APPEND | BIONIC_O_NONBLOCK | BIONIC_O_DIRECT));
        if(argument < 0) {
            return -1;
        }
    }

#ifdef __vita__
    vita_newlib_descriptor_t* descriptor = __vita_fd_grab(fd);
    if(!descriptor) {
        errno = EBADF;
        return -1;
    }

    bool random = __random_descriptor(descriptor);
    int flags = __atomic_load_n(&descriptor->flags, __ATOMIC_RELAXED);
    if(command == 1 || command == 2) {
        __vita_fd_drop(descriptor);
        if(command == 1) {
            return __descriptor_flags[fd];
        }

        if(argument & ~1) {
            errno = EINVAL;
            return -1;
        }

        __descriptor_flags[fd] = (unsigned char)argument;
        return 0;
    }

    if(command == 3 && (random || descriptor->type == 0 || descriptor->type == 1)) {
        __vita_fd_drop(descriptor);
        return __status_flags(flags);
    }

    if(command == 4 && (random || descriptor->type == 0)) {
        const int mutable = O_APPEND | O_NONBLOCK;
        __atomic_store_n(&descriptor->flags, (flags & ~mutable) | (argument & mutable), __ATOMIC_RELAXED);
        __vita_fd_drop(descriptor);
        return 0;
    }

    __vita_fd_drop(descriptor);
#endif
    int result = fcntl(fd, native, argument);
    return command == 3 && result >= 0 ? __status_flags(result) : result;
}

int bridge_fcntl(int fd, int command, ...)
{
    int argument = 0;
    if(command == 0 || command == 2 || command == 4 || command == 1030) {
        va_list values;
        va_start(values, command);
        argument = va_arg(values, int);
        va_end(values);
    }

#ifdef __vita__
    pthread_mutex_lock(&__descriptor_mutex);
#endif
    int result = __fcntl(fd, command, argument);
#ifdef __vita__
    int error = errno;
    if(result >= 0 && (command == 0 || command == 1030)) {
        __descriptor_flags[result] = command == 1030 ? 1 : 0;
    }

    pthread_mutex_unlock(&__descriptor_mutex);
    errno = error;
#endif
    return result;
}

int bridge_access(const char* path, int mode)
{
    char buffer[1024];
    const char* resolved = bridge_resolve_path(path, buffer, sizeof(buffer));
    return resolved ? access(resolved, mode) : -1;
}

int bridge_unlink(const char* path)
{
    char buffer[1024];
    const char* resolved = bridge_resolve_path(path, buffer, sizeof(buffer));
    return resolved ? unlink(resolved) : -1;
}

int bridge_remove(const char* path)
{
    char buffer[1024];
    const char* resolved = bridge_resolve_path(path, buffer, sizeof(buffer));
    return resolved ? remove(resolved) : -1;
}
