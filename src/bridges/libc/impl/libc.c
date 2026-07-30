#include <string.h>
#include <wchar.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>

#include <psp2/kernel/rng.h>

#include "../../../logcat/logcat.h"
#include <config.h>
#include "libc.h"

void bridge_stack_chk_fail()
{
  log_f(TAG, "__stack_chk_fail hited. Program exit.");
  exit(-1);
}

void bridge_check_failed()
{
  log_f(TAG, "__check_failed hited. Program exit.");
  exit(-1);
}

void bridge_assert2()
{
  log_f(TAG, "__assert2 hited. Program exit.");
  exit(-1);
}

void *bridge_memmem(const void *haystack, size_t haystacklen,
                   const void *needle, size_t needlelen)
{
  log_w(TAG, "Unsupported symbol 'memmem' called.");
  return NULL;
}

int bridge_mkdir(const char *path, mode_t mode)
{
  log_w(TAG, "Unsupported symbol 'mkdir' called.");
  return 0;
}

_off64_t bridge_lseek64(int fd, _off64_t offset, int whence)
{
  log_w(TAG, "Unsupported symbol 'lseek64' called.");
  return 0;
}

int bridge_lstat(const char *path, struct stat *buf)
{
  log_w(TAG, "Unsupported symbol 'lstat' called.");
  return 0;
}

int bridge_fsync(int fildes)
{
  log_w(TAG, "Unsupported symbol 'fsync' called.");
  return 0;
}

int bridge_ftruncate64(int fildes, _off64_t length)
{
  log_w(TAG, "Unsupported symbol 'ftruncate64' called.");
  return 0;
}

const char *bridge_gai_strerror(int ecode)
{
  log_w(TAG, "Unsupported symbol 'gai_strerror' called.");
  return NULL;
}

char *bridge_getcwd(char *buf, size_t size)
{
  log_w(TAG, "Unsupported symbol 'getcwd' called.");
  return NULL;
}

uid_t bridge_geteuid(void)
{
  log_w(TAG, "Unsupported symbol 'geteuid' called.");
  return 0;
}

int bridge_ioctl(int fildes, int request, ...)
{
  log_w(TAG, "Unsupported symbol 'ioctl' called.");
  return 0;
}

int bridge_rmdir(const char *path)
{
  log_w(TAG, "Unsupported symbol 'rmdir' called.");
  return 0;
}

long bridge_sysconf(int name)
{
  log_w(TAG, "Unsupported symbol 'sysconf' called.");
  return 0;
}

intmax_t bridge_strtoimax(const char *nptr, char **endptr, int base)
{
  log_w(TAG, "Unsupported symbol 'strtoimax' called.");
  return 0;
}

uintmax_t bridge_strtoumax(const char *nptr, char **endptr, int base)
{
  log_w(TAG, "Unsupported symbol 'strtoumax' called.");
  return 0;
}

ssize_t bridge_readlink(const char *path, char *buf, size_t bufsize)
{
  log_w(TAG, "Unsupported symbol 'readlink' called.");
  return 0;
}

long bridge_syscall(long number, ...)
{
  log_w(TAG, "Unsupported symbol 'syscall' called.");
  return 0;
}

int bridge_utimes(const char *path, const struct timeval times[2])
{
  log_w(TAG, "Unsupported symbol 'utimes' called.");
  return 0;
}

void *bridge_mremap(void *old_address, size_t old_size,
                   size_t new_size, int flags, ... /* void *new_address */)
{
  log_w(TAG, "Unsupported symbol 'mremap' called.");
  return NULL;
}

int bridge_munmap(void *addr, size_t length)
{
  log_w(TAG, "Unsupported symbol 'munmap' called.");
  return 0;
}

int bridge_nanosleep(const struct timespec *req, struct timespec *rem)
{
  log_w(TAG, "Unsupported symbol 'nanosleep' called.");
  return 0;
}

int bridge_fchmod(int fildes, mode_t mode)
{
  log_w(TAG, "Unsupported symbol 'fchmod' called.");
  return 0;
}

