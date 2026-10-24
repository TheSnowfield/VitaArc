#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libc/file.h>
#include <libc/posix.h>
#include <platform/newlib.h>
#include <psp2/io/stat.h>
#include <psp2/io/dirent.h>
#include <config.h>

typedef struct mock_file {
    char path[1024];
    bool exists;
    bool nonempty;
    SceIoStat status;
    char bytes[64];
} mock_file_t;

static mock_file_t __files[16] = {{.path = PATH_TO_DATA "file"},
    {.path = PATH_TO_DATA "other"},
    {.path = PATH_TO_DATA "directory", .exists = true, .status = {.st_mode = SCE_S_IFDIR | 0700}},
    {.path = PATH_TO_DATA "link", .exists = true, .status = {.st_mode = SCE_S_IFLNK | 0700}}};

typedef struct mock_open {
    mock_file_t* file;
    int64_t offset;
} mock_open_t;

vita_newlib_descriptor_t* __vita_fdmap[256];
static vita_newlib_descriptor_t __pool[256];
static mock_open_t __opened[64];
static unsigned __next_uid = 10, __native_closes, __syncs, __rng_calls;
static unsigned __rng_failure;
static bool __no_descriptors;
static unsigned __creation_mode;
static int __creation_flags;
static bool __sync_failure;
static unsigned __rename_calls, __rename_failure_call, __backup_collisions;
static int __rename_failure_error;
static bool __rollback_failure, __cleanup_failure, __atomic_replace;
static int __directory_failure;
static unsigned __warnings, __errors;

void log_w(const char* tag, const char* format, ...)
{
    (void)tag;
    (void)format;
    ++__warnings;
}

void log_e(const char* tag, const char* format, ...)
{
    (void)tag;
    (void)format;
    ++__errors;
}

static int __error(int error)
{
    return -1000 - error;
}

static mock_file_t* __lookup(const char* path)
{
    mock_file_t* unused = NULL;
    for(size_t index = 0; index < sizeof(__files) / sizeof(__files[0]); ++index) {
        if(strcmp(path, __files[index].path) == 0) {
            if(__files[index].exists) {
                return &__files[index];
            }

            unused = &__files[index];
        }
    }

    return unused;
}

static mock_file_t* __claim(const char* path)
{
    mock_file_t* file = __lookup(path);
    if(file) {
        return file;
    }

    for(size_t index = 0; index < sizeof(__files) / sizeof(__files[0]); ++index) {
        if(!__files[index].exists) {
            snprintf(__files[index].path, sizeof(__files[index].path), "%s", path);
            return &__files[index];
        }
    }

    return NULL;
}

char* __realpath(const char* path)
{
    return strdup(path);
}

int _fcntl2sony(int flags)
{
    /* The host boundary fixture checks translated native flags. The linked ARM
     * fixture separately executes VitaSDK's actual native-to-Sony converter. */
    return flags;
}

int __vita_acquire_descriptor(void)
{
    if(__no_descriptors) {
        return -1;
    }

    for(int fd = 3; fd < 256; ++fd) {
        if(__vita_fdmap[fd]) {
            continue;
        }

        for(int index = 3; index < 256; ++index) {
            if(!__pool[index].references) {
                __pool[index] = (vita_newlib_descriptor_t){.references = 1};

                __vita_fdmap[fd] = &__pool[index];
                return fd;
            }
        }
    }

    return -1;
}

vita_newlib_descriptor_t* __vita_fd_grab(int fd)
{
    if(fd < 0 || fd >= 256 || !__vita_fdmap[fd]) {
        return NULL;
    }

    ++__vita_fdmap[fd]->references;
    return __vita_fdmap[fd];
}

int sceIoClose(int uid)
{
    assert(uid >= 10 && uid < (int)__next_uid && __opened[uid].file);
    __opened[uid].file = NULL;
    ++__native_closes;
    return 0;
}

int sceIoDclose(int uid)
{
    return sceIoClose(uid);
}

int __vita_fd_drop(vita_newlib_descriptor_t* descriptor)
{
    assert(descriptor && descriptor->references > 0);
    if(--descriptor->references) {
        return 0;
    }

    if(descriptor->type == 0 || descriptor->type == 1) {
        sceIoClose(descriptor->uid);
        free(descriptor->path);
    } else {
        assert(descriptor->type == VITAARC_DESCRIPTOR_RANDOM);
    }

    memset(descriptor, 0, sizeof(*descriptor));
    return 0;
}

