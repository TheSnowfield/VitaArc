#ifndef VITAARC_PLATFORM_IO_H
#define VITAARC_PLATFORM_IO_H

#include <psp2/io/stat.h>

/* Unsafe executables must retain system RW access. Vita ignores POSIX group
 * permissions and does not support its set-ID or sticky mode bits. */
static inline unsigned vita_io_mode(unsigned mode)
{
    return (mode & 0777) | SCE_S_IRSYS | SCE_S_IWSYS;
}

#endif
