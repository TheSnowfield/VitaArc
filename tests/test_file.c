#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <config.h>
#include <libc/file.h>
#include "paths.h"
#include <libc/posix.h>

static void __test_flags_and_paths(void)
{
    test_dirs(PATH_TO_USER "persistent/");
    assert(mkdir(PATH_TO_DATA, 0700) == 0);
    assert(bridge_access("/data/data/moe.low.arc", F_OK) == 0);
    bridge_bionic_stat_t status;
    assert(bridge_bionic_stat("/data/user/0/moe.low.arc", &status) == 0 && (status.mode & 0170000) == 0040000);
    mode_t old_mask = umask(0);
    const char* path = "/data/data/moe.low.arc/file";
    int fd = bridge_open(path, BIONIC_O_CREAT | BIONIC_O_EXCL | BIONIC_O_RDWR | BIONIC_O_CLOEXEC, 0640u);
    umask(old_mask);
    assert(fd >= 0);
    assert(bridge_fcntl(fd, 1) == 1);
    int protected_duplicate = bridge_fcntl(fd, 1030, 0);
    assert(protected_duplicate >= 0 && bridge_fcntl(protected_duplicate, 1) == 1);
    assert(bridge_close(protected_duplicate) == 0);
    assert(bridge_bionic_fstat(fd, &status) == 0 && (status.mode & 0777) == 0640);
    assert(bridge_write(fd, "abc", 3) == 3);
    assert(bridge_open(path, BIONIC_O_CREAT | BIONIC_O_EXCL | BIONIC_O_RDWR, 0777u) == -1 && errno == EEXIST);
    assert(bridge_close(fd) == 0);
    fd = bridge_open(path, BIONIC_O_WRONLY | BIONIC_O_APPEND);
    assert(fd >= 0);
    assert(bridge_fcntl(fd, 1) == 0);
    assert(bridge_fcntl(fd, 3) & BIONIC_O_APPEND);
    assert(bridge_lseek(fd, 0, SEEK_SET) == 0);
    assert(bridge_write(fd, "def", 3) == 3);
    assert(bridge_close(fd) == 0);
    FILE* file = bridge_fopen("/data/user/0/moe.low.arc/file", "rb");
    char contents[8] = {0};

    assert(file && fread(contents, 1, 6, file) == 6 && strcmp(contents, "abcdef") == 0);
    assert(bridge_fclose(file) == 0);
    fd = bridge_open(path, BIONIC_O_RDWR | BIONIC_O_TRUNC);
    assert(fd >= 0 && bridge_bionic_fstat(fd, &status) == 0 && status.size == 0);
    const bridge_io_vector_t vectors[] = {{"pay", 3}, {"load", 4}};

    assert(bridge_writev(fd, vectors, 2) == 7);
    const bridge_io_vector_t excessive[] = {{"x", INT32_MAX}, {"y", 1}};

    assert(bridge_writev(fd, excessive, 2) == -1 && errno == EINVAL);
    assert(bridge_writev(fd, NULL, 1) == -1 && errno == EFAULT);
    assert(bridge_writev(fd, NULL, 0) == 0);
    int duplicate = bridge_fcntl(fd, 0, 0);
    assert(duplicate >= 0 && duplicate != fd);
    assert(bridge_fcntl(fd, 2, 1) == 0 && bridge_fcntl(fd, 1) == 1);
    assert(bridge_fcntl(duplicate, 1) == 0);
    assert(bridge_lseek(duplicate, 2, SEEK_SET) == 2);
    assert(bridge_lseek(fd, 0, SEEK_CUR) == 2);
    assert(bridge_fcntl(fd, 4, BIONIC_O_RDWR | BIONIC_O_NONBLOCK) == 0);
    assert(bridge_fcntl(duplicate, 3) & BIONIC_O_NONBLOCK);
    assert(bridge_close(duplicate) == 0);
    file = bridge_fdopen(fd, "rb");
    assert(file && fread(contents, 1, 5, file) == 5 && memcmp(contents, "yload", 5) == 0);
    assert(bridge_fclose(file) == 0);
    assert(bridge_open_2(path, BIONIC_O_CREAT) == -1 && errno == EINVAL);
    assert(bridge_open(path, 3) == -1 && errno == EINVAL);
    assert(bridge_open(path, 0x40000000) == -1 && errno == EINVAL);
    assert(bridge_open(NULL, 0) == -1 && errno == EFAULT);
    assert(bridge_fopen(NULL, "r") == NULL && errno == EFAULT);
    assert(bridge_fopen(path, NULL) == NULL && errno == EINVAL);
    assert(bridge_read(-1, contents, 1) == -1 && errno == EBADF);
    assert(bridge_write(-1, contents, 1) == -1 && errno == EBADF);
    assert(bridge_read(-1, NULL, 1) == -1 && errno == EFAULT);
    assert(bridge_read(-1, contents, (size_t)INT32_MAX + 1) == -1 && errno == EINVAL);
    assert(bridge_rename(path, "/data/user/0/moe.low.arc/renamed") == 0);
    assert(bridge_access(path, F_OK) == -1 && errno == ENOENT);
    assert(bridge_remove("/data/data/moe.low.arc/renamed") == 0);
    fd = bridge_open(path, BIONIC_O_CREAT | BIONIC_O_RDWR, 0600u);
    assert(fd >= 0 && bridge_close(fd) == 0);
    assert(bridge_unlink(path) == 0);
    assert(bridge_unlink(path) == -1 && errno == ENOENT);
}

