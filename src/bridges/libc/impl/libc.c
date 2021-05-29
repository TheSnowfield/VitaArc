#include <string.h>
#include <wchar.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>

#include <psp2/kernel/rng.h>

#include "../../../logcat/logcat.h"
#include "../../../common/define.h"
#include "libc.h"

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
  return NULL;
}

int mkdir(const char *path, mode_t mode)
{
  logW(TAG, "Unsupported symbol 'mkdir' called.");
  return 0;
}

_off64_t lseek64(int fd, _off64_t offset, int whence)
{
  logW(TAG, "Unsupported symbol 'lseek64' called.");
  return 0;
}

int lstat(const char *path, struct stat *buf)
{
  logW(TAG, "Unsupported symbol 'lstat' called.");
  return 0;
}

int fsync(int fildes)
{
  logW(TAG, "Unsupported symbol 'fsync' called.");
  return 0;
}

int ftruncate64(int fildes, _off64_t length)
{
  logW(TAG, "Unsupported symbol 'ftruncate64' called.");
  return 0;
}

const char *gai_strerror(int ecode)
{
  logW(TAG, "Unsupported symbol 'gai_strerror' called.");
  return NULL;
}

char *getcwd(char *buf, size_t size)
{
  logW(TAG, "Unsupported symbol 'getcwd' called.");
  return NULL;
}

uid_t geteuid(void)
{
  logW(TAG, "Unsupported symbol 'geteuid' called.");
  return 0;
}

int ioctl(int fildes, int request, ...)
{
  logW(TAG, "Unsupported symbol 'ioctl' called.");
  return 0;
}

int rmdir(const char *path)
{
  logW(TAG, "Unsupported symbol 'rmdir' called.");
  return 0;
}

long sysconf(int name)
{
  logW(TAG, "Unsupported symbol 'sysconf' called.");
  return 0;
}

intmax_t strtoimax(const char *nptr, char **endptr, int base)
{
  logW(TAG, "Unsupported symbol 'strtoimax' called.");
  return 0;
}

uintmax_t strtoumax(const char *nptr, char **endptr, int base)
{
  logW(TAG, "Unsupported symbol 'strtoumax' called.");
  return 0;
}

ssize_t readlink(const char *path, char *buf, size_t bufsize)
{
  logW(TAG, "Unsupported symbol 'readlink' called.");
  return 0;
}

long syscall(long number, ...)
{
  logW(TAG, "Unsupported symbol 'syscall' called.");
  return 0;
}

int utimes(const char *path, const struct timeval times[2])
{
  logW(TAG, "Unsupported symbol 'utimes' called.");
  return 0;
}

void *mremap(void *old_address, size_t old_size,
             size_t new_size, int flags, ... /* void *new_address */)
{
  logW(TAG, "Unsupported symbol 'mremap' called.");
  return NULL;
}

int munmap(void *addr, size_t length)
{
  logW(TAG, "Unsupported symbol 'munmap' called.");
  return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
  logW(TAG, "Unsupported symbol 'nanosleep' called.");
  return 0;
}

int fchmod(int fildes, mode_t mode)
{
  logW(TAG, "Unsupported symbol 'fchmod' called.");
  return 0;
}

int fchown(int fildes, uid_t owner, gid_t group)
{
  logW(TAG, "Unsupported symbol 'fchown' called.");
  return 0;
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

  return 0;
}

int dladdr(void *addr, void *info)
{
  logW(TAG, "Unsupported symbol 'dladdr' called.");
  return 0;
}

int dlclose(void *handle)
{
  logW(TAG, "Unsupported symbol 'dlclose' called.");
  return 0;
}

char *dlerror(void)
{
  logW(TAG, "Unsupported symbol 'dlerror' called.");
  return NULL;
}

void *dlopen(const char *filename, int flags)
{
  logW(TAG, "Unsupported symbol 'dlopen' called.");
  return NULL;
}

void *dlsym(void *handle, const char *symbol)
{
  logW(TAG, "Unsupported symbol 'dlsym' called.");
  return NULL;
}

void *mmap2(unsigned long addr, unsigned long length,
            unsigned long prot, unsigned long flags,
            unsigned long fd, unsigned long pgoffset)
{
  logW(TAG, "Unsupported symbol 'mmap2' called.");
  return NULL;
}

void __fortify_chk_fail(const char *msg, uint32_t event_tag)
{
  logF("bionic", msg);
  exit(-1);
}

uintptr_t __gnu_Unwind_Find_exidx(uintptr_t pc, int *pcount)
{
  logW(TAG, "called '__gnu_Unwind_Find_exidx(0x%08X, 0x%08X)'.", pc, pcount);
  return 0;
}

FILE *_fopen(const char *_name, const char *_type)
{
  logW(TAG, "Called _fopen(\"%s\", \"%s\")", _name, _type);

  // Redirect /data/data/
  if (strstr(_name, "/data/data/") == _name)
  {
    char szRedirect[256] = {0};
    sprintf(szRedirect, "%s%s", PATH_TO_DATA, _name + 11);

    logI(TAG, "  Redirect fopen path to \"%s\"", szRedirect);
    return fopen(szRedirect, _type);
  }

  return fopen(_name, _type);
}

size_t _fread(void *_buf, size_t _size, size_t _n, FILE *_file)
{
  logV(TAG, "Called _fread(0x%08X, %d, %d, 0x%08X)", _buf, _size, _n, _file);
  return fread(_buf, _size, _n, _file);
}

size_t _strlen(const char *_str)
{
  logV(TAG, "Called _strlen(\"%s\")", _str);
  return strlen(_str);
}

// Unix devices
typedef enum DEVICES
{
  DEV_URANDOM = 1,
};

int _open(const char *_device, int _flg, ...)
{
  logV(TAG, "Called _open(\"%s\", %d)", _device, _flg);

  // prandom number device
  if (strcmp(_device, "/dev/urandom") == 0)
    return DEV_URANDOM;

  return open(_device, _flg);
}

int _read(int _fd, void *_buf, size_t _nbyte)
{
  logV(TAG, "Called _read(%d, 0x%08X, %d)", _fd, _buf, _nbyte);

  switch (_fd)
  {
  case DEV_URANDOM:
    sceKernelGetRandomNumber(_buf, _nbyte);
    return _nbyte;
  }

  return read(_fd, _buf, _nbyte);
}

int _close(int _fd)
{
  logV(TAG, "Called _close(%d)", _fd);

  switch (_fd)
  {
  case DEV_URANDOM:
    return 0;
  }

  return close(_fd);
}
