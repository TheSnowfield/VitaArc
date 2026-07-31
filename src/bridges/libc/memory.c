#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __vita__
#include <psp2/kernel/sysmem.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <platform/newlib.h>
#include <kubridge.h>
#else
#include <sys/mman.h>
#endif

#include "memory.h"

#define PAGE_BYTES 4096u
#define MAP_FAILURE ((void*)-1)

typedef struct mapping {
    struct mapping* next;
    unsigned char* base;
    size_t capacity;
    size_t active_pages;
    unsigned char* pages;
    int protection;
    int fd;
    int64_t offset;
#ifdef __vita__
    SceUID block;
#endif
} mapping_t;

static mapping_t* __mappings;
static pthread_mutex_t __mapping_mutex = PTHREAD_MUTEX_INITIALIZER;

static bool __round_length(size_t length, size_t* rounded)
{
    if(!length || length > (size_t)PTRDIFF_MAX - (PAGE_BYTES - 1)) {
        return false;
    }

    *rounded = (length + PAGE_BYTES - 1) & ~(size_t)(PAGE_BYTES - 1);
    return true;
}

static void* __failure(int error)
{
    errno = error;
    return MAP_FAILURE;
}

static void __release(mapping_t* mapping)
{
    if(mapping->base) {
#ifdef __vita__
        sceKernelFreeMemBlock(mapping->block);
#else
        munmap(mapping->base, mapping->capacity);
#endif
    }

    if(mapping->fd >= 0) {
        close(mapping->fd);
    }

    free(mapping->pages);
    free(mapping);
}

