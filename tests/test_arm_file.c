#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <libc/file.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/io/dirent.h>
#include <psp2/rtc.h>

typedef struct file_api {
    int (*open)(const char*, int, ...);
    ssize_t (*read)(int, void*, size_t);
    ssize_t (*write)(int, const void*, size_t);
    int (*close)(int);
    int (*stat)(const char*, bridge_bionic_stat_t*);
    int (*fstat)(int, bridge_bionic_stat_t*);
    int64_t (*seek)(int, int64_t, int);
    int (*truncate)(int, int64_t);
    int (*fcntl)(int, int, ...);
    int (*sync)(int);
    int* (*error)(void);
    FILE* (*fdopen)(int, const char*);
    int (*rename)(const char*, const char*);
    int (*mkdir)(const char*, unsigned int);
} file_api_t;

static bool __exists;
static int64_t __size, __offset;
static char __bytes[64];
static unsigned __rng_calls, __rng_failure, __file_closes, __syncs;
static int __flags, __mode;
static bool __sync_failure;

typedef struct path_file {
    char name[256];
    bool exists, nonempty;
    unsigned mode;
    char data;
} path_file_t;

static path_file_t __paths[8];
static bool __path_mode, __rollback_failure, __cleanup_failure;
static unsigned __rename_calls, __rename_failure_call, __mkdir_mode, __directory_cursor;
static int __directory_index = -1;

static int __sce_error(int error)
{
    return (int)(0x80010000u | (unsigned int)error);
}

static const char* __path_name(const char* path)
{
    const char* prefix = "vitaarc/persistent/data/";
    const char* match = strstr(path, prefix);
    return match ? match + strlen(prefix) : path;
}

static path_file_t* __path_lookup(const char* path)
{
    const char* name = __path_name(path);
    for(unsigned index = 0; index < sizeof(__paths) / sizeof(__paths[0]); ++index) {
        if(__paths[index].exists && !strcmp(name, __paths[index].name)) {
            return &__paths[index];
        }
    }

    return NULL;
}

static void __path_reset(void)
{
    memset(__paths, 0, sizeof(__paths));
    strcpy(__paths[0].name, "source");
    strcpy(__paths[1].name, "target");
    __paths[0].exists = __paths[1].exists = true;
    __paths[0].mode = __paths[1].mode = SCE_S_IFREG | 0600;
    __paths[0].data = 'n';
    __paths[1].data = 'o';
    __rename_calls = __rename_failure_call = 0;
    __rollback_failure = __cleanup_failure = false;
}

int mock_path_mkdir(const char* path, unsigned int mode)
{
    if(strcmp(__path_name(path), "requested")) {
        return __sce_error(2);
    }

    __mkdir_mode = mode;
    return __mkdir_mode == 0610 ? 0 : __sce_error(13);
}

int mock_path_rename(const char* old_path, const char* new_path)
{
    ++__rename_calls;
    if(__rename_calls == __rename_failure_call) {
        return __sce_error(28);
    }

    if(__rollback_failure && strstr(old_path, ".vitaarc-rename-")) {
        return __sce_error(13);
    }

    path_file_t* source = __path_lookup(old_path);
    if(!source) {
        return __sce_error(2);
    }

    if(__path_lookup(new_path)) {
        return __sce_error(17);
    }

    const char* name = __path_name(new_path);
    if(strlen(name) >= sizeof(source->name)) {
        return __sce_error(36);
    }

    strcpy(source->name, name);
    return 0;
}

int mock_path_remove(const char* path)
{
    if(__cleanup_failure) {
        return __sce_error(13);
    }

    path_file_t* file = __path_lookup(path);
    if(!file) {
        return __sce_error(2);
    }

    if(SCE_S_ISDIR(file->mode)) {
        return __sce_error(21);
    }

    file->exists = false;
    return 0;
}

int mock_path_rmdir(const char* path)
{
    path_file_t* file = __path_lookup(path);
    if(!file) {
        return __sce_error(2);
    }

    if(!SCE_S_ISDIR(file->mode)) {
        return __sce_error(20);
    }

    if(file->nonempty) {
        return __sce_error(39);
    }

    file->exists = false;
    return 0;
}

int mock_path_dopen(const char* path)
{
    path_file_t* file = __path_lookup(path);
    if(!file || !SCE_S_ISDIR(file->mode) || __directory_index != -1) {
        return __sce_error(20);
    }

    __directory_index = (int)(file - __paths);
    __directory_cursor = 0;
    return 222;
}

