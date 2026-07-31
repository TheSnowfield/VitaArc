#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/io/stat.h>
#include <platform/newlib.h>
#include <libc/posix.h>

static vita_newlib_descriptor_t __descriptor = {.uid = 91, .flags = O_RDWR};

static int __references;
static int __error;
static int64_t __offset;
static int64_t __length;
static uint64_t __microseconds;
static unsigned int __delays;
static uint64_t __clock = UINT64_C(9876543210123);
static uint64_t __failure_elapsed;
static uint64_t __success_extra;
static unsigned int __failure_delay;
static bool __clock_regresses;

char* __realpath(const char* path)
{
    return strdup(path);
}

int sceIoMkdir(const char* path, unsigned int mode)
{
    assert(strcmp(path, "ux0:vitaarc/persistent/data/requested") == 0 && mode == 0610);
    return __error ? -123 : 0;
}

vita_newlib_descriptor_t* __vita_fd_grab(int fd)
{
    if(fd != 7) {
        return NULL;
    }

    ++__references;
    return &__descriptor;
}

int __vita_fd_drop(vita_newlib_descriptor_t* descriptor)
{
    assert(descriptor == &__descriptor && __references == 1);
    --__references;
    return 0;
}

int __vita_sce_errno_to_errno(int error, int type)
{
    assert(error == -123 && type == 0);
    return EIO;
}

int64_t sceIoLseek(int fd, int64_t offset, int whence)
{
    assert(fd == 91 && whence == SEEK_SET && __references == 1);
    __offset = offset;
    return __error ? -123 : offset;
}

int sceIoChstatByFd(int fd, const SceIoStat* status, unsigned int fields)
{
    assert(fd == 91 && fields == SCE_CST_SIZE && __references == 1);
    __length = status->st_size;
    return __error ? -123 : 0;
}

uint64_t sceKernelGetProcessTimeWide(void)
{
    return __clock;
}

int sceKernelDelayThread(unsigned int microseconds)
{
    assert(microseconds <= 3601000000u);
    __microseconds += microseconds;
    ++__delays;
    bool failed = __error && (!__failure_delay || __failure_delay == __delays);
    if(failed && __clock_regresses)
        __clock -= __failure_elapsed;
    else
        __clock += failed ? __failure_elapsed : microseconds + __success_extra;
    return failed ? -123 : 0;
}

int main(void)
{
    assert(bridge_mkdir("/data/data/moe.low.arc/requested", 0100610) == 0);
    __error = 1;
    assert(bridge_mkdir("/data/user/0/moe.low.arc/requested", 0610) == -1 && errno == EIO);
    __error = 0;
    assert(bridge_mkdir(NULL, 0700) == -1 && errno == EFAULT);
    int64_t large = INT64_C(5) * 1024 * 1024 * 1024;
    assert(bridge_lseek64(7, large, SEEK_SET) == large && __offset == large);
    assert(bridge_ftruncate64(7, large) == 0 && __length == large);
    assert(__references == 0);
    assert(bridge_lseek64(-1, 0, SEEK_SET) == -1 && errno == EBADF);
    assert(bridge_ftruncate64(-1, 10) == -1 && errno == EBADF);
    __descriptor.type = 1;
    assert(bridge_lseek64(7, 0, SEEK_SET) == -1 && errno == EBADF);
    assert(bridge_ftruncate64(7, 10) == -1 && errno == EBADF);
    __descriptor.type = 0;
    __descriptor.flags = O_RDONLY;
    assert(bridge_ftruncate64(7, 10) == -1 && errno == EBADF);
    __descriptor.flags = O_RDWR;
    __error = 1;
    assert(bridge_lseek64(7, 0, SEEK_SET) == -1 && errno == EIO);
    assert(bridge_ftruncate64(7, 10) == -1 && errno == EIO);
    assert(__references == 0);
    __error = 0;
    struct timespec time;
    assert(bridge_clock_gettime(1, &time) == 0);
    assert(time.tv_sec == 9876543 && time.tv_nsec == 210123000);
    assert(bridge_clock_gettime(4, &time) == 0);
    assert(bridge_clock_gettime(2, &time) == -1 && errno == EINVAL);
    struct timespec request = {.tv_sec = 3601, .tv_nsec = 1};

    assert(bridge_nanosleep(&request, NULL) == 0);
    assert(__delays == 2 && __microseconds == UINT64_C(3601000001));
    request.tv_sec = 0;
    request.tv_nsec = 999999999;
    __delays = 0;
    __microseconds = 0;
    assert(bridge_nanosleep(&request, NULL) == 0);
    assert(__delays == 1 && __microseconds == 1000000);
    struct timespec remaining = {0};

    __error = 1;
    assert(bridge_nanosleep(&request, &remaining) == -1 && errno == EIO);
    assert(remaining.tv_sec == request.tv_sec && remaining.tv_nsec == request.tv_nsec);
    request = (struct timespec){.tv_sec = 2, .tv_nsec = 100000000};

    __failure_elapsed = 750000;
    assert(bridge_nanosleep(&request, &remaining) == -1 && errno == EIO);
    assert(remaining.tv_sec == 1 && remaining.tv_nsec == 350000000);
    request = (struct timespec){.tv_sec = 0, .tv_nsec = 500000000};

    __failure_elapsed = 500000;
    assert(bridge_nanosleep(&request, &remaining) == -1 && errno == EIO);
    assert(remaining.tv_sec == 0 && remaining.tv_nsec == 0);
    __failure_elapsed = 700000;
    assert(bridge_nanosleep(&request, &remaining) == -1 && errno == EIO);
    assert(remaining.tv_sec == 0 && remaining.tv_nsec == 0);
    // Successful earlier chunks have already reduced the outstanding request.
    request = (struct timespec){.tv_sec = 3601, .tv_nsec = 250000000};

    __failure_elapsed = 500000;
    __failure_delay = 2;
    __delays = 0;
    assert(bridge_nanosleep(&request, &remaining) == -1 && errno == EIO);
    assert(__delays == 2 && remaining.tv_sec == 0 && remaining.tv_nsec == 750000000);
    __delays = 0;
    __success_extra = 250000;
    assert(bridge_nanosleep(&request, &remaining) == -1 && errno == EIO);
    assert(__delays == 2 && remaining.tv_sec == 0 && remaining.tv_nsec == 500000000);
    __success_extra = 0;
    __failure_delay = 0;
    __clock_regresses = true;
    assert(bridge_nanosleep(&request, &remaining) == -1 && errno == EIO);
    assert(remaining.tv_sec == request.tv_sec && remaining.tv_nsec == request.tv_nsec);
    __clock_regresses = false;
    __clock = UINT64_MAX - 100;
    __failure_elapsed = 50;
    request = (struct timespec){.tv_sec = 0, .tv_nsec = 90000};

    assert(bridge_nanosleep(&request, &request) == -1 && errno == EIO);
    assert(request.tv_sec == 0 && request.tv_nsec == 40000);
    __clock = UINT64_C(9876543210123);
    __failure_elapsed = 1250;
    request = (struct timespec){.tv_sec = INT32_MAX, .tv_nsec = 1};

    assert(bridge_nanosleep(&request, &remaining) == -1 && errno == EIO);
    assert(remaining.tv_sec == INT32_MAX - 1 && remaining.tv_nsec == 998750001);
    return 0;
}
