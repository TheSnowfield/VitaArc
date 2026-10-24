#define _GNU_SOURCE

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libc/directory.h>
#include <libc/file.h>
#include <libc/posix.h>
#include "paths.h"

static bool __fail_allocation;
static char __long_name[256];
static atomic_uint __seen;

void* __real_calloc(size_t count, size_t size);
void* __wrap_calloc(size_t count, size_t size)
{
    if(__fail_allocation) {
        __fail_allocation = false;
        errno = ENOMEM;
        return NULL;
    }

    return __real_calloc(count, size);
}

static unsigned __fd_count(void)
{
    DIR* directory = opendir("/proc/self/fd");
    assert(directory);
    unsigned count = 0;
    while(readdir(directory)) {
        ++count;
    }

    assert(closedir(directory) == 0);
    return count;
}

static void __check_entry(const bridge_dirent_t* entry)
{
    assert(entry->offset > 0 && entry->inode);
    size_t length = strlen(entry->name);
    assert(length <= 255);
    assert(entry->record_length == ((19 + length + 1 + 7) & ~7u));
    char path[512];
    snprintf(path, sizeof(path), "/data/data/moe.low.arc/list/%s", entry->name);
    bridge_bionic_stat_t status;
    assert(bridge_bionic_lstat(path, &status) == 0);
    /* Some WSL filesystems report a different inode for '..' in getdents than
     * stat. Compare every converted inode with independent native readdir. */
    DIR* native = opendir(PATH_TO_DATA "list");
    assert(native);
    struct dirent* native_entry;
    bool found = false;
    while((native_entry = readdir(native))) {
        if(!strcmp(native_entry->d_name, entry->name)) {
            assert(native_entry->d_ino == entry->inode && native_entry->d_type == entry->type);
            found = true;
            break;
        }
    }

    assert(closedir(native) == 0 && found);
    unsigned expected = (status.mode & 0170000) == 0040000 ? 4 : (status.mode & 0170000) == 0120000 ? 10 : 8;
    assert(entry->type == expected);
}

static void __check_positions(void)
{
    bridge_directory_t* directory = bridge_opendir("/data/user/0/moe.low.arc/list");
    bridge_directory_t* second = bridge_opendir("/data/data/moe.low.arc/list");
    assert(directory && second);
    int fd = bridge_dirfd(directory);
    assert(fd >= 0 && (fcntl(fd, F_GETFD) & FD_CLOEXEC));
    assert(bridge_telldir(directory) == 0);
    bridge_dirent_t first = *bridge_readdir(directory);
    bridge_dirent_t next = *bridge_readdir(directory);
    assert(bridge_telldir(directory) == 2);
    assert(!strcmp(bridge_readdir(second)->name, first.name));
    assert(bridge_telldir(second) == 1);
    bridge_seekdir(directory, 1);
    assert(bridge_telldir(directory) == 1 && !strcmp(bridge_readdir(directory)->name, next.name));
    bridge_seekdir(directory, -1);
    assert(errno == EINVAL && bridge_telldir(directory) == 2);
    bridge_seekdir(directory, 1000);
    assert(errno == EINVAL && bridge_telldir(directory) == 2);
    bridge_rewinddir(directory);
    unsigned count = 0;
    bridge_dirent_t* entry;
    errno = EBUSY;
    while((entry = bridge_readdir(directory))) {
        assert(errno == EBUSY);
        __check_entry(entry);
        assert(entry->offset == ++count);
    }

    assert(count == 6 && errno == EBUSY);
    bridge_rewinddir(directory);
    struct {
        uint64_t before;
        bridge_dirent_t entry;
        uint64_t after;
    } output = {.before = UINT64_MAX, .after = UINT64_MAX};

    count = 0;
    errno = ERANGE;
    for(;;) {
        bridge_dirent_t* result = (void*)1;
        assert(bridge_readdir_r(directory, &output.entry, &result) == 0);
        assert(errno == ERANGE && output.before == UINT64_MAX && output.after == UINT64_MAX);
        if(!result) {
            break;
        }

        assert(result == &output.entry);
        __check_entry(result);
        ++count;
    }

    assert(count == 6);
    assert(bridge_closedir(directory) == 0 && fcntl(fd, F_GETFD) == -1 && errno == EBADF);
    assert(bridge_closedir(second) == 0);
}

