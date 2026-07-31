#ifndef VITAARC_LIBC_FILE_SHARED_H
#define VITAARC_LIBC_FILE_SHARED_H

#include <errno.h>
#include <stdbool.h>
#ifdef __vita__
#include <platform/newlib.h>
static inline bool __random_descriptor(vita_newlib_descriptor_t* descriptor)
{
    return descriptor && descriptor->type == VITAARC_DESCRIPTOR_RANDOM;
}

static inline int __native_error(int result)
{
    errno = __vita_sce_errno_to_errno(result, 0);
    return -1;
}

#endif

#endif
