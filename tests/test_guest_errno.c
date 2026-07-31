#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <libc/guest_errno.h>

int _pthread_cond_timedwait(bridge_cond_t* condition, bridge_mutex_t* mutex, const struct timespec* time)
{
    (void)condition;
    (void)mutex;
    (void)time;
    errno = EILSEQ;
    return ETIMEDOUT;
}

static void __check_errors(void)
{
    assert(bridge_errno_from_native(ENAMETOOLONG) == 36);
    assert(bridge_errno_from_native(EOVERFLOW) == 75);
    assert(bridge_errno_from_native(ENOTSUP) == 95);
    assert(bridge_errno_from_native(ETIMEDOUT) == 110);
    assert(bridge_errno_from_native(EILSEQ) == 84);
    assert(bridge_errno_from_native(EDEADLK) == 35);
    assert(bridge_errno_from_native(123456) == 5);
    assert(bridge_errno_to_native(36) == ENAMETOOLONG);
    assert(bridge_errno_to_native(75) == EOVERFLOW);
    assert(bridge_errno_to_native(95) == ENOTSUP);
    assert(bridge_errno_to_native(110) == ETIMEDOUT);
    errno = 95;
    assert(bridge_guest_syscall(-1) == -1 && errno == 38);
    errno = 0;
    assert(bridge_guest_syscall(-1) == -1 && errno == 38);
    assert(bridge_guest_fcntl(-1, 5) == -1 && errno == 95);
    assert(bridge_guest_mmap(NULL, 4096, 3, 0x21, -1, 0) == (void*)-1 && errno == 95);
    errno = 123456;
    assert(bridge_guest_munmap((void*)4096, 4096) == 0 && errno == 123456);
    assert(bridge_guest_pthread_cond_timedwait(NULL, NULL, NULL) == 110 && errno == 123456);
    char buffer[128];
    assert(bridge_guest_strerror_r(95, buffer, sizeof(buffer)) == 0 && errno == 123456);
    assert(!strcmp(buffer, bridge_guest_strerror(95)));
    assert(bridge_guest_strerror_r(95, buffer, 1) == 34 && errno == 123456);
    assert(bridge_guest_snprintf(buffer, sizeof(buffer), "%d %.1f", 7, 2.5) == 5 && errno == 123456);
    assert(!strcmp(buffer, "7 2.5"));
    int integer;
    double fraction;
    assert(bridge_guest_sscanf(buffer, "%d %lf", &integer, &fraction) == 2 && integer == 7 && fraction == 2.5);
    assert(errno == 123456);
}

static void* __worker(void* argument)
{
    int marker = (int)(uintptr_t)argument;
    for(unsigned iteration = 0; iteration < 100; ++iteration) {
        errno = marker;
        assert(bridge_guest_munmap((void*)4096, 4096) == 0 && errno == marker);
        assert(bridge_guest_mmap(NULL, 4096, 3, 0x21, -1, 0) == (void*)-1 && errno == 95);
        assert(bridge_guest_syscall(-1) == -1 && errno == 38);
    }

    return NULL;
}

int main(void)
{
    __check_errors();
    pthread_t threads[4];
    errno = 76543;
    for(unsigned index = 0; index < 4; ++index) {
        assert(!pthread_create(&threads[index], NULL, __worker, (void*)(uintptr_t)(1000 + index)));
    }

    for(unsigned index = 0; index < 4; ++index) {
        assert(!pthread_join(threads[index], NULL));
    }

    assert(errno == 76543);
    puts(
        "Guest errno mapping, repeated errors, preserved values, direct error returns, strerror_r and thread isolation passed.");
}