int __vita_release_descriptor(int fd)
{
    if(fd < 0 || fd >= 256 || !__vita_fdmap[fd]) {
        errno = EBADF;
        return -1;
    }

    vita_newlib_descriptor_t* descriptor = __vita_fdmap[fd];
    __vita_fdmap[fd] = NULL;
    return __vita_fd_drop(descriptor);
}

int __wrap_close(int fd)
{
    return __vita_release_descriptor(fd);
}

int __vita_sce_errno_to_errno(int error, int type)
{
    assert(type == 0 && error < -1000);
    return -error - 1000;
}

int sceIoGetstat(const char* path, SceIoStat* status)
{
    mock_file_t* file = __lookup(path);
    if(!file || !file->exists) {
        return __error(ENOENT);
    }

    *status = file->status;
    return 0;
}

int sceIoGetstatByFd(int uid, SceIoStat* status)
{
    assert(uid >= 10 && uid < (int)__next_uid && __opened[uid].file);
    *status = __opened[uid].file->status;
    return 0;
}

int sceIoOpen(const char* path, int flags, unsigned int mode)
{
    mock_file_t* file = __lookup(path);
    if(!file || (!file->exists && !(flags & O_CREAT))) {
        return __error(ENOENT);
    }

    if(file->exists && (flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)) {
        return __error(EEXIST);
    }

    if(!file->exists) {
        file->exists = true;
        file->status.st_mode = SCE_S_IFREG | mode;
        file->status.st_atime = (SceDateTime){.year = 2026, .second = 1, .microsecond = 123456};

        file->status.st_mtime = (SceDateTime){.year = 2026, .second = 2, .microsecond = 654321};

        file->status.st_ctime = (SceDateTime){.year = 2026, .second = 3};
    }

    if(flags & O_TRUNC) {
        file->status.st_size = 0;
    }

    __creation_mode = mode;
    __creation_flags = flags;
    assert(__next_uid < 64);
    unsigned uid = __next_uid++;
    __opened[uid] = (mock_open_t){.file = file};

    return (int)uid;
}

int sceIoDopen(const char* path)
{
    return sceIoOpen(path, O_RDONLY, 0);
}

int sceIoDread(int uid, SceIoDirent* entry)
{
    mock_open_t* opened = &__opened[uid];
    assert(opened->file && SCE_S_ISDIR(opened->file->status.st_mode));
    if(__directory_failure) {
        return __error(__directory_failure);
    }

    if(opened->offset < 2) {
        strcpy(entry->d_name, opened->offset++ ? ".." : ".");
        return 1;
    }

    strcpy(entry->d_name, "child");
    return opened->file->nonempty ? 1 : 0;
}

int sceIoMkdir(const char* path, unsigned int mode)
{
    mock_file_t* file = __claim(path);
    if(!file) {
        return __error(ENOSPC);
    }

    if(file->exists) {
        return __error(EEXIST);
    }

    file->exists = true;
    file->status = (SceIoStat){.st_mode = SCE_S_IFDIR | mode};

    return 0;
}

int sceIoRename(const char* old_path, const char* new_path)
{
    ++__rename_calls;
    if(__rename_calls == __rename_failure_call) {
        return __error(__rename_failure_error);
    }

    bool backup_source = strstr(old_path, ".vitaarc-rename-") != NULL;
    bool backup_target = strstr(new_path, ".vitaarc-rename-") != NULL;
    if(backup_source && __rollback_failure) {
        return __error(EACCES);
    }

    if(backup_target && __backup_collisions) {
        --__backup_collisions;
        return __error(EEXIST);
    }

    mock_file_t *source = __lookup(old_path), *destination = __lookup(new_path);
    if(!source || !source->exists) {
        return __error(ENOENT);
    }

    if(destination && destination->exists) {
        if(!__atomic_replace) {
            return __error(EEXIST);
        }

        destination->exists = false;
    }

    snprintf(source->path, sizeof(source->path), "%s", new_path);
    return 0;
}

int sceIoRemove(const char* path)
{
    mock_file_t* file = __lookup(path);
    if(__cleanup_failure && strstr(path, ".vitaarc-rename-")) {
        return __error(EACCES);
    }

    if(!file || !file->exists) {
        return __error(ENOENT);
    }

    if(SCE_S_ISDIR(file->status.st_mode)) {
        return __error(EISDIR);
    }

    file->exists = false;
    return 0;
}

