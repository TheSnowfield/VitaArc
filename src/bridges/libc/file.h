#ifndef __BRIDGES_LIBC_FILE_H
#define __BRIDGES_LIBC_FILE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

/* Android's ARM32 stat/stat64 layout, independent of host/newlib typedefs. */
typedef struct bridge_bionic_stat {
    uint64_t device;
    uint32_t pad_device;
    uint32_t legacy_inode;
    uint32_t mode;
    uint32_t links;
    uint32_t uid;
    uint32_t gid;
    uint64_t special_device;
    uint32_t pad_special_device;
    uint32_t align_size;
    int64_t size;
    uint32_t block_size;
    uint32_t align_blocks;
    uint64_t blocks;
    uint32_t access_seconds;
    uint32_t access_nanoseconds;
    uint32_t modify_seconds;
    uint32_t modify_nanoseconds;
    uint32_t change_seconds;
    uint32_t change_nanoseconds;
    uint64_t inode;
} __attribute__((aligned(8))) bridge_bionic_stat_t;

typedef struct bridge_io_vector {
    const void* base;
    size_t length;
} bridge_io_vector_t;

enum {
    BIONIC_O_RDONLY = 0,
    BIONIC_O_WRONLY = 1,
    BIONIC_O_RDWR = 2,
    BIONIC_O_CREAT = 00000100,
    BIONIC_O_EXCL = 00000200,
    BIONIC_O_NOCTTY = 00000400,
    BIONIC_O_TRUNC = 00001000,
    BIONIC_O_APPEND = 00002000,
    BIONIC_O_NONBLOCK = 00004000,
    BIONIC_O_DSYNC = 00010000,
    BIONIC_O_DIRECTORY = 00040000,
    BIONIC_O_NOFOLLOW = 00100000,
    BIONIC_O_DIRECT = 00200000,
    BIONIC_O_LARGEFILE = 00400000,
    BIONIC_O_CLOEXEC = 02000000,
    BIONIC_O_SYNC = 04010000,
};

int bridge_open(const char* path, int flags, ...);
int bridge_open_2(const char* path, int flags);
ssize_t bridge_read(int fd, void* buffer, size_t size);
ssize_t bridge_write(int fd, const void* buffer, size_t size);
int bridge_writev(int fd, const void* vectors, int count);
int bridge_close(int fd);
FILE* bridge_fopen(const char* path, const char* mode);
int bridge_fclose(FILE* stream);
FILE* bridge_fdopen(int fd, const char* mode);
int bridge_fcntl(int fd, int command, ...);
int32_t bridge_lseek(int fd, int32_t offset, int whence);
int bridge_ftruncate(int fd, int32_t length);
int bridge_bionic_stat(const char* path, bridge_bionic_stat_t* status);
int bridge_bionic_lstat(const char* path, bridge_bionic_stat_t* status);
int bridge_bionic_fstat(int fd, bridge_bionic_stat_t* status);
int bridge_access(const char* path, int mode);
int bridge_unlink(const char* path);
int bridge_remove(const char* path);
int bridge_rename(const char* old_path, const char* new_path);

#endif
