#ifndef VITAARC_LIBC_FORTIFY_H
#define VITAARC_LIBC_FORTIFY_H

#include <stddef.h>
#include <sys/types.h>

char* bridge_strcpy_chk(char* output, const char* text, size_t size);
ssize_t bridge_read_chk(int fd, void* buffer, size_t count, size_t size);
void bridge_fd_set_chk(int fd, void* set, size_t size);
void bridge_fd_clr_chk(int fd, void* set, size_t size);

#endif