static bool __allocate_storage(mapping_t* mapping)
{
#ifdef __vita__
    int type = mapping->protection == BIONIC_PROT_READ ? SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_R
                                                       : SCE_KERNEL_MEMBLOCK_TYPE_USER_RW;
    mapping->block = sceKernelAllocMemBlock("guest_mmap", type, mapping->capacity, NULL);
    if(mapping->block <= 0) {
        errno = ENOMEM;
        return false;
    }

    if(sceKernelGetMemBlockBase(mapping->block, (void**)&mapping->base) < 0 || !mapping->base) {
        sceKernelFreeMemBlock(mapping->block);
        mapping->base = NULL;
        errno = ENOMEM;
        return false;
    }

#else
    mapping->base = mmap(NULL, mapping->capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(mapping->base == MAP_FAILED) {
        mapping->base = NULL;
        return false;
    }

#endif
    return true;
}

static bool __copy(void* destination, const void* source, size_t length)
{
#ifdef __vita__
    if(kuKernelCpuUnrestrictedMemcpy(destination, source, length) < 0) {
        errno = EIO;
        return false;
    }

#else
    memcpy(destination, source, length);
#endif
    return true;
}

static bool __writable(mapping_t* mapping, bool enabled)
{
#ifndef __vita__
    if(mapping->protection == BIONIC_PROT_READ &&
        mprotect(mapping->base, mapping->capacity, enabled ? PROT_READ | PROT_WRITE : PROT_READ) < 0) {
        return false;
    }

#else
    (void)mapping;
    (void)enabled;
#endif
    return true;
}

/* Positional reads leave the caller's shared descriptor offset unchanged.
 * The duplicate keeps the open file alive for a later file-backed mremap. */
static bool __populate(mapping_t* mapping, size_t start, size_t length)
{
    unsigned char buffer[PAGE_BYTES];
#ifdef __vita__
    vita_newlib_descriptor_t* descriptor = NULL;
    if(mapping->fd >= 0) {
        descriptor = __vita_fd_grab(mapping->fd);
        if(!descriptor) {
            errno = EBADF;
            return false;
        }

        if(descriptor->type != 0) {
            __vita_fd_drop(descriptor);
            errno = ENODEV;
            return false;
        }

        if((descriptor->flags & O_ACCMODE) == O_WRONLY) {
            __vita_fd_drop(descriptor);
            errno = EACCES;
            return false;
        }

        SceIoStat status;
        int result = sceIoGetstatByFd(descriptor->uid, &status);
        if(result < 0 || (status.st_mode & SCE_S_IFMT) != SCE_S_IFREG) {
            __vita_fd_drop(descriptor);
            errno = result < 0 ? __vita_sce_errno_to_errno(result, 0) : ENODEV;
            return false;
        }
    }

#endif
    bool okay = true;
    for(size_t done = 0; done < length && okay; done += PAGE_BYTES) {
        memset(buffer, 0, sizeof(buffer));
        size_t count = length - done < PAGE_BYTES ? length - done : PAGE_BYTES;
        size_t filled = 0;
        while(mapping->fd >= 0 && filled < count) {
            int64_t offset = mapping->offset + (int64_t)start + (int64_t)done + (int64_t)filled;
#ifdef __vita__
            ssize_t result = sceIoPread(descriptor->uid, buffer + filled, count - filled, offset);
            if(result < 0) {
                errno = __vita_sce_errno_to_errno((int)result, 0);
            }

#else
            ssize_t result = pread(mapping->fd, buffer + filled, count - filled, offset);
#endif
            if(result < 0 && errno == EINTR) {
                continue;
            }

            if(result < 0) {
                okay = false;
                break;
            }

            if(!result) {
                break;
            }

            filled += (size_t)result;
        }

        if(okay) {
            okay = __copy(mapping->base + start + done, buffer, count);
        }
    }

#ifdef __vita__
    if(descriptor) {
        __vita_fd_drop(descriptor);
    }

#endif
    return okay;
}

static mapping_t* __create(size_t length, int protection, int fd, int64_t offset, size_t populate_start)
{
    mapping_t* mapping = calloc(1, sizeof(*mapping));
    if(!mapping) {
        return NULL;
    }

    mapping->fd = -1;
    mapping->capacity = length;
    mapping->protection = protection;
    mapping->offset = offset;
    mapping->active_pages = length / PAGE_BYTES;
    mapping->pages = malloc(mapping->active_pages);
    if(!mapping->pages) {
        __release(mapping);
        return NULL;
    }

    memset(mapping->pages, 1, mapping->active_pages);
    if(fd >= 0) {
        mapping->fd = dup(fd);
        if(mapping->fd < 0) {
            __release(mapping);
            return NULL;
        }
    }

    if(!__allocate_storage(mapping) || !__populate(mapping, populate_start, length - populate_start) ||
        !__writable(mapping, false)) {
        int error = errno;
        __release(mapping);
        errno = error;
        return NULL;
    }

    return mapping;
}

static void* __map(size_t length, int protection, int flags, int fd, int64_t offset)
{
    size_t rounded;
    if(!length || offset < 0 || (offset & (PAGE_BYTES - 1))) {
        return __failure(EINVAL);
    }

    if(!__round_length(length, &rounded)) {
        return __failure(ENOMEM);
    }

    if(offset > INT64_MAX - (int64_t)rounded) {
        return __failure(EOVERFLOW);
    }

    if(protection & ~(BIONIC_PROT_READ | BIONIC_PROT_WRITE | BIONIC_PROT_EXEC)) {
        return __failure(EINVAL);
    }

    const int known =
        3 | BIONIC_MAP_FIXED | BIONIC_MAP_ANONYMOUS | 0x800 | 0x1000 | 0x4000 | 0x8000 | 0x20000 | 0x100000;
    if((flags & ~known) || !(flags & 3) || (flags & 3) == 3) {
        return __failure(EINVAL);
    }

    /* Vita has no shared page cache, reservation/protection changes, or safe
     * replacement of arbitrary caller addresses through these user APIs. */
    if((flags & 3) != BIONIC_MAP_PRIVATE || (flags & (BIONIC_MAP_FIXED | 0x100000)) ||
        (protection != BIONIC_PROT_READ && protection != (BIONIC_PROT_READ | BIONIC_PROT_WRITE))) {
        return __failure(ENOTSUP);
    }

    if(flags & BIONIC_MAP_ANONYMOUS) {
        fd = -1;
    } else if(fd < 0) {
        return __failure(EBADF);
    }

#ifndef __vita__
    if(fd >= 0) {
        int access = fcntl(fd, F_GETFL);
        if(access < 0) {
            return MAP_FAILURE;
        }

        if((access & O_ACCMODE) == O_WRONLY) {
            return __failure(EACCES);
        }

        struct stat status;
        if(fstat(fd, &status) < 0) {
            return MAP_FAILURE;
        }

        if(!S_ISREG(status.st_mode)) {
            return __failure(ENODEV);
        }
    }

#endif
    mapping_t* mapping = __create(rounded, protection, fd, offset, 0);
    if(!mapping) {
        return MAP_FAILURE;
    }

    pthread_mutex_lock(&__mapping_mutex);
    mapping->next = __mappings;
    __mappings = mapping;
    pthread_mutex_unlock(&__mapping_mutex);
    return mapping->base;
}

void* bridge_mmap(void* address, size_t length, int protection, int flags, int fd, off_t offset)
{
    (void)address; /* Non-fixed addresses are hints. */
    return __map(length, protection, flags, fd, offset);
}

void* bridge_mmap2(unsigned long address, unsigned long length, unsigned long protection, unsigned long flags,
    unsigned long fd, unsigned long page_offset)
{
    (void)address;
    if(page_offset > (uint64_t)INT64_MAX / PAGE_BYTES) {
        return __failure(EOVERFLOW);
    }

    return __map(length, (int)protection, (int)flags, (int)fd, (int64_t)page_offset * PAGE_BYTES);
}

/* Partial unmaps retire pages immediately from the registry. The backing Vita
 * memblock is retained until its last live page is unmapped; holes are neither
 * protected nor returned to the kernel individually. */
static void __unmap(uintptr_t address, size_t length)
{
    uintptr_t end = address + length;
    mapping_t** link = &__mappings;
    while(*link) {
        mapping_t* mapping = *link;
        uintptr_t base = (uintptr_t)mapping->base;
        uintptr_t last = base + mapping->capacity;
        uintptr_t first = address > base ? address : base;
        uintptr_t stop = end < last ? end : last;
        for(uintptr_t page = first; page < stop; page += PAGE_BYTES) {
            size_t index = (page - base) / PAGE_BYTES;
            if(mapping->pages[index]) {
                mapping->pages[index] = 0;
                --mapping->active_pages;
            }
        }

        if(!mapping->active_pages) {
            *link = mapping->next;
            __release(mapping);
        } else {
            link = &mapping->next;
        }
    }
}

int bridge_munmap(void* address, size_t length)
{
    size_t rounded;
    if(((uintptr_t)address & (PAGE_BYTES - 1)) || !__round_length(length, &rounded) ||
        (uintptr_t)address > UINTPTR_MAX - rounded) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&__mapping_mutex);
    __unmap((uintptr_t)address, rounded);
    pthread_mutex_unlock(&__mapping_mutex);
    return 0;
}