static void __check_ownership(void)
{
    unsigned count = __fd_count();
    __fail_allocation = true;
    assert(!bridge_opendir("/data/data/moe.low.arc/list") && errno == ENOMEM);
    assert(__fd_count() == count);
    int fd = bridge_open("/data/data/moe.low.arc/list/file", BIONIC_O_RDONLY);
    assert(fd >= 0 && !bridge_fdopendir(fd) && errno == ENOTDIR && fcntl(fd, F_GETFD) >= 0);
    assert(bridge_close(fd) == 0);
    fd = bridge_open("/data/data/moe.low.arc/list", BIONIC_O_RDONLY | BIONIC_O_DIRECTORY);
    assert(fd >= 0);
    __fail_allocation = true;
    assert(!bridge_fdopendir(fd) && errno == ENOMEM && fcntl(fd, F_GETFD) >= 0);
    bridge_directory_t* directory = bridge_fdopendir(fd);
    assert(directory && bridge_dirfd(directory) == fd && bridge_closedir(directory) == 0);
    assert(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
    assert(!bridge_opendir(NULL) && errno == EFAULT);
    assert(!bridge_opendir("/data/data/moe.low.arc/missing") && errno == ENOENT);
    assert(!bridge_opendir("/data/data/moe.low.arc/list/file") && errno == ENOTDIR);
    assert(!bridge_fdopendir(-1) && errno == EBADF);
    assert(!bridge_readdir(NULL) && errno == EBADF);
    assert(bridge_closedir(NULL) == -1 && errno == EINVAL);
    bridge_dirent_t entry, *result = (void*)1;
    errno = EBUSY;
    assert(bridge_readdir_r(NULL, &entry, &result) == EINVAL && !result && errno == EBUSY);
    assert(bridge_telldir(NULL) == -1 && errno == EBADF);
    assert(bridge_dirfd(NULL) == -1 && errno == EBADF);
    bridge_rewinddir(NULL);
    assert(errno == EBADF);
    assert(__fd_count() == count);

    int saved = dup(STDIN_FILENO);
    close(STDIN_FILENO);
    directory = bridge_opendir("/data/data/moe.low.arc/list");
    assert(directory && bridge_dirfd(directory) == 0 && bridge_closedir(directory) == 0);
    assert(fcntl(0, F_GETFD) == -1 && errno == EBADF);
    if(saved >= 0) {
        assert(dup2(saved, 0) == 0 && close(saved) == 0);
    }
}

static void* __worker(void* argument)
{
    bridge_directory_t* directory = argument;
    for(;;) {
        bridge_dirent_t entry, *result;
        errno = ENOSPC;
        assert(bridge_readdir_r(directory, &entry, &result) == 0 && errno == ENOSPC);
        if(!result) {
            break;
        }

        __check_entry(result);
        const char* names[] = {".", "..", "file", "folder", "link", __long_name};

        unsigned index;
        for(index = 0; index < 6 && strcmp(entry.name, names[index]); ++index) {
        }

        assert(index < 6);
        unsigned bit = 1u << index;
        assert(!(atomic_fetch_or(&__seen, bit) & bit));
    }

    return NULL;
}

int main(void)
{
    unsigned initial = __fd_count();
    test_dirs(PATH_TO_USER "persistent/");
    assert(bridge_mkdir("/data/data/moe.low.arc", 0700) == 0);
    assert(bridge_mkdir("/data/data/moe.low.arc/list", 0700) == 0);
    assert(bridge_mkdir("/data/data/moe.low.arc/list/folder", 0700) == 0);
    int fd = bridge_open("/data/data/moe.low.arc/list/file", BIONIC_O_CREAT | BIONIC_O_RDWR, 0600u);
    assert(fd >= 0 && bridge_close(fd) == 0);
    assert(symlink("file", PATH_TO_DATA "list/link") == 0);
    memset(__long_name, 'x', 255);
    char path[512];
    snprintf(path, sizeof(path), "/data/data/moe.low.arc/list/%s", __long_name);
    fd = bridge_open(path, BIONIC_O_CREAT | BIONIC_O_RDWR, 0600u);
    assert(fd >= 0 && bridge_close(fd) == 0);
    __check_positions();
    __check_ownership();
    bridge_directory_t* directory = bridge_opendir("/data/data/moe.low.arc/list");
    assert(directory);
    pthread_t threads[4];
    for(unsigned index = 0; index < 4; ++index) {
        assert(!pthread_create(&threads[index], NULL, __worker, directory));
    }

    for(unsigned index = 0; index < 4; ++index) {
        assert(!pthread_join(threads[index], NULL));
    }

    assert(atomic_load(&__seen) == 63 && bridge_closedir(directory) == 0);
    assert(__fd_count() == initial);
    puts(
        "Directory ABI, aliases/types/inodes, cookies/rewind, 255-byte names, fd ownership/zero, failure cleanup and shared-stream reentrant reads passed.");
}
