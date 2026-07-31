#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <libc/memory.h>

#define PAGE 4096u
#define FAILED ((void*)-1)
#define RW (BIONIC_PROT_READ | BIONIC_PROT_WRITE)
#define ANON (BIONIC_MAP_PRIVATE | BIONIC_MAP_ANONYMOUS)

static _Thread_local int __fail_allocation, __fail_dup, __fail_read, __interrupt_read;
static _Thread_local size_t __read_limit;
static _Thread_local bool __fail_storage;
static atomic_int __blocks;

void* __real_calloc(size_t count, size_t size);
void* __real_malloc(size_t size);
int __real_dup(int fd);
ssize_t __real_pread(int fd, void* buffer, size_t length, off_t offset);
void* __real_mmap(void* address, size_t length, int protection, int flags, int fd, off_t offset);
int __real_munmap(void* address, size_t length);

void* __wrap_mmap(void* address, size_t length, int protection, int flags, int fd, off_t offset)
{
    if(__fail_storage) {
        __fail_storage = false;
        errno = ENOMEM;
        return MAP_FAILED;
    }

    void* result = __real_mmap(address, length, protection, flags, fd, offset);
    if(result != MAP_FAILED) {
        atomic_fetch_add(&__blocks, 1);
    }

    return result;
}

int __wrap_munmap(void* address, size_t length)
{
    int result = __real_munmap(address, length);
    assert(result == 0);
    atomic_fetch_sub(&__blocks, 1);
    return result;
}

void* __wrap_calloc(size_t count, size_t size)
{
    if(__fail_allocation && !--__fail_allocation) {
        errno = ENOMEM;
        return NULL;
    }

    return __real_calloc(count, size);
}

void* __wrap_malloc(size_t size)
{
    if(__fail_allocation && !--__fail_allocation) {
        errno = ENOMEM;
        return NULL;
    }

    return __real_malloc(size);
}

int __wrap_dup(int fd)
{
    if(__fail_dup) {
        __fail_dup = 0;
        errno = EMFILE;
        return -1;
    }

    return __real_dup(fd);
}

ssize_t __wrap_pread(int fd, void* buffer, size_t length, off_t offset)
{
    if(__interrupt_read) {
        --__interrupt_read;
        errno = EINTR;
        return -1;
    }

    if(__fail_read && !--__fail_read) {
        errno = EIO;
        return -1;
    }

    if(__read_limit && length > __read_limit) {
        length = __read_limit;
    }

    return __real_pread(fd, buffer, length, offset);
}

static void* __anonymous(size_t size)
{
    return bridge_mmap(NULL, size, RW, ANON, -1, 0);
}

static void __check_invalid_calls(void)
{
    assert(__anonymous(0) == FAILED && errno == EINVAL);
    assert(__anonymous(SIZE_MAX) == FAILED && errno == ENOMEM);
    assert(bridge_mmap(NULL, PAGE, RW, BIONIC_MAP_PRIVATE, -1, 0) == FAILED && errno == EBADF);
    assert(bridge_mmap(NULL, PAGE, RW, ANON, -1, 1) == FAILED && errno == EINVAL);
    assert(bridge_mmap(NULL, PAGE, RW, ANON, -1, -4096) == FAILED && errno == EINVAL);
    assert(bridge_mmap(NULL, PAGE, RW, 0, -1, 0) == FAILED && errno == EINVAL);
    assert(bridge_mmap(NULL, PAGE, RW, ANON | 0x40000000, -1, 0) == FAILED && errno == EINVAL);
    assert(bridge_mmap(NULL, PAGE, RW, BIONIC_MAP_SHARED | BIONIC_MAP_ANONYMOUS, -1, 0) == FAILED && errno == ENOTSUP);
    assert(bridge_mmap(NULL, PAGE, RW, ANON | BIONIC_MAP_FIXED, -1, 0) == FAILED && errno == ENOTSUP);
    assert(bridge_mmap(NULL, PAGE, BIONIC_PROT_EXEC, ANON, -1, 0) == FAILED && errno == ENOTSUP);
    assert(bridge_mmap(NULL, PAGE, 0, ANON, -1, 0) == FAILED && errno == ENOTSUP);
    assert(bridge_munmap((void*)1, PAGE) == -1 && errno == EINVAL);
    assert(bridge_munmap(NULL, 0) == -1 && errno == EINVAL);
    assert(bridge_munmap((void*)(UINTPTR_MAX & ~(uintptr_t)(PAGE - 1)), PAGE) == -1 && errno == EINVAL);
    assert(bridge_munmap((void*)0x1000, PAGE) == 0); /* No mapping: no arbitrary free. */
    assert(bridge_mremap((void*)0x1000, PAGE, PAGE * 2, 1) == FAILED && errno == EFAULT);
    for(int index = 1; index <= 2; ++index) {
        __fail_allocation = index;
        assert(__anonymous(PAGE) == FAILED && errno == ENOMEM);
    }

    __fail_storage = true;
    assert(__anonymous(PAGE) == FAILED && errno == ENOMEM);
}

