#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <config.h>
#include <jni/preferences.h>
#include <libc/posix.h>
#include <logcat/logcat.h>

typedef struct directory_t {
    const char* path;
    mode_t mode;
} directory_t;

static directory_t __paths[] = {
    {"ux0:data", S_IFDIR},
    {"ux0:data/vitaarc", S_IFDIR},
    {"ux0:data/vitaarc/persistent", S_IFDIR},
    {"ux0:data/vitaarc/persistent/data", S_IFDIR},
    {"ux0:data/vitaarc/persistent/data/files", 0},
};

static unsigned __creates;
static int __create_error;
static int __stat_error;
static char __error[256];

int __wrap_stat(const char* path, struct stat* status)
{
    for(unsigned index = 0; index < sizeof(__paths) / sizeof(__paths[0]); ++index) {
        directory_t* entry = &__paths[index];
        if(strcmp(path, entry->path)) {
            continue;
        }

        if(!entry->mode || __stat_error) {
            errno = __stat_error ? __stat_error : ENOENT;
            return -1;
        }

        memset(status, 0, sizeof(*status));
        status->st_mode = entry->mode;
        return 0;
    }

    assert(0);
    return -1;
}

int bridge_mkdir(const char* path, mode_t mode)
{
    /* A visible ancestor may still reject attempts to create it again. */
    ++__creates;
    assert(strcmp(path, __paths[4].path) == 0 && mode == 0777);
    if(__create_error) {
        if(__create_error == EEXIST) {
            __paths[4].mode = S_IFDIR;
        }

        errno = __create_error;
        return -1;
    }

    __paths[4].mode = S_IFDIR;
    return 0;
}

int bridge_rename(const char* source, const char* destination)
{
    return rename(source, destination);
}

void log_e(const char* tag, const char* format, ...)
{
    (void)tag;
    va_list args;
    va_start(args, format);
    vsnprintf(__error, sizeof(__error), format, args);
    va_end(args);
}

int main(void)
{
    assert(cocos_prepare_data_directory() && __creates == 0);
    assert(cocos_prepare_files_directory() && __creates == 1);
    assert(cocos_prepare_files_directory() && __creates == 1);

    __paths[4].mode = 0;
    __create_error = EEXIST;
    assert(cocos_prepare_files_directory() && __creates == 2);

    __paths[4].mode = 0;
    __create_error = EACCES;
    assert(!cocos_prepare_files_directory() && errno == EACCES);
    assert(strstr(__error, "mkdir failed: " PATH_TO_DATA "files"));

    __stat_error = EACCES;
    assert(!cocos_prepare_files_directory() && errno == EACCES);
    assert(strstr(__error, "stat failed: ux0:data "));

    __stat_error = 0;
    __paths[4].mode = S_IFREG;
    assert(!cocos_prepare_files_directory() && errno == ENOTDIR);
    return 0;
}