int sceIoRmdir(const char* path)
{
    mock_file_t* file = __lookup(path);
    if(__cleanup_failure && strstr(path, ".vitaarc-rename-")) {
        return __error(EACCES);
    }

    if(!file || !file->exists) {
        return __error(ENOENT);
    }

    if(!SCE_S_ISDIR(file->status.st_mode)) {
        return __error(ENOTDIR);
    }

    if(file->nonempty) {
        return __error(ENOTEMPTY);
    }

    file->exists = false;
    return 0;
}

int64_t sceIoLseek(int uid, int64_t offset, int whence)
{
    mock_open_t* opened = &__opened[uid];
    assert(opened->file);
    if(whence == SEEK_CUR) {
        offset += opened->offset;
    }

    if(whence == SEEK_END) {
        offset += opened->file->status.st_size;
    }

    if(offset < 0) {
        return __error(EINVAL);
    }

    return opened->offset = offset;
}

int sceIoChstatByFd(int uid, const SceIoStat* status, unsigned int fields)
{
    assert(fields == SCE_CST_SIZE && __opened[uid].file);
    __opened[uid].file->status.st_size = status->st_size;
    return 0;
}

int sceRtcGetTime64_t(const SceDateTime* time, SceUInt64* seconds)
{
    *seconds = time->year == 2200 ? UINT64_C(5000000000) : UINT64_C(1700000000) + time->second;
    return 0;
}

ssize_t __wrap_write(int fd, const void* buffer, size_t size)
{
    vita_newlib_descriptor_t* descriptor = __vita_fd_grab(fd);
    if(!descriptor || (descriptor->flags & O_ACCMODE) == O_RDONLY) {
        if(descriptor) {
            __vita_fd_drop(descriptor);
        }

        errno = EBADF;
        return -1;
    }

    assert(descriptor->type == 0);
    mock_open_t* opened = &__opened[descriptor->uid];
    if(descriptor->flags & O_APPEND) {
        opened->offset = opened->file->status.st_size;
    }

    assert(opened->offset >= 0 && opened->offset + (int64_t)size <= 64);
    memcpy(opened->file->bytes + opened->offset, buffer, size);
    opened->offset += size;
    if(opened->offset > opened->file->status.st_size) {
        opened->file->status.st_size = opened->offset;
    }

    __vita_fd_drop(descriptor);
    return (ssize_t)size;
}

ssize_t __wrap_read(int fd, void* buffer, size_t size)
{
    vita_newlib_descriptor_t* descriptor = __vita_fd_grab(fd);
    if(!descriptor || (descriptor->flags & O_ACCMODE) == O_WRONLY) {
        if(descriptor) {
            __vita_fd_drop(descriptor);
        }

        errno = EBADF;
        return -1;
    }

    assert(descriptor->type == 0);
    mock_open_t* opened = &__opened[descriptor->uid];
    int64_t remaining = opened->file->status.st_size - opened->offset;
    if(remaining < (int64_t)size) {
        size = remaining > 0 ? (size_t)remaining : 0;
    }

    assert(opened->offset >= 0 && opened->offset + (int64_t)size <= 64);
    memcpy(buffer, opened->file->bytes + opened->offset, size);
    opened->offset += size;
    __vita_fd_drop(descriptor);
    return (ssize_t)size;
}

int __wrap_fcntl(int fd, int command, ...)
{
    if(fd < 0 || fd >= 256 || !__vita_fdmap[fd]) {
        errno = EBADF;
        return -1;
    }

    if(command == F_GETFD || command == F_SETFD) {
        return 0;
    }

    assert(command == F_DUPFD);
    va_list args;
    va_start(args, command);
    int minimum = va_arg(args, int);
    va_end(args);
    for(int next = minimum > 3 ? minimum : 3; next < 256; ++next) {
        if(!__vita_fdmap[next]) {
            __vita_fdmap[next] = __vita_fdmap[fd];
            ++__vita_fdmap[next]->references;
            return next;
        }
    }

    errno = EMFILE;
    return -1;
}

int __wrap_fsync(int fd)
{
    assert(fd >= 3 && __vita_fdmap[fd] && __vita_fdmap[fd]->type == 0);
    ++__syncs;
    return 0;
}

