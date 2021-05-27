#ifndef _BRIDGE_LIBC_LIBCIMPL_H_
#define _BRIDGE_LIBC_LIBCIMPL_H_

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <psp2/rtc.h>

extern int *__errno _PARAMS((void));
extern void *__cxa_atexit;
extern void *__cxa_finalize;
static const uint32_t __stack_chk_guard = 0xDEADBEEF;

void __stack_chk_fail();

void __check_failed();

void __assert2();

void *memmem(const void *haystack, size_t haystacklen,
             const void *needle, size_t needlelen);

int mkdir(const char *path, mode_t mode);

_off64_t lseek64(int fd, _off64_t offset, int whence);

int lstat(const char *path, struct stat *buf);

int fsync(int fildes);

int ftruncate64(int fildes, _off64_t length);

const char *gai_strerror(int ecode);

char *getcwd(char *buf, size_t size);

uid_t geteuid(void);

int ioctl(int fildes, int request, ...);

int rmdir(const char *path);

long sysconf(int name);

intmax_t strtoimax(const char *nptr, char **endptr, int base);

uintmax_t strtoumax(const char *nptr, char **endptr, int base);

ssize_t readlink(const char *path, char *buf, size_t bufsize);

long syscall(long number, ...);

int utimes(const char *path, const struct timeval times[2]);

void *mremap(void *old_address, size_t old_size,
             size_t new_size, int flags, ... /* void *new_address */);

int munmap(void *addr, size_t length);

int nanosleep(const struct timespec *req, struct timespec *rem);

int fchmod(int fildes, mode_t mode);

int fchown(int fildes, uid_t owner, gid_t group);

int clock_gettime(clockid_t clock_id, struct timespec *tp);

int dladdr(void *addr, void *info);

int dlclose(void *handle);

char *dlerror(void);

void *dlopen(const char *filename, int flags);

void *dlsym(void *handle, const char *symbol);

void *mmap2(unsigned long addr, unsigned long length,
            unsigned long prot, unsigned long flags,
            unsigned long fd, unsigned long pgoffset);

void __fortify_chk_fail(const char *msg, uint32_t event_tag);

uintptr_t __gnu_Unwind_Find_exidx(uintptr_t pc, int *pcount);

FILE *_fopen(const char *_name, const char *_type);

#endif /* _BRIDGE_LIBC_LIBCIMPL_H_ */
