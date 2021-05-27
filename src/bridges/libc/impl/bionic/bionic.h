#ifndef _BIONIC_H_
#define _BIONIC_H_

#include <stddef.h>

void *__memcpy_chk(void *dest, const void *src,
                   size_t copy_amount, size_t dest_len);

void *__memmove_chk(void *dest, const void *src,
                    size_t len, size_t dest_len);

void *__memset_chk(void *dest, int c, size_t n, size_t dest_len);

char *__strchr_chk(const char *p, int ch, size_t s_len);

size_t __strlen_chk(const char *s, size_t s_len);

int __vsnprintf_chk(
    char *dest,
    size_t supplied_size,
    int /*flags*/,
    size_t dest_len_from_compiler,
    const char *format,
    va_list va);

int __vsprintf_chk(
    char *dest,
    int /*flags*/,
    size_t dest_len_from_compiler,
    const char *format,
    va_list va);

#define O_LARGEFILE 0

#endif /* _BIONIC_H_ */