int sceIoSyncByFd(int uid, unsigned int flags)
{
    assert(uid >= 10 && __opened[uid].file && !flags);
    ++__syncs;
    return __sync_failure ? __error(EIO) : 0;
}

int sceIoRead(int uid, void* buffer, unsigned int size)
{
    mock_open_t* opened = &__opened[uid];
    assert(opened->file);
    int64_t remaining = opened->file->status.st_size - opened->offset;
    if(remaining < size) {
        size = remaining > 0 ? (unsigned int)remaining : 0;
    }

    assert(opened->offset >= 0 && opened->offset + size <= 64);
    memcpy(buffer, opened->file->bytes + opened->offset, size);
    opened->offset += size;
    return (int)size;
}

int sceIoWrite(int uid, const void* buffer, unsigned int size)
{
    mock_open_t* opened = &__opened[uid];
    assert(opened->file && opened->offset >= 0 && opened->offset + size <= 64);
    memcpy(opened->file->bytes + opened->offset, buffer, size);
    opened->offset += size;
    if(opened->offset > opened->file->status.st_size) {
        opened->file->status.st_size = opened->offset;
    }

    return (int)size;
}

int sceKernelGetRandomNumber(void* output, unsigned int size)
{
    assert(size > 0 && size <= 64);
    if(++__rng_calls == __rng_failure) {
        return __error(EIO);
    }

    memset(output, (int)__rng_calls, size);
    return 0;
}

uint64_t sceKernelGetProcessTimeWide(void)
{
    return 0;
}

int sceKernelDelayThread(unsigned int delay)
{
    (void)delay;
    return 0;
}

