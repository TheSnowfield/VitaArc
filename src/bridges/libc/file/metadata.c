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

_Static_assert(sizeof(bridge_bionic_stat_t) == 104, "Android ARM stat size");
_Static_assert(offsetof(bridge_bionic_stat_t, mode) == 16, "Android ARM stat mode");
_Static_assert(offsetof(bridge_bionic_stat_t, size) == 48, "Android ARM stat file size");
_Static_assert(offsetof(bridge_bionic_stat_t, blocks) == 64, "Android ARM stat blocks");
_Static_assert(offsetof(bridge_bionic_stat_t, inode) == 96, "Android ARM stat inode");
#ifndef __vita__
static int __host_stat(const struct stat* native, bridge_bionic_stat_t* output)
{
    if(native->st_atime < INT32_MIN || native->st_atime > UINT32_MAX || native->st_mtime < INT32_MIN ||
        native->st_mtime > UINT32_MAX || native->st_ctime < INT32_MIN || native->st_ctime > UINT32_MAX) {
        errno = EOVERFLOW;
        return -1;
    }

    bridge_bionic_stat_t status = {0};

    status.device = native->st_dev;
    status.inode = native->st_ino;
    status.legacy_inode = (uint32_t)status.inode;
    status.mode = native->st_mode;
    status.links = native->st_nlink;
    status.uid = native->st_uid;
    status.gid = native->st_gid;
    status.special_device = native->st_rdev;
    status.size = native->st_size;
    status.block_size = native->st_blksize;
    status.blocks = native->st_blocks;
    status.access_seconds = native->st_atime;
    status.modify_seconds = native->st_mtime;
    status.change_seconds = native->st_ctime;
#ifndef __vita__
    status.access_nanoseconds = native->st_atim.tv_nsec;
    status.modify_nanoseconds = native->st_mtim.tv_nsec;
    status.change_nanoseconds = native->st_ctim.tv_nsec;
#endif
    *output = status;
    return 0;
}

#endif

#ifdef __vita__
static uint64_t __path_identity(const char* path, bool drive_only)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    if(path) {
        while(*path) {
            unsigned char byte = (unsigned char)*path++;
            hash = (hash ^ byte) * UINT64_C(1099511628211);
            if(drive_only && byte == ':') {
                break;
            }
        }
    }

    return hash;
}

static int __timestamp(const SceDateTime* time, uint32_t* seconds, uint32_t* nanoseconds)
{
    if(!time->year) {
        return 0;
    }

    SceUInt64 value;
    int result = sceRtcGetTime64_t(time, &value);
    if(result < 0) {
        return __native_error(result);
    }

    if((value > UINT32_MAX && value < UINT64_MAX - INT32_MAX) || time->microsecond >= 1000000) {
        errno = EOVERFLOW;
        return -1;
    }

    *seconds = (uint32_t)value;
    *nanoseconds = time->microsecond * 1000;
    return 0;
}

static int __vita_stat(const SceIoStat* native, const char* path, bridge_bionic_stat_t* output)
{
    if(native->st_size < 0) {
        errno = EIO;
        return -1;
    }

    bridge_bionic_stat_t status = {0};

    status.device = __path_identity(path, true);
    status.inode = __path_identity(path, false);
    status.legacy_inode = (uint32_t)status.inode;
    status.mode = native->st_mode & 0777;
    if(SCE_S_ISREG(native->st_mode)) {
        status.mode |= 0100000;
    }

    if(SCE_S_ISDIR(native->st_mode)) {
        status.mode |= 0040000;
    }

    if(SCE_S_ISLNK(native->st_mode)) {
        status.mode |= 0120000;
    }

    status.links = 1;
    status.size = native->st_size;
    status.block_size = 64 * 1024;
    status.blocks = (uint64_t)native->st_size / 512 + (native->st_size % 512 != 0);
    if(__timestamp(&native->st_atime, &status.access_seconds, &status.access_nanoseconds) < 0 ||
        __timestamp(&native->st_mtime, &status.modify_seconds, &status.modify_nanoseconds) < 0 ||
        __timestamp(&native->st_ctime, &status.change_seconds, &status.change_nanoseconds) < 0) {
        return -1;
    }

    *output = status;
    return 0;
}

static void __random_stat(bridge_bionic_stat_t* status)
{
    *status = (bridge_bionic_stat_t){
        .mode = 0020444, .links = 1, .special_device = 0x109, .block_size = 4096, .inode = 9, .legacy_inode = 9};
}

#endif

static int __path_stat(const char* path, bridge_bionic_stat_t* status, bool follow)
{
    if(!status) {
        errno = EFAULT;
        return -1;
    }

    char buffer[1024];
    const char* resolved = bridge_resolve_path(path, buffer, sizeof(buffer));
    if(!resolved) {
        return -1;
    }

#ifdef __vita__
    if(strcmp(resolved, "/dev/urandom") == 0) {
        __random_stat(status);
        return 0;
    }

    char* canonical = __realpath(resolved);
    if(!canonical) {
        return -1;
    }

    SceIoStat native;
    int result = sceIoGetstat(canonical, &native);
    if(result >= 0 && follow && SCE_S_ISLNK(native.st_mode)) {
        free(canonical);
        errno = ENOTSUP;
        return -1;
    }

    if(result >= 0) {
        result = __vita_stat(&native, canonical, status);
    } else {
        result = __native_error(result);
    }

    free(canonical);
    return result;

#else
    struct stat native;
    int result = follow ? stat(resolved, &native) : lstat(resolved, &native);
    return result < 0 ? -1 : __host_stat(&native, status);

#endif
}

int bridge_bionic_stat(const char* path, bridge_bionic_stat_t* status)
{
    return __path_stat(path, status, true);
}

int bridge_bionic_lstat(const char* path, bridge_bionic_stat_t* status)
{
    return __path_stat(path, status, false);
}

int bridge_bionic_fstat(int fd, bridge_bionic_stat_t* status)
{
    if(!status) {
        errno = EFAULT;
        return -1;
    }

#ifdef __vita__
    vita_newlib_descriptor_t* descriptor = __vita_fd_grab(fd);
    if(!descriptor) {
        errno = EBADF;
        return -1;
    }

    int result;
    if(__random_descriptor(descriptor)) {
        __random_stat(status);
        result = 0;
    } else if(descriptor->type == 0 || descriptor->type == 1) {
        SceIoStat native;
        result = sceIoGetstatByFd(descriptor->uid, &native);
        result = result < 0 ? __native_error(result) : __vita_stat(&native, descriptor->path, status);
    } else {
        bridge_bionic_stat_t special = {.links = 1, .inode = (uint32_t)descriptor->uid, .block_size = 4096};

        result = 0;
        switch(descriptor->type) {
            case 2:
                special.mode = 0140600;
                break;

            case 3:
                special.mode = 0020666;
                break;

            case 4:
                special.mode = 0010600;
                break;

            default:
                errno = EBADF;
                result = -1;
                break;
        }

        special.legacy_inode = (uint32_t)special.inode;
        if(result == 0) {
            *status = special;
        }
    }

    __vita_fd_drop(descriptor);
    return result;

#else
    struct stat native;
    return fstat(fd, &native) < 0 ? -1 : __host_stat(&native, status);

#endif
}