static void __check_ranges(void)
{
    unsigned char* base = __anonymous(PAGE * 4 - 5);
    assert(base != FAILED && !((uintptr_t)base & (PAGE - 1)));
    for(size_t index = 0; index < PAGE * 4; ++index) {
        assert(base[index] == 0);
        base[index] = (unsigned char)(index / PAGE + 1);
    }

    assert(bridge_munmap(base + PAGE, 1) == 0);
    assert(bridge_mremap(base, PAGE * 4, PAGE * 5, 1) == FAILED && errno == EFAULT);
    assert(bridge_mremap(base + 2 * PAGE, 2 * PAGE, PAGE, 0) == base + 2 * PAGE);
    assert(bridge_mremap(base + 2 * PAGE, PAGE, PAGE * 2, 0) == base + 2 * PAGE);
    for(size_t index = 0; index < PAGE; ++index) {
        assert(base[index + 3 * PAGE] == 0);
    }

    unsigned char* moved = bridge_mremap(base + 2 * PAGE, PAGE * 2, PAGE * 3, BIONIC_MREMAP_MAYMOVE);
    assert(moved != FAILED && moved != base + 2 * PAGE);
    for(size_t index = 0; index < PAGE; ++index) {
        assert(moved[index] == 3 && moved[index + PAGE] == 0);
        assert(base[index] == 1);
    }

    assert(bridge_munmap(base, PAGE * 4) == 0);
    assert(bridge_munmap(moved, PAGE * 3) == 0);
    assert(bridge_munmap(moved, PAGE * 3) == 0);

    base = __anonymous(PAGE);
    assert(base != FAILED);
    memset(base, 0x5a, PAGE);
    __fail_allocation = 1;
    assert(bridge_mremap(base, PAGE, PAGE * 2, 1) == FAILED && errno == ENOMEM);
    for(size_t index = 0; index < PAGE; ++index) {
        assert(base[index] == 0x5a);
    }

    assert(bridge_mremap(base + 1, PAGE, PAGE * 2, 1) == FAILED && errno == EINVAL);
    assert(bridge_mremap(base, PAGE, PAGE * 2, 2) == FAILED && errno == EINVAL);
    assert(bridge_mremap(base, PAGE, PAGE * 2, 3, NULL) == FAILED && errno == ENOTSUP);
    assert(bridge_mremap(base, PAGE, PAGE * 2, 4) == FAILED && errno == EINVAL);
    assert(bridge_munmap(base, PAGE) == 0);
}

static bool __is_read_only(void* address)
{
    FILE* maps = fopen("/proc/self/maps", "r");
    assert(maps);
    char line[512], permissions[5];
    uintptr_t begin, end;
    bool result = false;
    while(fgets(line, sizeof(line), maps)) {
        if(sscanf(line, "%lx-%lx %4s", &begin, &end, permissions) == 3 && (uintptr_t)address >= begin &&
            (uintptr_t)address < end) {
            result = permissions[0] == 'r' && permissions[1] == '-';
            break;
        }
    }

    fclose(maps);
    return result;
}