static void __test_files(void)
{
    int fd = bridge_open(
        "/data/data/moe.low.arc/file", BIONIC_O_CREAT | BIONIC_O_EXCL | BIONIC_O_RDWR | BIONIC_O_CLOEXEC, 0640u);
    assert(
        fd >= 3 && __creation_mode == 0646 && (__creation_flags & O_CREAT) && (__creation_flags & O_ACCMODE) == O_RDWR);
    assert(bridge_fcntl(fd, 1) == 1);
    int duplicate = bridge_fcntl(fd, 0, 0);
    int protected_duplicate = bridge_fcntl(fd, 1030, 0);
    assert(duplicate >= 3 && bridge_fcntl(duplicate, 1) == 0);
    assert(protected_duplicate >= 3 && bridge_fcntl(protected_duplicate, 1) == 1);
    assert(bridge_fcntl(duplicate, 2, 1) == 0 && bridge_fcntl(duplicate, 1) == 1);
    assert(bridge_fcntl(duplicate, 2, 0) == 0 && bridge_fcntl(duplicate, 1) == 0 && bridge_fcntl(fd, 1) == 1);
    assert(bridge_fcntl(duplicate, 2, 2) == -1 && errno == EINVAL);
    assert(bridge_close(duplicate) == 0 && bridge_close(protected_duplicate) == 0);
    assert(bridge_open("/data/data/moe.low.arc/file", BIONIC_O_CREAT | BIONIC_O_EXCL | BIONIC_O_RDWR, 0700u) == -1 &&
        errno == EEXIST);
    const bridge_io_vector_t vectors[] = {{"abc", 3}, {"def", 3}};

    assert(bridge_writev(fd, vectors, 2) == 6);
    const bridge_io_vector_t excessive[] = {{"x", INT32_MAX}, {"y", 1}};

    assert(bridge_writev(fd, excessive, 2) == -1 && errno == EINVAL);
    assert(bridge_lseek64(fd, 1, SEEK_SET) == 1);
    char bytes[4] = {0};

    assert(bridge_read(fd, bytes, 3) == 3 && strcmp(bytes, "bcd") == 0);
    bridge_bionic_stat_t status, named;
    assert(bridge_bionic_fstat(fd, &status) == 0 && status.size == 6 && status.mode == 0100646);
    assert(status.modify_seconds == 1700000002 && status.modify_nanoseconds == 654321000);
    assert(bridge_bionic_stat("/data/user/0/moe.low.arc/file", &named) == 0 && named.inode == status.inode);
    assert(bridge_fcntl(fd, 4, BIONIC_O_APPEND) == 0);
    assert(bridge_lseek64(fd, 0, SEEK_SET) == 0);
    assert(bridge_write(fd, "!", 1) == 1 && __files[0].status.st_size == 7 && __files[0].bytes[6] == '!');
    int synchronous = bridge_open("/data/data/moe.low.arc/file", BIONIC_O_WRONLY | BIONIC_O_SYNC);
    assert(synchronous >= 3 && bridge_fdopen(synchronous, "w") == NULL && errno == ENOTSUP);
    assert(bridge_write(synchronous, "q", 1) == 1 && __syncs == 1);
    __sync_failure = true;
    assert(bridge_write(synchronous, "r", 1) == -1 && errno == EIO && __syncs == 2);
    __sync_failure = false;
    assert(bridge_close(synchronous) == 0);
    int64_t length = INT64_C(5) * 1024 * 1024 * 1024 + 7;
    assert(bridge_ftruncate64(fd, length) == 0);
    assert(bridge_bionic_fstat(fd, &status) == 0 && status.size == length &&
        status.blocks == (uint64_t)(length / 512 + 1));
    assert(bridge_fcntl(fd, 4, BIONIC_O_RDWR | BIONIC_O_APPEND | BIONIC_O_NONBLOCK) == 0);
    assert(bridge_fcntl(fd, 3) & BIONIC_O_APPEND);
    assert(bridge_fcntl(fd, 3) & BIONIC_O_NONBLOCK);
    assert(bridge_fcntl(fd, 2, 0) == 0 && bridge_fcntl(fd, 1) == 0);
    assert(bridge_fcntl(fd, 5, NULL) == -1 && errno == ENOTSUP);
    assert(bridge_fsync(fd) == 0 && __syncs == 3);
    assert(bridge_close(fd) == 0);
    assert(bridge_bionic_fstat(fd, &status) == -1 && errno == EBADF);
    unsigned closes = __native_closes;
    __no_descriptors = true;
    assert(bridge_open("/data/data/moe.low.arc/file", BIONIC_O_RDONLY) == -1 && errno == EMFILE);
    assert(__native_closes == closes + 1);
    assert(bridge_open("/dev/urandom", BIONIC_O_RDONLY) == -1 && errno == EMFILE);
    __no_descriptors = false;
    assert(
        bridge_open("/data/data/moe.low.arc/missing", BIONIC_O_RDONLY | BIONIC_O_DIRECTORY) == -1 && errno == ENOENT);
    assert(bridge_open("/data/data/moe.low.arc/file", BIONIC_O_DIRECTORY) == -1 && errno == ENOTDIR);
    assert(bridge_open("/data/data/moe.low.arc/directory", BIONIC_O_WRONLY) == -1 && errno == EISDIR);
    int directory = bridge_open("/data/data/moe.low.arc/directory", BIONIC_O_DIRECTORY);
    assert(directory >= 3 && bridge_bionic_fstat(directory, &status) == 0 && (status.mode & 0170000) == 0040000);
    assert(bridge_read(directory, bytes, 1) == -1 && errno == EISDIR);
    assert(bridge_close(directory) == 0);
    assert(bridge_open("/data/data/moe.low.arc/link", BIONIC_O_NOFOLLOW) == -1 && errno == ELOOP);
    assert(bridge_bionic_lstat("/data/data/moe.low.arc/link", &status) == 0 && (status.mode & 0170000) == 0120000);
    assert(bridge_bionic_stat("/data/data/moe.low.arc/link", &status) == -1 && errno == ENOTSUP);
    __files[0].status.st_mtime.year = 2200;
    memset(&status, 0xa5, sizeof(status));
    named = status;
    assert(bridge_bionic_stat("/data/data/moe.low.arc/file", &status) == -1 && errno == EOVERFLOW &&
        memcmp(&status, &named, sizeof(status)) == 0);
    __files[0].status.st_mtime.year = 2026;
    fd = bridge_open("/data/data/moe.low.arc/file", BIONIC_O_CLOEXEC);
    assert(fd >= 3 && bridge_fcntl(fd, 1) == 1 && bridge_close(fd) == 0);
    int reused = bridge_open("/data/data/moe.low.arc/file", BIONIC_O_RDONLY);
    assert(reused == fd && bridge_fcntl(reused, 1) == 0 && bridge_close(reused) == 0);
}

