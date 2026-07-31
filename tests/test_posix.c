#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <config.h>
#include <libc/posix.h>

static void __test_files(void)
{
    assert(mkdir("ux0:vitaarc", 0700) == 0);
    assert(mkdir("ux0:vitaarc/persistent", 0700) == 0);
    assert(mkdir(PATH_TO_DATA, 0700) == 0);
    const char* directory = "/data/data/moe.low.arc/files";
    assert(bridge_mkdir(directory, 0700) == 0);
    struct stat status;
    assert(bridge_lstat(PATH_TO_DATA "files", &status) == 0 && S_ISDIR(status.st_mode));
    errno = 0;
    assert(bridge_mkdir(directory, 0700) == -1 && errno == EEXIST);
    assert(bridge_lstat("/data/user/0/moe.low.arc/files", &status) == 0);
    assert(bridge_lstat("missing", &status) == -1 && errno == ENOENT);
    assert(bridge_lstat(directory, NULL) == -1 && errno == EFAULT);
    char long_path[2048];
    memset(long_path, 'x', sizeof(long_path));
    memcpy(long_path, "/data/data/", 11);
    long_path[sizeof(long_path) - 1] = 0;
    assert(bridge_mkdir(long_path, 0700) == -1 && errno == ENAMETOOLONG);
    assert(bridge_mkdir(NULL, 0700) == -1 && errno == EFAULT);
    int fd = open(PATH_TO_DATA "file", O_CREAT | O_RDWR, 0600);
    assert(fd >= 0);
    assert(write(fd, "abcdef", 6) == 6);
    assert(bridge_fsync(fd) == 0);
    assert(bridge_lseek64(fd, 2, SEEK_SET) == 2);
    char byte;
    assert(read(fd, &byte, 1) == 1 && byte == 'c');
    assert(bridge_ftruncate64(fd, 3) == 0);
    assert(fstat(fd, &status) == 0 && status.st_size == 3);
    assert(bridge_ftruncate64(fd, -1) == -1 && errno == EINVAL);
    int64_t large = INT64_C(5) * 1024 * 1024 * 1024;
    assert(bridge_ftruncate64(fd, large) == 0);
    assert(bridge_lseek64(fd, 0, SEEK_END) == large);
    assert(fstat(fd, &status) == 0 && status.st_size == large);
    assert(bridge_ftruncate64(fd, 3) == 0);
    assert(bridge_lseek64(fd, 0, -1) == -1 && errno == EINVAL);
    assert(bridge_fchmod(fd, 0640) == 0);
    assert(fstat(fd, &status) == 0 && (status.st_mode & 0777) == 0640);
    assert(bridge_fchown(fd, (uid_t)-1, (gid_t)-1) == 0);
    assert(bridge_ioctl(fd, 0x1234) == -1 && errno == ENOTTY);
    close(fd);
    assert(bridge_fsync(fd) == -1 && errno == EBADF);
    assert(bridge_lseek64(fd, 0, SEEK_SET) == -1 && errno == EBADF);
    assert(bridge_ftruncate64(fd, 1) == -1 && errno == EBADF);
    struct timeval times[] = {{1234567890, 0}, {1234567891, 0}};

    assert(bridge_utimes("/data/data/moe.low.arc/file", times) == 0);
    assert(stat(PATH_TO_DATA "file", &status) == 0 && status.st_mtime == 1234567891);
    assert(symlink("file", PATH_TO_DATA "link") == 0);
    char target[8] = {0};

    assert(bridge_readlink("/data/data/moe.low.arc/link", target, sizeof(target)) == 4);
    assert(memcmp(target, "file", 4) == 0);
    assert(bridge_lstat("/data/data/moe.low.arc/link", &status) == 0 && S_ISLNK(status.st_mode));
    assert(bridge_rmdir(directory) == 0);
    assert(bridge_rmdir(directory) == -1 && errno == ENOENT);
}

static void __test_time(void)
{
    struct timespec real, before, after;
    assert(bridge_clock_gettime(0, &real) == 0);
    assert(real.tv_sec > 1500000000 && real.tv_nsec >= 0 && real.tv_nsec < 1000000000);
    assert(bridge_clock_gettime(1, &before) == 0);
    struct timespec delay = {.tv_nsec = 2000000};

    assert(bridge_nanosleep(&delay, NULL) == 0);
    assert(bridge_clock_gettime(1, &after) == 0);
    int64_t elapsed = (after.tv_sec - before.tv_sec) * INT64_C(1000000000) + after.tv_nsec - before.tv_nsec;
    assert(elapsed >= 2000000);
    assert(bridge_clock_gettime(-1, &after) == -1 && errno == EINVAL);
    assert(bridge_clock_gettime(1, NULL) == -1 && errno == EFAULT);
    assert(bridge_nanosleep(NULL, NULL) == -1 && errno == EFAULT);
    delay.tv_sec = -1;
    assert(bridge_nanosleep(&delay, NULL) == -1 && errno == EINVAL);
    delay.tv_sec = 0;
    delay.tv_nsec = 1000000000;
    assert(bridge_nanosleep(&delay, NULL) == -1 && errno == EINVAL);
}

static void __test_queries(void)
{
    assert(bridge_sysconf(0x27) == sysconf(_SC_PAGESIZE));
    assert(bridge_sysconf(0x28) == sysconf(_SC_PAGESIZE));
    assert(bridge_sysconf(0x61) == sysconf(_SC_NPROCESSORS_ONLN));
    assert(bridge_sysconf(-1) == -1 && errno == EINVAL);
    assert(bridge_syscall(-1) == -1 && errno == ENOSYS);
    assert(strcmp(bridge_gai_strerror(2), "Temporary name lookup failure") == 0);
    assert(strcmp(bridge_gai_strerror(-1), "Unknown address lookup error") == 0);
    char* cwd = bridge_getcwd(NULL, 0);
    assert(cwd != NULL);
    free(cwd);
    const char text[] = "xxab\0cdab";
    assert(bridge_memmem(text, sizeof(text), "ab\0c", 4) == text + 2);
    assert(bridge_memmem(text, sizeof(text), "", 0) == text);
    assert(bridge_memmem(text, 2, "abc", 3) == NULL);
    assert(bridge_memmem(text, sizeof(text), "missing", 7) == NULL);
    char* end;
    assert(bridge_strtoimax("-9223372036854775808x", &end, 10) == INT64_MIN && *end == 'x');
    assert(bridge_strtoumax("18446744073709551615!", &end, 10) == UINT64_MAX && *end == '!');
    errno = 0;
    assert(bridge_strtoimax("9223372036854775808", &end, 10) == INT64_MAX && errno == ERANGE);
}

int main(void)
{
    __test_files();
    __test_time();
    __test_queries();
    return 0;
}
