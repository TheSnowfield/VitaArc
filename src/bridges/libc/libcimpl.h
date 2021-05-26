#ifndef _BRIDGE_LIBC_IMPL_H_
#define _BRIDGE_LIBC_IMPL_H_

#include <stddef.h>
#include <sys/unistd.h>
#include <logcat/logcat.h>
#include <psp2/rtc.h>

extern int *__errno _PARAMS((void));
extern void *__cxa_atexit;
extern void *__cxa_finalize;
static const uint32_t __stack_chk_guard = 0xDEADBEEF;

void __stack_chk_fail()
{
  logF(TAG, "__stack_chk_fail hited. Program exit.");
  exit(-1);
}

void __check_failed()
{
  logF(TAG, "__check_failed hited. Program exit.");
  exit(-1);
}

void __assert2()
{
  logF(TAG, "__assert2 hited. Program exit.");
  exit(-1);
}

void *memmem(const void *haystack, size_t haystacklen,
             const void *needle, size_t needlelen)
{
  logW(TAG, "Unsupported symbol 'memmem' called.");
}

int mkdir(const char *path, mode_t mode)
{
  logW(TAG, "Unsupported symbol 'mkdir' called.");
}

_off64_t lseek64(int fd, _off64_t offset, int whence)
{
  logW(TAG, "Unsupported symbol 'lseek64' called.");
}

int lstat(const char *restrict path, struct stat *restrict buf)
{
  logW(TAG, "Unsupported symbol 'lstat' called.");
}

int fsync(int fildes)
{
  logW(TAG, "Unsupported symbol 'fsync' called.");
}

int ftruncate64(int fildes, _off64_t length)
{
  logW(TAG, "Unsupported symbol 'ftruncate64' called.");
}

const char *gai_strerror(int ecode)
{
  logW(TAG, "Unsupported symbol 'gai_strerror' called.");
}

char *getcwd(char *buf, size_t size)
{
  logW(TAG, "Unsupported symbol 'getcwd' called.");
}

uid_t geteuid(void)
{
  logW(TAG, "Unsupported symbol 'geteuid' called.");
}

int ioctl(int fildes, int request, ...)
{
  logW(TAG, "Unsupported symbol 'ioctl' called.");
}

int rmdir(const char *path)
{
  logW(TAG, "Unsupported symbol 'rmdir' called.");
}

long sysconf(int name)
{
  logW(TAG, "Unsupported symbol 'sysconf' called.");
}

intmax_t strtoimax(const char *nptr, char **endptr, int base)
{
  logW(TAG, "Unsupported symbol 'strtoimax' called.");
}

uintmax_t strtoumax(const char *nptr, char **endptr, int base)
{
  logW(TAG, "Unsupported symbol 'strtoumax' called.");
}

ssize_t readlink(const char *restrict path,
                 char *restrict buf, size_t bufsize)
{
  logW(TAG, "Unsupported symbol 'readlink' called.");
}

long syscall(long number, ...)
{
  logW(TAG, "Unsupported symbol 'syscall' called.");
}

int utimes(const char *path, const struct timeval times[2])
{
  logW(TAG, "Unsupported symbol 'utimes' called.");
}

void *mremap(void *old_address, size_t old_size,
             size_t new_size, int flags, ... /* void *new_address */)
{
  logW(TAG, "Unsupported symbol 'mremap' called.");
}

int munmap(void *addr, size_t length)
{
  logW(TAG, "Unsupported symbol 'munmap' called.");
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
  logW(TAG, "Unsupported symbol 'nanosleep' called.");
}

int fchmod(int fildes, mode_t mode)
{
  logW(TAG, "Unsupported symbol 'fchmod' called.");
}

int fchown(int fildes, uid_t owner, gid_t group)
{
  logW(TAG, "Unsupported symbol 'fchown' called.");
}

int clock_gettime(clockid_t clock_id, struct timespec *tp)
{
  logV(TAG, "Called clock_gettime(%d, 0x%08X)", clock_id, tp);

  SceRtcTick sRtcTick;
  sceRtcGetCurrentTick(&sRtcTick);
  {
    tp->tv_nsec = 0;
    tp->tv_sec = sRtcTick.tick;
    logV(TAG, "    Tick: %ld", sRtcTick.tick);
  }
}

int dladdr(void *addr, void *info)
{
  logW(TAG, "Unsupported symbol 'dladdr' called.");
}

int dlclose(void *handle)
{
  logW(TAG, "Unsupported symbol 'dlclose' called.");
}

char *dlerror(void)
{
  logW(TAG, "Unsupported symbol 'dlerror' called.");
}

void *dlopen(const char *filename, int flags)
{
  logW(TAG, "Unsupported symbol 'dlopen' called.");
}

void *dlsym(void *restrict handle, const char *restrict symbol)
{
  logW(TAG, "Unsupported symbol 'dlsym' called.");
}

void *mmap2(unsigned long addr, unsigned long length,
            unsigned long prot, unsigned long flags,
            unsigned long fd, unsigned long pgoffset)
{
  logW(TAG, "Unsupported symbol 'mmap2' called.");
}

void __fortify_chk_fail(const char *msg, uint32_t event_tag)
{
  logF("bionic", msg);
  exit(-1);
}

#endif /* _BRIDGE_LIBC_IMPL_H_ */
