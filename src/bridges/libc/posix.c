#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __vita__
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <platform/newlib.h>
#else
#include <sys/ioctl.h>
#endif

#include <config.h>
#include "posix.h"

#if defined(__vita__) && UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(struct timespec) == 8, "Android ARM timespec ABI");
_Static_assert(sizeof(struct timeval) == 8, "Android ARM timeval ABI");
#endif

const char* bridge_resolve_path(const char* path, char* buffer, size_t capacity)
{
    static const char* prefixes[] = {"/data/data/moe.low.arc", "/data/user/0/moe.low.arc", "/data/data"};

    if(!path) {
        errno = EFAULT;
        return NULL;
    }

    for(size_t index = 0; index < sizeof(prefixes) / sizeof(prefixes[0]); ++index) {
        size_t length = strlen(prefixes[index]);
        if(strncmp(path, prefixes[index], length) != 0 || (path[length] && path[length] != '/')) {
            continue;
        }

        const char* suffix = path + length;
        while(*suffix == '/') {
            ++suffix;
        }

        int size = snprintf(buffer, capacity, "%s%s", PATH_TO_DATA, suffix);
        if(size < 0 || (size_t)size >= capacity) {
            errno = ENAMETOOLONG;
            return NULL;
        }

        return buffer;
    }

    return path;
}

void* bridge_memmem(const void* haystack, size_t haystack_length, const void* needle, size_t needle_length)
{
    if(!needle_length) {
        return (void*)haystack;
    }

    if(needle_length > haystack_length) {
        return NULL;
    }

    const unsigned char* bytes = haystack;
    size_t positions = haystack_length - needle_length + 1;
    while(positions) {
        const unsigned char* match = memchr(bytes, *(const unsigned char*)needle, positions);
        if(!match) {
            return NULL;
        }

        if(memcmp(match, needle, needle_length) == 0) {
            return (void*)match;
        }

        size_t consumed = (size_t)(match - bytes) + 1;
        positions -= consumed;
        bytes = match + 1;
    }

    return NULL;
}

int bridge_mkdir(const char* path, mode_t mode)
{
    char buffer[1024];
    const char* resolved = bridge_resolve_path(path, buffer, sizeof(buffer));
    if(!resolved) {
        return -1;
    }

#ifdef __vita__
    char* canonical = __realpath(resolved);
    if(!canonical) {
        return -1;
    }

    int result = sceIoMkdir(canonical, mode & 07777);
    free(canonical);
    if(result < 0) {
        errno = __vita_sce_errno_to_errno(result, 0);
        return -1;
    }

    return 0;

#else
    return mkdir(resolved, mode);

#endif
}

int bridge_lstat(const char* path, struct stat* status)
{
    if(!status) {
        errno = EFAULT;
        return -1;
    }

    char buffer[1024];
    const char* resolved = bridge_resolve_path(path, buffer, sizeof(buffer));
    return resolved ? lstat(resolved, status) : -1;
}

int bridge_fsync(int fd)
{
#ifdef __vita__
    vita_newlib_descriptor_t* descriptor = __vita_fd_grab(fd);
    if(!descriptor) {
        errno = EBADF;
        return -1;
    }

    bool random = descriptor->type == VITAARC_DESCRIPTOR_RANDOM;
    __vita_fd_drop(descriptor);
    if(random) {
        errno = EINVAL;
        return -1;
    }

#endif
    return fsync(fd);
}

#ifdef __vita__
static vita_newlib_descriptor_t* __file_descriptor(int fd)
{
    vita_newlib_descriptor_t* descriptor = __vita_fd_grab(fd);
    if(!descriptor) {
        errno = EBADF;
        return NULL;
    }

    if(descriptor->type != 0 && descriptor->type != VITAARC_DESCRIPTOR_RANDOM) {
        __vita_fd_drop(descriptor);
        errno = EBADF;
        return NULL;
    }

    return descriptor;
}

#endif

int64_t bridge_lseek64(int fd, int64_t offset, int whence)
{
    if(whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
        errno = EINVAL;
        return -1;
    }

#ifdef __vita__
    vita_newlib_descriptor_t* descriptor = __file_descriptor(fd);
    if(!descriptor) {
        return -1;
    }

    if(descriptor->type == VITAARC_DESCRIPTOR_RANDOM) {
        __vita_fd_drop(descriptor);
        return 0;
    }

    int64_t result = sceIoLseek(descriptor->uid, offset, whence);
    __vita_fd_drop(descriptor);
    if(result < 0) {
        errno = __vita_sce_errno_to_errno((int)result, 0);
        return -1;
    }

    return result;

#else
    _Static_assert(sizeof(off_t) >= sizeof(int64_t), "host tests require 64-bit off_t");
    return lseek(fd, offset, whence);

#endif
}