int mock_path_dread(int uid, SceIoDirent* entry)
{
    if(uid != 222 || __directory_index < 0) {
        return __sce_error(9);
    }

    if(__directory_cursor < 2) {
        strcpy(entry->d_name, __directory_cursor++ ? ".." : ".");
        return 1;
    }

    strcpy(entry->d_name, "child");
    return __paths[__directory_index].nonempty ? 1 : 0;
}

int mock_path_dclose(int uid)
{
    if(uid != 222 || __directory_index < 0) {
        return __sce_error(9);
    }

    __directory_index = -1;
    return 0;
}

int mock_file_open(const char* path, int flags, unsigned int mode)
{
    if(!strstr(path, "vitaarc/persistent/data/file")) {
        return __sce_error(2);
    }

    if(__exists && (flags & (SCE_O_CREAT | SCE_O_EXCL)) == (SCE_O_CREAT | SCE_O_EXCL)) {
        return __sce_error(17);
    }

    if(!__exists && !(flags & SCE_O_CREAT)) {
        return __sce_error(2);
    }

    __exists = true;
    __offset = 0;
    __flags = flags;
    __mode = mode;
    if(flags & SCE_O_TRUNC) {
        __size = 0;
    }

    return 111;
}

int mock_file_close(int uid)
{
    if(uid != 111) {
        return __sce_error(9);
    }

    ++__file_closes;
    return 0;
}

int mock_file_getstat(const char* path, SceIoStat* status)
{
    if(__path_mode) {
        path_file_t* file = __path_lookup(path);
        if(!file) {
            return __sce_error(2);
        }

        memset(status, 0, sizeof(*status));
        status->st_mode = file->mode;
        status->st_size = 1;
        return 0;
    }

    if(!__exists || !strstr(path, "vitaarc/persistent/data/file")) {
        return __sce_error(2);
    }

    memset(status, 0, sizeof(*status));
    status->st_mode = SCE_S_IFREG | 0640;
    status->st_size = __size;
    status->st_atime = (SceDateTime){.year = 2026, .second = 1, .microsecond = 123456};

    status->st_mtime = (SceDateTime){.year = 2026, .second = 2, .microsecond = 654321};

    status->st_ctime = (SceDateTime){.year = 2026, .second = 3};

    return 0;
}

int mock_file_fstat(int uid, SceIoStat* status)
{
    return uid == 111 ? mock_file_getstat("vitaarc/persistent/data/file", status) : __sce_error(9);
}

int mock_file_chstat(int uid, const SceIoStat* status, unsigned int fields)
{
    if(uid != 111 || fields != SCE_CST_SIZE) {
        return __sce_error(22);
    }

    __size = status->st_size;
    return 0;
}

int64_t mock_file_seek(int uid, int64_t offset, int whence)
{
    if(uid != 111) {
        return __sce_error(9);
    }

    if(whence == SEEK_CUR) {
        offset += __offset;
    }

    if(whence == SEEK_END) {
        offset += __size;
    }

    if(offset < 0) {
        return __sce_error(22);
    }

    return __offset = offset;
}

int mock_file_write(int uid, const void* buffer, unsigned int size)
{
    if(uid != 111 || __offset < 0 || __offset + size > sizeof(__bytes)) {
        return __sce_error(22);
    }

    memcpy(__bytes + __offset, buffer, size);
    __offset += size;
    if(__offset > __size) {
        __size = __offset;
    }

    return (int)size;
}

int mock_file_read(int uid, void* buffer, unsigned int size)
{
    if(uid != 111 || __offset < 0 || __offset + size > sizeof(__bytes)) {
        return __sce_error(22);
    }

    if(__offset + size > __size) {
        size = __offset >= __size ? 0 : (unsigned int)(__size - __offset);
    }

    memcpy(buffer, __bytes + __offset, size);
    __offset += size;
    return (int)size;
}

int mock_file_sync(int uid, unsigned int flags)
{
    if(uid != 111 || flags) {
        return __sce_error(22);
    }

    ++__syncs;
    return __sync_failure ? __sce_error(5) : 0;
}

int mock_file_time(const SceDateTime* time, SceUInt64* seconds)
{
    *seconds = UINT64_C(1700000000) + time->second;
    return 0;
}

int mock_file_random(void* buffer, unsigned int size)
{
    if(!size || size > 64) {
        return __sce_error(22);
    }

    if(++__rng_calls == __rng_failure) {
        return __sce_error(5);
    }

    memset(buffer, (int)__rng_calls, size);
    return 0;
}