static void __check_files(void)
{
    char path[] = "mapping-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    unsigned char bytes[PAGE * 3];
    for(size_t index = 0; index < sizeof(bytes); ++index) {
        bytes[index] = (unsigned char)(index * 17 + index / PAGE);
    }

    assert(write(fd, bytes, sizeof(bytes)) == sizeof(bytes));
    assert(lseek(fd, 23, SEEK_SET) == 23);
    __read_limit = 97;
    __interrupt_read = 2;
    unsigned char* base = bridge_mmap(NULL, PAGE, RW, BIONIC_MAP_PRIVATE, fd, PAGE);
    __read_limit = 0;
    assert(base != FAILED && !memcmp(base, bytes + PAGE, PAGE));
    assert(lseek(fd, 0, SEEK_CUR) == 23);
    base[0] = 77;
    unsigned char original;
    assert(pread(fd, &original, 1, PAGE) == 1 && original == bytes[PAGE]);
    __fail_dup = 1;
    assert(bridge_mmap(NULL, PAGE, RW, BIONIC_MAP_PRIVATE, fd, 0) == FAILED && errno == EMFILE);
    __fail_read = 2;
    assert(bridge_mmap(NULL, PAGE * 3, RW, BIONIC_MAP_PRIVATE, fd, 0) == FAILED && errno == EIO);
    __fail_read = 1;
    assert(bridge_mremap(base, PAGE, PAGE * 2, 1) == FAILED && errno == EIO);
    assert(base[0] == 77 && base[1] == bytes[PAGE + 1]);
    close(fd); /* Growth still has the open file and the correct offset. */
    unsigned char* moved = bridge_mremap(base, PAGE, PAGE * 2, 1);
    assert(moved != FAILED && moved[0] == 77 && !memcmp(moved + PAGE, bytes + PAGE * 2, PAGE));
    assert(bridge_munmap(moved, PAGE * 2) == 0);

    fd = open(path, O_WRONLY);
    assert(fd >= 0);
    assert(bridge_mmap(NULL, PAGE, RW, BIONIC_MAP_PRIVATE, fd, 0) == FAILED && errno == EACCES);
    close(fd);
    fd = open(path, O_RDWR);
    assert(fd >= 0);
    const off_t large = (off_t)5 * 1024 * 1024 * 1024;
    assert(pwrite(fd, "64-bit", 6, large) == 6);
    base = bridge_mmap2(0, PAGE, BIONIC_PROT_READ, BIONIC_MAP_PRIVATE, fd, large / PAGE);
    assert(base != FAILED && !memcmp(base, "64-bit", 6) && __is_read_only(base));
    for(size_t index = 6; index < PAGE; ++index) {
        assert(base[index] == 0);
    }

    moved = bridge_mremap(base, PAGE, PAGE * 2, 1);
    assert(moved != FAILED && !memcmp(moved, "64-bit", 6) && __is_read_only(moved));
    assert(bridge_munmap(moved, PAGE * 2) == 0);
    close(fd);
    assert(unlink(path) == 0);
}

static void* __worker(void* argument)
{
    unsigned char value = (uintptr_t)argument;
    for(unsigned iteration = 0; iteration < 40; ++iteration) {
        unsigned char* base = __anonymous(PAGE * 3);
        assert(base != FAILED);
        memset(base, value, PAGE * 3);
        assert(bridge_munmap(base + PAGE, PAGE) == 0);
        unsigned char* moved = bridge_mremap(base, PAGE, PAGE * 2, 1);
        assert(moved != FAILED && moved[0] == value && moved[PAGE - 1] == value && moved[PAGE] == 0);
        assert(base[PAGE * 2] == value);
        assert(bridge_munmap(base + PAGE * 2, PAGE) == 0);
        assert(bridge_munmap(moved, PAGE * 2) == 0);
    }

    return NULL;
}

int main(void)
{
    DIR* descriptors = opendir("/proc/self/fd");
    assert(descriptors);
    unsigned fd_count = 0;
    while(readdir(descriptors)) {
        ++fd_count;
    }

    closedir(descriptors);
    __check_invalid_calls();
    __check_ranges();
    __check_files();
    pthread_t threads[4];
    for(unsigned index = 0; index < 4; ++index) {
        assert(!pthread_create(&threads[index], NULL, __worker, (void*)(uintptr_t)(index + 1)));
    }

    for(unsigned index = 0; index < 4; ++index) {
        assert(!pthread_join(threads[index], NULL));
    }

    assert(atomic_load(&__blocks) == 0);
    descriptors = opendir("/proc/self/fd");
    assert(descriptors);
    unsigned final_count = 0;
    while(readdir(descriptors)) {
        ++final_count;
    }

    closedir(descriptors);
    assert(final_count == fd_count);
    puts(
        "Mapping alignment, ranges, failure rollback, private file contents/offsets, read protection, 64-bit mmap2 and concurrency passed.");
}