static void __test_stat_layout(void)
{
    const char* path = "/data/data/moe.low.arc/sparse";
    int fd = bridge_open(path, BIONIC_O_CREAT | BIONIC_O_RDWR, 0600u);
    assert(fd >= 0);
    int64_t length = INT64_C(5) * 1024 * 1024 * 1024 + 7;
    assert(bridge_ftruncate64(fd, length) == 0);
    struct {
        uint64_t before;
        bridge_bionic_stat_t status;
        uint64_t after;
    } guarded = {.before = UINT64_C(0x123456789abcdef0), .after = UINT64_C(0xfedcba9876543210)};

    assert(bridge_bionic_fstat(fd, &guarded.status) == 0);
    assert(guarded.before == UINT64_C(0x123456789abcdef0) && guarded.after == UINT64_C(0xfedcba9876543210));
    assert(sizeof(guarded.status) == 104 && guarded.status.size == length);
    struct stat native;
    assert(fstat(fd, &native) == 0);
    assert(guarded.status.inode == native.st_ino && guarded.status.legacy_inode == (uint32_t)native.st_ino);
    assert(guarded.status.device == native.st_dev && guarded.status.blocks == (uint64_t)native.st_blocks);
    assert(guarded.status.mode == native.st_mode && guarded.status.links == native.st_nlink);
    assert(guarded.status.modify_seconds == (uint32_t)native.st_mtime);
    bridge_bionic_stat_t named;
    assert(bridge_bionic_stat(path, &named) == 0 && named.inode == guarded.status.inode && named.size == length);
    assert(bridge_lseek64(fd, length, SEEK_SET) == length);
    assert(bridge_lseek(fd, 0, SEEK_CUR) == -1 && errno == EOVERFLOW);
    assert(bridge_ftruncate(fd, 3) == 0 && bridge_bionic_fstat(fd, &named) == 0 && named.size == 3);
    assert(bridge_bionic_stat(path, NULL) == -1 && errno == EFAULT);
    assert(bridge_bionic_fstat(fd, NULL) == -1 && errno == EFAULT);
    memset(&named, 0xa5, sizeof(named));
    bridge_bionic_stat_t previous = named;
    assert(bridge_bionic_stat("missing", &named) == -1 && memcmp(&named, &previous, sizeof(named)) == 0);
    assert(symlink("sparse", PATH_TO_DATA "link") == 0);
    assert(bridge_bionic_lstat("/data/data/moe.low.arc/link", &named) == 0 && (named.mode & 0170000) == 0120000);
    assert(bridge_bionic_stat("/data/data/moe.low.arc/link", &named) == 0 && named.size == 3);
    assert(bridge_open("/data/data/moe.low.arc/link", BIONIC_O_NOFOLLOW) == -1 && errno == ELOOP);
    assert(bridge_close(fd) == 0);
    assert(bridge_bionic_fstat(fd, &named) == -1 && errno == EBADF);
    assert(bridge_unlink("/data/data/moe.low.arc/link") == 0);
    assert(bridge_unlink(path) == 0);
}