static bool __original_paths(void)
{
    path_file_t *source = __path_lookup("source"), *target = __path_lookup("target");
    return source && source->data == 'n' && target && target->data == 'o';
}

static bool __retained_backup(void)
{
    for(unsigned index = 0; index < sizeof(__paths) / sizeof(__paths[0]); ++index) {
        if(__paths[index].exists && strstr(__paths[index].name, ".vitaarc-rename-") && __paths[index].data == 'o') {
            return true;
        }
    }

    return false;
}

static int __test_paths(const file_api_t* api)
{
    const char *source = "/data/data/moe.low.arc/source", *target = "/data/user/0/moe.low.arc/target";
    __path_mode = true;
    __path_reset();
    if(api->mkdir("/data/user/0/moe.low.arc/requested", 0100610) || __mkdir_mode != 0610) {
        return 26;
    }

    if(api->mkdir("/data/user/0/moe.low.arc/requested", 0700) != -1 || *api->error() != EACCES) {
        return 27;
    }

    if(api->rename("/data/data/moe.low.arc/missing", target) != -1 || *api->error() != ENOENT || !__original_paths() ||
        __rename_calls) {
        return 28;
    }

    if(api->rename(source, "/data/user/0/moe.low.arc/./source") || !__original_paths() || __rename_calls) {
        return 29;
    }

    if(api->rename(source, target) || __rename_calls != 3 || __path_lookup("source") || !__path_lookup("target") ||
        __path_lookup("target")->data != 'n' || __retained_backup()) {
        return 30;
    }

    __path_reset();
    __rename_failure_call = 3;
    if(api->rename(source, target) != -1 || *api->error() != ENOSPC || !__original_paths() || __rename_calls != 4 ||
        __retained_backup()) {
        return 31;
    }

    __path_reset();
    __rename_failure_call = 3;
    __rollback_failure = true;
    if(api->rename(source, target) != -1 || *api->error() != EIO || !__path_lookup("source") ||
        __path_lookup("target") || !__retained_backup()) {
        return 32;
    }

    __path_reset();
    __cleanup_failure = true;
    if(api->rename(source, target) || __path_lookup("source") || !__path_lookup("target") ||
        __path_lookup("target")->data != 'n' || !__retained_backup()) {
        return 33;
    }

    __path_reset();
    __paths[1].mode = SCE_S_IFDIR | 0700;
    if(api->rename(source, target) != -1 || *api->error() != EISDIR || !__original_paths() || __rename_calls) {
        return 34;
    }

    __paths[0].mode = SCE_S_IFDIR | 0700;
    __paths[1].nonempty = true;
    if(api->rename(source, target) != -1 || *api->error() != ENOTEMPTY || !__original_paths() || __rename_calls) {
        return 35;
    }

    __paths[1].nonempty = false;
    if(api->rename(source, target) || __rename_calls != 3 || __directory_index != -1 || !__path_lookup("target") ||
        !SCE_S_ISDIR(__path_lookup("target")->mode) || __retained_backup()) {
        return 36;
    }

    return 0;
}