static void __test_random(void)
{
    int first = bridge_open("/dev/urandom", BIONIC_O_RDONLY | BIONIC_O_CLOEXEC);
    int second = bridge_open("/dev/urandom", BIONIC_O_RDONLY);
    assert(first >= 3 && second >= 3 && first != second && __vita_fdmap[1]->uid == 777);
    unsigned closes = __native_closes;
    unsigned char bytes[257];
    assert(bridge_read(first, bytes, sizeof(bytes)) == sizeof(bytes) && __rng_calls == 5);
    assert(bytes[0] == 1 && bytes[256] == 5);
    __rng_failure = __rng_calls + 2;
    assert(bridge_read(first, bytes, sizeof(bytes)) == 64);
    __rng_failure = __rng_calls + 1;
    assert(bridge_read(first, bytes, 1) == -1 && errno == EIO);
    __rng_failure = 0;
    assert(bridge_read(first, NULL, 0) == 0);
    assert(bridge_lseek64(first, 15, SEEK_SET) == 0);
    assert(bridge_ftruncate64(first, 0) == -1 && errno == EINVAL);
    assert(bridge_fsync(first) == -1 && errno == EINVAL && __syncs == 3);
    assert(bridge_fchmod(first, 0777) == -1 && errno == ENOTSUP);
    assert(bridge_fchown(first, 0, 0) == -1 && errno == ENOTSUP);
    assert(bridge_fdopen(first, "r") == NULL && errno == ENOTSUP);
    assert(bridge_write(first, bytes, 1) == -1 && errno == EBADF);
    bridge_io_vector_t vector = {bytes, 1};

    assert(bridge_writev(first, &vector, 1) == -1 && errno == EBADF);
    bridge_bionic_stat_t status, named;
    assert(bridge_bionic_fstat(first, &status) == 0 && status.mode == 0020444 && status.special_device == 0x109);
    assert(bridge_bionic_stat("/dev/urandom", &named) == 0 && memcmp(&status, &named, sizeof(status)) == 0);
    int duplicate = bridge_fcntl(first, 0, 0);
    assert(duplicate >= 3 && duplicate != first && duplicate != second);
    assert(bridge_fcntl(first, 1) == 1 && bridge_fcntl(duplicate, 1) == 0);
    assert(bridge_fcntl(duplicate, 4, BIONIC_O_NONBLOCK) == 0 && (bridge_fcntl(first, 3) & BIONIC_O_NONBLOCK));
    assert(bridge_close(first) == 0 && bridge_read(first, bytes, 1) == -1 && errno == EBADF);
    assert(bridge_read(duplicate, bytes, 1) == 1);
    assert(bridge_close(duplicate) == 0 && bridge_close(second) == 0 && __native_closes == closes);
    assert(__vita_fdmap[1] && __vita_fdmap[1]->uid == 777);
    assert(bridge_open("/dev/urandom", BIONIC_O_DIRECTORY) == -1 && errno == ENOTDIR);
    assert(bridge_open("/dev/urandom", BIONIC_O_CREAT | BIONIC_O_EXCL, 0600u) == -1 && errno == EEXIST);
    FILE* stream = bridge_fopen("/dev/urandom", "rb");
    assert(stream && setvbuf(stream, NULL, _IONBF, 0) == 0);
    unsigned calls = __rng_calls;
    assert(fread(bytes, 1, 4, stream) == 4 && __rng_calls > calls);
    assert(!feof(stream) && !ferror(stream));
    __rng_failure = __rng_calls + 1;
    assert(fread(bytes, 1, 1, stream) == 0 && ferror(stream) && errno == EIO);
    __rng_failure = 0;
    clearerr(stream);
    assert(fread(bytes, 1, 1, stream) == 1 && !ferror(stream));
    assert(bridge_fclose(stream) == 0 && __native_closes == closes);
    assert(bridge_fopen("/dev/urandom", "r+") == NULL && errno == ENOTSUP);
    assert(bridge_fopen("/dev/urandom", "w") == NULL && errno == ENOTSUP);
}

#define SOURCE_PATH PATH_TO_DATA "source"
#define TARGET_PATH PATH_TO_DATA "target"
#define SOURCE_GUEST "/data/data/moe.low.arc/source"
#define TARGET_GUEST "/data/user/0/moe.low.arc/target"