void* bridge_mremap(void* address, size_t old_size, size_t new_size, int flags, ...)
{
    size_t old_length, new_length;
    uintptr_t start = (uintptr_t)address;
    if((start & (PAGE_BYTES - 1)) || !__round_length(old_size, &old_length) || !__round_length(new_size, &new_length) ||
        start > UINTPTR_MAX - old_length || (flags & ~3) ||
        ((flags & BIONIC_MREMAP_FIXED) && !(flags & BIONIC_MREMAP_MAYMOVE))) {
        return __failure(EINVAL);
    }

    if(flags & BIONIC_MREMAP_FIXED) {
        return __failure(ENOTSUP);
    }

    pthread_mutex_lock(&__mapping_mutex);
    mapping_t* mapping = __mappings;
    size_t relative = 0;
    for(; mapping; mapping = mapping->next) {
        uintptr_t base = (uintptr_t)mapping->base;
        if(start >= base && start - base < mapping->capacity && old_length <= mapping->capacity - (start - base)) {
            relative = start - base;
            break;
        }
    }

    if(!mapping) {
        pthread_mutex_unlock(&__mapping_mutex);
        return __failure(EFAULT);
    }

    for(size_t page = relative / PAGE_BYTES; page < (relative + old_length) / PAGE_BYTES; ++page) {
        if(!mapping->pages[page]) {
            pthread_mutex_unlock(&__mapping_mutex);
            return __failure(EFAULT);
        }
    }

    if(new_length <= old_length) {
        if(new_length < old_length) {
            __unmap(start + new_length, old_length - new_length);
        }

        pthread_mutex_unlock(&__mapping_mutex);
        return address;
    }

    if(mapping->offset > INT64_MAX - (int64_t)relative - (int64_t)new_length) {
        pthread_mutex_unlock(&__mapping_mutex);
        return __failure(EOVERFLOW);
    }

    if(new_length <= mapping->capacity - relative) {
        bool available = true;
        for(size_t page = (relative + old_length) / PAGE_BYTES; page < (relative + new_length) / PAGE_BYTES; ++page) {
            if(mapping->pages[page]) {
                available = false;
                break;
            }
        }

        if(available) {
            bool populated =
                __writable(mapping, true) && __populate(mapping, relative + old_length, new_length - old_length);
            int error = errno;
            if(!__writable(mapping, false)) {
                populated = false;
                error = errno;
            }

            if(populated) {
                for(size_t page = (relative + old_length) / PAGE_BYTES; page < (relative + new_length) / PAGE_BYTES;
                    ++page) {
                    mapping->pages[page] = 1;
                    ++mapping->active_pages;
                }
            }

            pthread_mutex_unlock(&__mapping_mutex);
            return populated ? address : __failure(error);
        }
    }

    /* A moved mapping owns a fresh file duplicate. Copy only the old range and
     * populate newly added pages from the file (or zeros for anonymous maps). */
    if(!(flags & BIONIC_MREMAP_MAYMOVE)) {
        pthread_mutex_unlock(&__mapping_mutex);
        return __failure(ENOMEM);
    }

    mapping_t* replacement =
        __create(new_length, mapping->protection, mapping->fd, mapping->offset + relative, old_length);
    if(!replacement) {
        pthread_mutex_unlock(&__mapping_mutex);
        return MAP_FAILURE;
    }

    bool copied = __writable(replacement, true) && __copy(replacement->base, address, old_length) &&
        __writable(replacement, false);
    if(!copied) {
        int error = errno;
        __release(replacement);
        pthread_mutex_unlock(&__mapping_mutex);
        return __failure(error);
    }

    replacement->next = __mappings;
    __mappings = replacement;
    __unmap(start, old_length);
    pthread_mutex_unlock(&__mapping_mutex);
    return replacement->base;
}
