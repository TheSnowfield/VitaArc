#ifndef __PLATFORM_NEWLIB_H
#define __PLATFORM_NEWLIB_H

#include <stdint.h>

/* Vita newlib descriptor ABI, from newlib/libc/sys/vita/vitadescriptor.h.
 * Hold the descriptor reference until the native operation completes. */
typedef struct vita_newlib_descriptor {
    int uid;
    uint8_t type;
    uint8_t type_padding[3];
    int references;
    char* path;
    int flags;
} vita_newlib_descriptor_t;

/* A reserved newlib slot with no native resource to close. Newlib's descriptor
 * release handles unknown types by clearing the slot without an OS close. */
#define VITAARC_DESCRIPTOR_RANDOM 127
extern vita_newlib_descriptor_t* __vita_fdmap[256];
int __vita_acquire_descriptor(void);
int __vita_release_descriptor(int fd);
int _fcntl2sony(int flags);
char* __realpath(const char* path);

vita_newlib_descriptor_t* __vita_fd_grab(int fd);
int __vita_fd_drop(vita_newlib_descriptor_t* descriptor);
int __vita_sce_errno_to_errno(int error, int type);

#endif /* __PLATFORM_NEWLIB_H */