static mock_file_t* __seed(const char* path, const char* contents, unsigned mode)
{
    mock_file_t* file = __claim(path);
    assert(file && strlen(contents) < sizeof(file->bytes));
    file->exists = true;
    file->nonempty = false;
    file->status = (SceIoStat){.st_mode = mode, .st_size = (int64_t)strlen(contents)};

    strcpy(file->bytes, contents);
    return file;
}

static void __reset_rename(void)
{
    memset(__files, 0, sizeof(__files));
    __seed(SOURCE_PATH, "new", SCE_S_IFREG | 0600);
    __seed(TARGET_PATH, "old", SCE_S_IFREG | 0600);
    __rename_calls = __rename_failure_call = __backup_collisions = 0;
    __rename_failure_error = EACCES;
    __rollback_failure = __cleanup_failure = __atomic_replace = false;
    __directory_failure = 0;
    __warnings = __errors = 0;
}

static void __expect_contents(const char* path, const char* contents)
{
    mock_file_t* file = __lookup(path);
    assert(file && file->exists && !strcmp(file->bytes, contents));
}

static void __expect_originals(void)
{
    __expect_contents(SOURCE_PATH, "new");
    __expect_contents(TARGET_PATH, "old");
}

static mock_file_t* __find_backup(void)
{
    for(unsigned index = 0; index < sizeof(__files) / sizeof(__files[0]); ++index) {
        if(__files[index].exists && strstr(__files[index].path, ".vitaarc-rename-")) {
            return &__files[index];
        }
    }

    return NULL;
}

static void __test_rename(void)
{
    __reset_rename();
    assert(bridge_rename("/data/data/moe.low.arc/missing", TARGET_GUEST) == -1 && errno == ENOENT);
    assert(bridge_rename("missing", "missing") == -1 && errno == ENOENT);
    assert(bridge_rename(NULL, TARGET_GUEST) == -1 && errno == EFAULT);
    assert(bridge_rename(SOURCE_GUEST, NULL) == -1 && errno == EFAULT);
    assert(bridge_rename(SOURCE_GUEST, "/data/user/0/moe.low.arc/source") == 0 && __rename_calls == 0);
    __expect_originals();
    assert(bridge_rename(SOURCE_GUEST, "uma0:target") == -1 && errno == EXDEV && __rename_calls == 0);
    __expect_originals();

    /* Existing backups survive, and collisions never invoke remove. */
    __seed(PATH_TO_DATA ".vitaarc-rename-00000001.bak", "recovery", SCE_S_IFREG | 0600);
    assert(bridge_rename(SOURCE_GUEST, TARGET_GUEST) == 0 && __rename_calls == 4);
    __expect_contents(TARGET_PATH, "new");
    __expect_contents(PATH_TO_DATA ".vitaarc-rename-00000001.bak", "recovery");

    __reset_rename();
    __lookup(TARGET_PATH)->exists = false;
    assert(bridge_rename(SOURCE_GUEST, TARGET_GUEST) == 0 && __rename_calls == 1 && !__find_backup());
    __expect_contents(TARGET_PATH, "new");

    __reset_rename();
    __atomic_replace = true;
    assert(bridge_rename(SOURCE_GUEST, TARGET_GUEST) == 0 && __rename_calls == 1 && !__find_backup());
    __expect_contents(TARGET_PATH, "new");

    __reset_rename();
    __rename_failure_call = 1;
    assert(bridge_rename(SOURCE_GUEST, TARGET_GUEST) == -1 && errno == EACCES && !__find_backup());
    __expect_originals();

    __reset_rename();
    __rename_failure_call = 2;
    assert(bridge_rename(SOURCE_GUEST, TARGET_GUEST) == -1 && errno == EACCES && !__find_backup());
    __expect_originals();

    __reset_rename();
    __rename_failure_call = 3;
    __rename_failure_error = ENOSPC;
    assert(
        bridge_rename(SOURCE_GUEST, TARGET_GUEST) == -1 && errno == ENOSPC && __rename_calls == 4 && !__find_backup());
    __expect_originals();

    __reset_rename();
    __rename_failure_call = 3;
    __rollback_failure = true;
    assert(bridge_rename(SOURCE_GUEST, TARGET_GUEST) == -1 && errno == EIO && __errors == 1);
    __expect_contents(SOURCE_PATH, "new");
    assert(__find_backup() && !strcmp(__find_backup()->bytes, "old"));

    __reset_rename();
    __cleanup_failure = true;
    assert(bridge_rename(SOURCE_GUEST, TARGET_GUEST) == 0 && __warnings == 1);
    __expect_contents(TARGET_PATH, "new");
    assert(__find_backup() && !strcmp(__find_backup()->bytes, "old"));

    __reset_rename();
    __backup_collisions = 100;
    assert(
        bridge_rename(SOURCE_GUEST, TARGET_GUEST) == -1 && errno == EEXIST && __rename_calls == 65 && !__find_backup());
    __expect_originals();

    __reset_rename();
    __lookup(TARGET_PATH)->status.st_mode = SCE_S_IFDIR | 0700;
    assert(bridge_rename(SOURCE_GUEST, TARGET_GUEST) == -1 && errno == EISDIR && __rename_calls == 0);
    __expect_originals();
    __lookup(TARGET_PATH)->status.st_mode = SCE_S_IFREG | 0600;
    __lookup(SOURCE_PATH)->status.st_mode = SCE_S_IFDIR | 0700;
    assert(bridge_rename(SOURCE_GUEST, TARGET_GUEST) == -1 && errno == ENOTDIR && __rename_calls == 0);
    assert(bridge_rename(SOURCE_GUEST, SOURCE_GUEST "/child") == -1 && errno == EINVAL && __rename_calls == 0);
    __expect_originals();

    __lookup(TARGET_PATH)->status.st_mode = SCE_S_IFDIR | 0700;
    __lookup(TARGET_PATH)->nonempty = true;
    assert(bridge_rename(SOURCE_GUEST, TARGET_GUEST) == -1 && errno == ENOTEMPTY && __rename_calls == 0);
    __lookup(TARGET_PATH)->nonempty = false;
    __directory_failure = EIO;
    assert(bridge_rename(SOURCE_GUEST, TARGET_GUEST) == -1 && errno == EIO && __rename_calls == 0);
    __directory_failure = 0;
    assert(bridge_rename(SOURCE_GUEST, TARGET_GUEST) == 0 && __rename_calls == 3 && !__find_backup());
    assert(SCE_S_ISDIR(__lookup(TARGET_PATH)->status.st_mode));
}