int test_arm_file_entry(const file_api_t* api)
{
    const char* path = "/data/data/moe.low.arc/file";
    int fd = api->open(path, BIONIC_O_CREAT | BIONIC_O_EXCL | BIONIC_O_RDWR | BIONIC_O_CLOEXEC, 0640u);
    if(fd < 3 || __flags != (SCE_O_CREAT | SCE_O_EXCL | SCE_O_RDWR) || __mode != 0640) {
        return 1;
    }

    int protected_duplicate = api->fcntl(fd, 1030, 0);
    if(api->fcntl(fd, 1) != 1 || protected_duplicate < 3 || api->fcntl(protected_duplicate, 1) != 1 ||
        api->fcntl(protected_duplicate, 2, 0) || api->fcntl(protected_duplicate, 1) != 0 || api->fcntl(fd, 1) != 1 ||
        api->close(protected_duplicate)) {
        return 24;
    }

    if(api->open(path, BIONIC_O_CREAT | BIONIC_O_EXCL | BIONIC_O_RDWR, 0600u) != -1 || *api->error() != EEXIST) {
        return 2;
    }

    if(api->write(fd, "abc", 3) != 3 || api->seek(fd, 0, SEEK_SET) != 0) {
        return 3;
    }

    char bytes[4] = {0};

    if(api->read(fd, bytes, 3) != 3 || strcmp(bytes, "abc")) {
        return 4;
    }

    struct {
        uint64_t before;
        bridge_bionic_stat_t value;
        uint64_t after;
    } status = {.before = UINT64_C(0x123456789abcdef0), .after = UINT64_C(0xfedcba9876543210)};

    if(api->fstat(fd, &status.value) || status.value.size != 3 || status.value.mode != 0100640 ||
        status.value.modify_seconds != 1700000002 || status.value.modify_nanoseconds != 654321000 ||
        status.before != UINT64_C(0x123456789abcdef0) || status.after != UINT64_C(0xfedcba9876543210)) {
        return 5;
    }

    bridge_bionic_stat_t named;
    if(api->stat("/data/user/0/moe.low.arc/file", &named) || named.inode != status.value.inode) {
        return 6;
    }

    if(api->fcntl(fd, 4, BIONIC_O_APPEND) || api->seek(fd, 0, SEEK_SET) || api->write(fd, "d", 1) != 1 || __size != 4 ||
        memcmp(__bytes, "abcd", 4)) {
        return 20;
    }

    int synchronous = api->open(path, BIONIC_O_WRONLY | BIONIC_O_SYNC);
    if(synchronous < 3 || api->fdopen(synchronous, "w") != NULL || *api->error() != ENOTSUP ||
        api->write(synchronous, "q", 1) != 1 || __syncs != 1) {
        return 21;
    }

    __sync_failure = true;
    if(api->write(synchronous, "r", 1) != -1 || *api->error() != EIO || __syncs != 2) {
        return 22;
    }

    __sync_failure = false;
    if(api->close(synchronous)) {
        return 23;
    }

    unsigned initial_closes = __file_closes;
    int64_t large = INT64_C(5) * 1024 * 1024 * 1024 + 7;
    if(api->truncate(fd, large) || api->seek(fd, 0, SEEK_END) != large || api->fstat(fd, &status.value) ||
        status.value.size != large || status.value.blocks != (uint64_t)(large / 512 + 1)) {
        return 7;
    }

    if(api->fcntl(fd, 4, BIONIC_O_APPEND | BIONIC_O_NONBLOCK) || !(api->fcntl(fd, 3) & BIONIC_O_APPEND) ||
        !(api->fcntl(fd, 3) & BIONIC_O_NONBLOCK)) {
        return 8;
    }

    if(api->sync(fd) || __syncs != 3) {
        return 9;
    }

    int duplicate = api->fcntl(fd, 0, 0);
    if(duplicate < 3 || duplicate == fd || api->fcntl(duplicate, 1) != 0 || api->fcntl(fd, 1) != 1 || api->close(fd) ||
        __file_closes != initial_closes) {
        return 10;
    }

    int first = api->open("/dev/urandom", BIONIC_O_RDONLY | BIONIC_O_CLOEXEC);
    int second = api->open("/dev/urandom", BIONIC_O_RDONLY);
    if(first < 3 || second < 3 || first == second || first == duplicate || second == duplicate) {
        return 11;
    }

    if(api->fcntl(first, 1) != 1 || api->fcntl(second, 1) != 0) {
        return 25;
    }

    unsigned char random[257];
    if(api->read(first, random, sizeof(random)) != sizeof(random) || __rng_calls != 5 || random[256] != 5) {
        return 12;
    }

    __rng_failure = __rng_calls + 2;
    if(api->read(first, random, sizeof(random)) != 64) {
        return 13;
    }

    __rng_failure = __rng_calls + 1;
    if(api->read(first, random, 1) != -1 || *api->error() != EIO) {
        return 14;
    }

    __rng_failure = 0;
    if(api->fstat(first, &named) || named.mode != 0020444 || named.special_device != 0x109) {
        return 15;
    }

    int random_duplicate = api->fcntl(first, 0, 0);
    if(random_duplicate < 3 || api->fcntl(random_duplicate, 1) != 0 || api->close(first) ||
        api->read(random_duplicate, random, 1) != 1) {
        return 16;
    }

    if(api->sync(random_duplicate) != -1 || *api->error() != EINVAL || __syncs != 3) {
        return 17;
    }

    if(api->close(random_duplicate) || api->close(second) || __file_closes != initial_closes) {
        return 18;
    }

    if(api->fstat(duplicate, &named) || named.size != large || api->close(duplicate) ||
        __file_closes != initial_closes + 1) {
        return 19;
    }

    return __test_paths(api);
}
