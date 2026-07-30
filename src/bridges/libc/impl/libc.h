#ifndef __BRIDGES_LIBC_IMPL_LIBC_H
#define __BRIDGES_LIBC_IMPL_LIBC_H

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <psp2/rtc.h>

extern int *__errno(void);
extern void *__cxa_atexit;
extern void *__cxa_finalize;
static const uint32_t __stack_chk_guard = 0xDEADBEEF;

void bridge_stack_chk_fail();

void bridge_check_failed();

void bridge_assert2();

void *bridge_memmem(const void *haystack, size_t haystacklen,
                   const void *needle, size_t needlelen);

int bridge_mkdir(const char *path, mode_t mode);

_off64_t bridge_lseek64(int fd, _off64_t offset, int whence);

int bridge_lstat(const char *path, struct stat *buf);

int bridge_fsync(int fildes);

int bridge_ftruncate64(int fildes, _off64_t length);

const char *bridge_gai_strerror(int ecode);

char *bridge_getcwd(char *buf, size_t size);

uid_t bridge_geteuid(void);

int bridge_ioctl(int fildes, int request, ...);

int bridge_rmdir(const char *path);

long bridge_sysconf(int name);

intmax_t bridge_strtoimax(const char *nptr, char **endptr, int base);

uintmax_t bridge_strtoumax(const char *nptr, char **endptr, int base);

ssize_t bridge_readlink(const char *path, char *buf, size_t bufsize);

long bridge_syscall(long number, ...);

int bridge_utimes(const char *path, const struct timeval times[2]);

void *bridge_mremap(void *old_address, size_t old_size,
                   size_t new_size, int flags, ... /* void *new_address */);

int bridge_munmap(void *addr, size_t length);

int bridge_nanosleep(const struct timespec *req, struct timespec *rem);

int bridge_fchmod(int fildes, mode_t mode);

int bridge_fchown(int fildes, uid_t owner, gid_t group);

int bridge_clock_gettime(clockid_t clock_id, struct timespec *tp);

int bridge_dladdr(void *addr, void *info);

int bridge_dlclose(void *handle);

char *bridge_dlerror(void);

void *bridge_dlopen(const char *filename, int flags);

void *bridge_dlsym(void *handle, const char *symbol);

void *bridge_mmap2(unsigned long addr, unsigned long length,
                  unsigned long prot, unsigned long flags,
                  unsigned long fd, unsigned long pgoffset);

void __fortify_chk_fail(const char *msg, uint32_t event_tag);

uintptr_t bridge_gnu_unwind_find_exidx(uintptr_t pc, int *pcount);

FILE *bridge_fopen(const char *_name, const char *_type);

size_t bridge_fread(void *_buf, size_t _size, size_t _n, FILE *_file);

size_t bridge_strlen(const char *_str);

int bridge_open(const char *_device, int _flg, ...);

int bridge_read(int _fd, void *_buf, size_t _nbyte);

int bridge_close(int _fd);

#endif /* __BRIDGES_LIBC_IMPL_LIBC_H */
