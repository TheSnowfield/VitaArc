#ifndef _BRIDGE_LIBC_LIBCIMPL_H_
#define _BRIDGE_LIBC_LIBCIMPL_H_

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

void bridgeStackChkFail();

void bridgeCheckFailed();

void bridgeAssert2();

void *bridgeMemmem(const void *haystack, size_t haystacklen,
                   const void *needle, size_t needlelen);

int bridgeMkdir(const char *path, mode_t mode);

_off64_t bridgeLseek64(int fd, _off64_t offset, int whence);

int bridgeLstat(const char *path, struct stat *buf);

int bridgeFsync(int fildes);

int bridgeFtruncate64(int fildes, _off64_t length);

const char *bridgeGaiStrerror(int ecode);

char *bridgeGetcwd(char *buf, size_t size);

uid_t bridgeGeteuid(void);

int bridgeIoctl(int fildes, int request, ...);

int bridgeRmdir(const char *path);

long bridgeSysconf(int name);

intmax_t bridgeStrtoimax(const char *nptr, char **endptr, int base);

uintmax_t bridgeStrtoumax(const char *nptr, char **endptr, int base);

ssize_t bridgeReadlink(const char *path, char *buf, size_t bufsize);

long bridgeSyscall(long number, ...);

int bridgeUtimes(const char *path, const struct timeval times[2]);

void *bridgeMremap(void *old_address, size_t old_size,
                   size_t new_size, int flags, ... /* void *new_address */);

int bridgeMunmap(void *addr, size_t length);

int bridgeNanosleep(const struct timespec *req, struct timespec *rem);

int bridgeFchmod(int fildes, mode_t mode);

int bridgeFchown(int fildes, uid_t owner, gid_t group);

int bridgeClockGettime(clockid_t clock_id, struct timespec *tp);

int bridgeDladdr(void *addr, void *info);

int bridgeDlclose(void *handle);

char *bridgeDlerror(void);

void *bridgeDlopen(const char *filename, int flags);

void *bridgeDlsym(void *handle, const char *symbol);

void *bridgeMmap2(unsigned long addr, unsigned long length,
                  unsigned long prot, unsigned long flags,
                  unsigned long fd, unsigned long pgoffset);

void __fortify_chk_fail(const char *msg, uint32_t event_tag);

uintptr_t bridgeGnuUnwindFindExidx(uintptr_t pc, int *pcount);

FILE *bridgeFopen(const char *_name, const char *_type);

size_t bridgeFread(void *_buf, size_t _size, size_t _n, FILE *_file);

size_t bridgeStrlen(const char *_str);

int bridgeOpen(const char *_device, int _flg, ...);

int bridgeRead(int _fd, void *_buf, size_t _nbyte);

int bridgeClose(int _fd);

#endif /* _BRIDGE_LIBC_LIBCIMPL_H_ */