static void* __rename_worker(void* path)
{
    assert(bridge_rename(path, TARGET_GUEST) == 0);
    return NULL;
}

static void __test_concurrent_renames(void)
{
    for(unsigned iteration = 0; iteration < 16; ++iteration) {
        __reset_rename();
        __seed(PATH_TO_DATA "second", "second", SCE_S_IFREG | 0600);
        pthread_t first, second;
        assert(pthread_create(&first, NULL, __rename_worker, SOURCE_GUEST) == 0);
        assert(pthread_create(&second, NULL, __rename_worker, "/data/data/moe.low.arc/second") == 0);
        assert(pthread_join(first, NULL) == 0 && pthread_join(second, NULL) == 0);
        assert(__rename_calls == 6 && !__find_backup() && __errors == 0 && __warnings == 0);
        assert(!__lookup(SOURCE_PATH) && !__lookup(PATH_TO_DATA "second"));
        mock_file_t* target = __lookup(TARGET_PATH);
        assert(target && target->exists && (!strcmp(target->bytes, "new") || !strcmp(target->bytes, "second")));
    }
}

int main(void)
{
    __pool[1] = (vita_newlib_descriptor_t){.uid = 777, .type = 3, .references = 1};

    __vita_fdmap[1] = &__pool[1];
    __test_files();
    __test_random();
    assert(bridge_mkdir("/data/user/0/moe.low.arc/requested", 0610) == 0);
    assert(__lookup(PATH_TO_DATA "requested")->status.st_mode == (SCE_S_IFDIR | 0616));
    assert(bridge_mkdir("/data/data/moe.low.arc/requested", 0700) == -1 && errno == EEXIST);
    __test_rename();
    __test_concurrent_renames();
    for(int fd = 3; fd < 256; ++fd) {
        assert(!__vita_fdmap[fd] && !__pool[fd].references);
    }

    for(unsigned uid = 10; uid < __next_uid; ++uid) {
        assert(!__opened[uid].file);
    }

    puts(
        "Vita file boundary passed: descriptor pins, flags/mode, full-width stat, RNG chunk/failure/dup/close and cleanup");
    return 0;
}