static void __test_random_descriptors(void)
{
    int regular = bridge_open("/data/data/moe.low.arc/regular", BIONIC_O_CREAT | BIONIC_O_RDWR, 0600u);
    int first = bridge_open("/dev/urandom", BIONIC_O_RDONLY);
    int second = bridge_open("/dev/urandom", BIONIC_O_RDONLY);
    assert(regular >= 0 && first >= 0 && second >= 0 && first != second && first != regular && second != regular);
    assert(first != 1 && second != 1);
    unsigned char bytes[257];
    assert(bridge_read(first, bytes, sizeof(bytes)) == sizeof(bytes));
    bridge_bionic_stat_t status;
    assert(bridge_bionic_fstat(first, &status) == 0 && (status.mode & 0170000) == 0020000);
    assert(bridge_close(first) == 0);
    assert(bridge_read(second, bytes, sizeof(bytes)) == sizeof(bytes));
    assert(bridge_write(regular, "still open", 10) == 10);
    assert(bridge_close(second) == 0 && bridge_close(regular) == 0);
    assert(bridge_unlink("/data/data/moe.low.arc/regular") == 0);
}

static void __expect_file(const char* path, const char* contents)
{
    FILE* file = bridge_fopen(path, "rb");
    char bytes[32] = {0};

    assert(file && fread(bytes, 1, sizeof(bytes) - 1, file) == strlen(contents));
    assert(!strcmp(bytes, contents) && bridge_fclose(file) == 0);
}

static void __test_replacement(void)
{
    const char *source = "/data/data/moe.low.arc/source", *target = "/data/user/0/moe.low.arc/target";
    int old = bridge_open(target, BIONIC_O_CREAT | BIONIC_O_RDWR, 0600u);
    int fresh = bridge_open(source, BIONIC_O_CREAT | BIONIC_O_RDWR, 0600u);
    assert(old >= 0 && fresh >= 0);
    assert(bridge_write(old, "old", 3) == 3 && bridge_write(fresh, "new", 3) == 3);
    assert(bridge_close(fresh) == 0);
    assert(bridge_rename("missing", target) == -1 && errno == ENOENT);
    assert(bridge_rename("missing", "missing") == -1 && errno == ENOENT);
    __expect_file(target, "old");
    assert(bridge_rename(source, source) == 0);
    assert(bridge_rename(source, target) == 0);
    __expect_file(target, "new");
    assert(bridge_access(source, F_OK) == -1 && errno == ENOENT);
    /* Native host replacement preserves the old open file description. */
    char bytes[4] = {0};

    assert(bridge_lseek(old, 0, SEEK_SET) == 0 && bridge_read(old, bytes, 3) == 3 && !strcmp(bytes, "old"));
    assert(bridge_close(old) == 0 && bridge_unlink(target) == 0);

    assert(bridge_mkdir(source, 0700) == 0 && bridge_mkdir(target, 0700) == 0);
    fresh = bridge_open("/data/user/0/moe.low.arc/target/child", BIONIC_O_CREAT | BIONIC_O_RDWR, 0600u);
    assert(fresh >= 0 && bridge_close(fresh) == 0);
    assert(bridge_rename(source, target) == -1 && (errno == ENOTEMPTY || errno == EEXIST));
    assert(bridge_access(source, F_OK) == 0);
    assert(bridge_unlink("/data/user/0/moe.low.arc/target/child") == 0);
    assert(bridge_rename(source, target) == 0 && bridge_rmdir(target) == 0);
}

int main(void)
{
    __test_flags_and_paths();
    __test_stat_layout();
    __test_random_descriptors();
    __test_replacement();
    puts(
        "File ABI passed: ARM stat layout, flags/modes, path aliases, 64-bit files and independent random descriptors");
    return 0;
}