int bridge_ftruncate64(int fd, int64_t length)
{
    if(length < 0) {
        errno = EINVAL;
        return -1;
    }

#ifdef __vita__
    vita_newlib_descriptor_t* descriptor = __file_descriptor(fd);
    if(!descriptor) {
        return -1;
    }

    if(descriptor->type == VITAARC_DESCRIPTOR_RANDOM) {
        __vita_fd_drop(descriptor);
        errno = EINVAL;
        return -1;
    }

    if((__atomic_load_n(&descriptor->flags, __ATOMIC_RELAXED) & O_ACCMODE) == O_RDONLY) {
        __vita_fd_drop(descriptor);
        errno = EBADF;
        return -1;
    }

    SceIoStat status = {0};

    status.st_size = length;
    int result = sceIoChstatByFd(descriptor->uid, &status, SCE_CST_SIZE);
    __vita_fd_drop(descriptor);
    if(result < 0) {
        errno = __vita_sce_errno_to_errno(result, 0);
        return -1;
    }

    return 0;

#else
    return ftruncate(fd, length);

#endif
}

const char* bridge_gai_strerror(int code)
{
    /* Bionic EAI values are positive and differ from host libc values. */
    static const char* messages[] = {"Success",
        "Address family mismatch",
        "Temporary name lookup failure",
        "Invalid address lookup flags",
        "Name lookup failed",
        "Unsupported address family",
        "Out of memory",
        "No address data",
        "Unknown host or service",
        "Unsupported service",
        "Unsupported socket type",
        "System error",
        "Invalid address lookup hints",
        "Unknown protocol",
        "Address buffer too small"};

    if(code < 0 || (size_t)code >= sizeof(messages) / sizeof(messages[0])) {
        return "Unknown address lookup error";
    }

    return messages[code];
}

char* bridge_getcwd(char* buffer, size_t size)
{
    return getcwd(buffer, size);
}

uid_t bridge_geteuid(void)
{
    return geteuid();
}

int bridge_ioctl(int fd, int request, ...)
{
    (void)fd;
    (void)request;
    /* Linux ioctl numbers and payloads are not Vita ioctls. */
    errno = ENOTTY;
    return -1;
}

int bridge_rmdir(const char* path)
{
    char buffer[1024];
    const char* resolved = bridge_resolve_path(path, buffer, sizeof(buffer));
    return resolved ? rmdir(resolved) : -1;
}

typedef struct {
    int android;
    int native;
} sysconf_key_t;

long bridge_sysconf(int name)
{
    /* Bionic query numbers differ from both newlib and host libc. */
    static const sysconf_key_t queries[] = {
        {0x0000, _SC_ARG_MAX},
        {0x0005, _SC_CHILD_MAX},
        {0x0006, _SC_CLK_TCK},
        {0x000b, _SC_OPEN_MAX},
        {0x001b, _SC_STREAM_MAX},
        {0x0027, _SC_PAGESIZE},
        {0x0028, _SC_PAGESIZE},
        {0x0060, _SC_NPROCESSORS_CONF},
        {0x0061, _SC_NPROCESSORS_ONLN},
        {0x0062, _SC_PHYS_PAGES},
    };

    for(size_t index = 0; index < sizeof(queries) / sizeof(queries[0]); ++index) {
        if(name == queries[index].android) {
            return sysconf(queries[index].native);
        }
    }

    errno = EINVAL;
    return -1;
}

intmax_t bridge_strtoimax(const char* text, char** end, int base)
{
    return strtoimax(text, end, base);
}

uintmax_t bridge_strtoumax(const char* text, char** end, int base)
{
    return strtoumax(text, end, base);
}

ssize_t bridge_readlink(const char* path, char* buffer, size_t size)
{
    char path_buffer[1024];
    const char* resolved = bridge_resolve_path(path, path_buffer, sizeof(path_buffer));
    if(!resolved) {
        return -1;
    }

#ifdef __vita__
    (void)buffer;
    (void)size;
    struct stat status;
    if(stat(resolved, &status) < 0) {
        return -1;
    }

    /* Vita filesystems have no POSIX symbolic links. */
    errno = EINVAL;
    return -1;

#else
    return readlink(resolved, buffer, size);

#endif
}

long bridge_syscall(long number, ...)
{
    (void)number;
    errno = ENOSYS;
    return -1;
}

