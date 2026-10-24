#ifndef VITAARC_LIBC_MEMORY_H
#define VITAARC_LIBC_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* Android ARM constants, independent of the native SDK's headers. */
enum {
    BIONIC_PROT_READ = 1,
    BIONIC_PROT_WRITE = 2,
    BIONIC_PROT_EXEC = 4,
    BIONIC_MAP_SHARED = 1,
    BIONIC_MAP_PRIVATE = 2,
    BIONIC_MAP_FIXED = 0x10,
    BIONIC_MAP_ANONYMOUS = 0x20,
    BIONIC_MREMAP_MAYMOVE = 1,
    BIONIC_MREMAP_FIXED = 2
};

void* bridge_mmap(void* address, size_t length, int protection, int flags, int fd, off_t offset);
void* bridge_mmap64(void* address, size_t length, int protection, int flags, int fd, int64_t offset);
void* bridge_mmap2(unsigned long address, unsigned long length, unsigned long protection, unsigned long flags,
    unsigned long fd, unsigned long page_offset);
int bridge_munmap(void* address, size_t length);
int bridge_posix_memalign(void** address, size_t alignment, size_t length);
int bridge_mprotect(void* address, size_t length, int protection);
int bridge_madvise(void* address, size_t length, int advice);
int bridge_mlock(const void* address, size_t length);
void* bridge_mremap(void* address, size_t old_size, size_t new_size, int flags, ...);

#endif