int bridge_fchown(int fildes, uid_t owner, gid_t group)
{
  log_w(TAG, "Unsupported symbol 'fchown' called.");
  return 0;
}

int bridge_clock_gettime(clockid_t clock_id, struct timespec *tp)
{
  log_v(TAG, "Called clock_gettime(%d, 0x%08X)", clock_id, tp);

  SceRtcTick rtc_tick;
  sceRtcGetCurrentTick(&rtc_tick);
  {
    tp->tv_nsec = 0;
    tp->tv_sec = rtc_tick.tick;
    log_v(TAG, "    Tick: %ld", rtc_tick.tick);
  }

  return 0;
}

int bridge_dladdr(void *addr, void *info)
{
  log_w(TAG, "Unsupported symbol 'dladdr' called.");
  return 0;
}

int bridge_dlclose(void *handle)
{
  log_w(TAG, "Unsupported symbol 'dlclose' called.");
  return 0;
}

char *bridge_dlerror(void)
{
  log_w(TAG, "Unsupported symbol 'dlerror' called.");
  return NULL;
}

void *bridge_dlopen(const char *filename, int flags)
{
  log_w(TAG, "Unsupported symbol 'dlopen' called.");
  return NULL;
}

void *bridge_dlsym(void *handle, const char *symbol)
{
  log_w(TAG, "Unsupported symbol 'dlsym' called.");
  return NULL;
}

void *bridge_mmap2(unsigned long addr, unsigned long length,
                  unsigned long prot, unsigned long flags,
                  unsigned long fd, unsigned long pgoffset)
{
  log_w(TAG, "Unsupported symbol 'mmap2' called.");
  return NULL;
}

void __fortify_chk_fail(const char *msg, uint32_t event_tag)
{
  log_f("bionic", msg);
  exit(-1);
}

uintptr_t bridge_gnu_unwind_find_exidx(uintptr_t pc, int *pcount)
{
  log_w(TAG, "called '__gnu_Unwind_Find_exidx(0x%08X, 0x%08X)'.", pc, pcount);
  return 0;
}

FILE *bridge_fopen(const char *_name, const char *_type)
{
  log_w(TAG, "Called _fopen(\"%s\", \"%s\")", _name, _type);

  // Redirect /data/data/
  if (strstr(_name, "/data/data/") == _name)
  {
    char redirect_path[256] = {0};
    sprintf(redirect_path, "%s%s", PATH_TO_DATA, _name + 11);

    log_i(TAG, "  Redirect fopen path to \"%s\"", redirect_path);
    return fopen(redirect_path, _type);
  }

  return fopen(_name, _type);
}

size_t bridge_fread(void *_buf, size_t _size, size_t _n, FILE *_file)
{
  log_v(TAG, "Called _fread(0x%08X, %d, %d, 0x%08X)", _buf, _size, _n, _file);
  return fread(_buf, _size, _n, _file);
}

size_t bridge_strlen(const char *_str)
{
  log_v(TAG, "Called _strlen(\"%s\")", _str);
  return strlen(_str);
}

// Unix devices
enum DEVICES
{
  DEV_URANDOM = 1,
};

int bridge_open(const char *_device, int _flg, ...)
{
  log_v(TAG, "Called _open(\"%s\", %d)", _device, _flg);

  // prandom number device
  if (strcmp(_device, "/dev/urandom") == 0)
    return DEV_URANDOM;

  return open(_device, _flg);
}

int bridge_read(int _fd, void *_buf, size_t _nbyte)
{
  log_v(TAG, "Called _read(%d, 0x%08X, %d)", _fd, _buf, _nbyte);

  switch (_fd)
  {
  case DEV_URANDOM:
    sceKernelGetRandomNumber(_buf, _nbyte);
    return _nbyte;
  }

  return read(_fd, _buf, _nbyte);
}

int bridge_close(int _fd)
{
  log_v(TAG, "Called _close(%d)", _fd);

  switch (_fd)
  {
  case DEV_URANDOM:
    return 0;
  }

  return close(_fd);
}