int bridge_utimes(const char* path, const struct timeval times[2])
{
    char buffer[1024];
    const char* resolved = bridge_resolve_path(path, buffer, sizeof(buffer));
    return resolved ? utimes(resolved, times) : -1;
}

int bridge_nanosleep(const struct timespec* request, struct timespec* remaining)
{
    if(!request) {
        errno = EFAULT;
        return -1;
    }

    if(request->tv_sec < 0 || request->tv_nsec < 0 || request->tv_nsec >= 1000000000) {
        errno = EINVAL;
        return -1;
    }

#ifdef __vita__
    struct timespec rest = *request;
    const struct timespec duration = rest;
    uint64_t started = remaining ? sceKernelGetProcessTimeWide() : 0;
    while(rest.tv_sec || rest.tv_nsec) {
        uint32_t seconds = rest.tv_sec > 3600 ? 3600 : (uint32_t)rest.tv_sec;
        uint32_t nanoseconds = seconds == rest.tv_sec ? (uint32_t)rest.tv_nsec : 0;
        uint32_t microseconds = seconds * 1000000u + (nanoseconds + 999) / 1000;
        int result = sceKernelDelayThread(microseconds);
        if(result < 0) {
            if(remaining) {
                rest = duration;
                uint64_t ended = sceKernelGetProcessTimeWide();
                uint64_t elapsed = ended >= started ? ended - started : 0;
                uint64_t elapsed_seconds = elapsed / 1000000;
                long elapsed_nanoseconds = (long)(elapsed % 1000000) * 1000;
                // Subtract from the outstanding request without multiplying its seconds
                // into a potentially overflowing nanosecond or microsecond total.
                if(elapsed_seconds > (uint64_t)rest.tv_sec ||
                    (elapsed_seconds == (uint64_t)rest.tv_sec && elapsed_nanoseconds >= rest.tv_nsec)) {
                    rest.tv_sec = 0;
                    rest.tv_nsec = 0;
                } else {
                    rest.tv_sec -= (time_t)elapsed_seconds;
                    if(rest.tv_nsec < elapsed_nanoseconds) {
                        --rest.tv_sec;
                        rest.tv_nsec += 1000000000;
                    }

                    rest.tv_nsec -= elapsed_nanoseconds;
                }

                *remaining = rest;
            }

            errno = __vita_sce_errno_to_errno(result, 0);
            return -1;
        }

        rest.tv_sec -= seconds;
        if(nanoseconds) {
            rest.tv_nsec = 0;
        }
    }

    return 0;

#else
    return nanosleep(request, remaining);

#endif
}

int bridge_fchmod(int fd, mode_t mode)
{
#ifdef __vita__
    vita_newlib_descriptor_t* descriptor = __vita_fd_grab(fd);
    if(!descriptor) {
        errno = EBADF;
        return -1;
    }

    bool random = descriptor->type == VITAARC_DESCRIPTOR_RANDOM;
    __vita_fd_drop(descriptor);
    if(random) {
        errno = ENOTSUP;
        return -1;
    }

#endif
    return fchmod(fd, mode);
}

int bridge_fchown(int fd, uid_t owner, gid_t group)
{
#ifdef __vita__
    (void)owner;
    (void)group;
    vita_newlib_descriptor_t* descriptor = __vita_fd_grab(fd);
    if(!descriptor) {
        errno = EBADF;
        return -1;
    }

    __vita_fd_drop(descriptor);
    errno = ENOTSUP;
    return -1;

#else
    return fchown(fd, owner, group);

#endif
}

int bridge_clock_gettime(clockid_t clock_id, struct timespec* time)
{
    if(!time) {
        errno = EFAULT;
        return -1;
    }

    switch(clock_id) {
        case 0:   /* Android CLOCK_REALTIME */
        case 5: { /* Android CLOCK_REALTIME_COARSE */
            struct timeval value;
            if(gettimeofday(&value, NULL) < 0) {
                return -1;
            }

            time->tv_sec = value.tv_sec;
            time->tv_nsec = value.tv_usec * 1000;
            return 0;
        }

        case 1: /* Android CLOCK_MONOTONIC */
        case 4: /* Android CLOCK_MONOTONIC_RAW */
        case 6: /* Android CLOCK_MONOTONIC_COARSE */
#ifdef __vita__
        {
            uint64_t value = sceKernelGetProcessTimeWide();
            time->tv_sec = value / 1000000;
            time->tv_nsec = (value % 1000000) * 1000;
            return 0;
        }

#else
            return clock_gettime(CLOCK_MONOTONIC, time);

#endif
        default:
            errno = EINVAL;
            return -1;
    }
}
