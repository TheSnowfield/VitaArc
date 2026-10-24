#ifndef __BRIDGES_LIBC_IMPL_POSIX_H
#define __BRIDGES_LIBC_IMPL_POSIX_H

#include <inttypes.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

const char* bridge_resolve_path(const char* path, char* buffer, size_t capacity);
void* bridge_memmem(const void* haystack, size_t haystack_length, const void* needle, size_t needle_length);
int bridge_mkdir(const char* path, mode_t mode);
int64_t bridge_lseek64(int fd, int64_t offset, int whence);
int bridge_lstat(const char* path, struct stat* status);
int bridge_fsync(int fd);
int bridge_ftruncate64(int fd, int64_t length);
const char* bridge_gai_strerror(int code);
char* bridge_getcwd(char* buffer, size_t size);
uid_t bridge_geteuid(void);
int bridge_ioctl(int fd, int request, ...);
int bridge_rmdir(const char* path);
long bridge_sysconf(int name);
intmax_t bridge_strtoimax(const char* text, char** end, int base);
uintmax_t bridge_strtoumax(const char* text, char** end, int base);
ssize_t bridge_readlink(const char* path, char* buffer, size_t size);
long bridge_syscall(long number, ...);
int bridge_utimes(const char* path, const struct timeval times[2]);
int bridge_nanosleep(const struct timespec* request, struct timespec* remaining);
int bridge_fchmod(int fd, mode_t mode);
int bridge_fchown(int fd, uid_t owner, gid_t group);
int bridge_clock_gettime(clockid_t clock_id, struct timespec* time);
unsigned bridge_if_nametoindex(const char* name);
int bridge_getpwuid_r(uid_t uid, void* entry, char* buffer, size_t size, void** result);

#endif /* __BRIDGES_LIBC_IMPL_POSIX_H */
