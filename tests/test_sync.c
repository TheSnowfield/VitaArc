#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <libc/guest_errno.h>

static atomic_int __allocations, __waiting;
static _Thread_local bool __fail_allocation;
static bool __fail_destroy;
static bridge_mutex_t __mutex;
static bridge_cond_t __condition;
static unsigned __counter;
static bool __ready;

void* __real_calloc(size_t count, size_t size);
void __real_free(void* pointer);
int __real_pthread_mutex_destroy(pthread_mutex_t* mutex);

void* __wrap_calloc(size_t count, size_t size)
{
    if(__fail_allocation) {
        __fail_allocation = false;
        errno = ENOMEM;
        return NULL;
    }

    void* pointer = __real_calloc(count, size);
    if(pointer) {
        atomic_fetch_add(&__allocations, 1);
    }

    return pointer;
}

void __wrap_free(void* pointer)
{
    if(pointer) {
        atomic_fetch_sub(&__allocations, 1);
    }

    __real_free(pointer);
}

int __wrap_pthread_mutex_destroy(pthread_mutex_t* mutex)
{
    if(__fail_destroy) {
        return EIO;
    }

    return __real_pthread_mutex_destroy(mutex);
}

static void* __increment(void* unused)
{
    (void)unused;
    for(unsigned index = 0; index < 1000; ++index) {
        assert(_pthread_mutex_lock(&__mutex) == 0);
        ++__counter;
        assert(_pthread_mutex_unlock(&__mutex) == 0);
    }

    return NULL;
}

static void* __timed_lock(void* argument)
{
    bridge_mutex_t* mutex = argument;
    struct timespec deadline;
    assert(clock_gettime(CLOCK_REALTIME, &deadline) == 0);
    deadline.tv_nsec += 10000000;
    if(deadline.tv_nsec >= 1000000000) {
        ++deadline.tv_sec;
        deadline.tv_nsec -= 1000000000;
    }

    errno = 95;
    assert(bridge_guest_pthread_mutex_timedlock(mutex, &deadline) == 110 && errno == 95);
    return NULL;
}

static void __unlock(void* argument)
{
    assert(_pthread_mutex_unlock(argument) == 0);
}

static void* __waiter(void* unused)
{
    (void)unused;
    assert(_pthread_mutex_lock(&__mutex) == 0);
    pthread_cleanup_push(__unlock, &__mutex);
    atomic_fetch_add(&__waiting, 1);
    while(!__ready) {
        assert(_pthread_cond_wait(&__condition, &__mutex) == 0);
    }

    ++__counter;
    pthread_cleanup_pop(1);
    return NULL;
}

static void __await(unsigned count)
{
    for(unsigned attempt = 0; (unsigned)atomic_load(&__waiting) < count; ++attempt) {
        assert(attempt < 10000);
        const struct timespec pause = {.tv_nsec = 1000000};

        assert(nanosleep(&pause, NULL) == 0);
    }
}

static void __check_mutex_types(void)
{
    struct {
        uint32_t before;
        bridge_mutex_t mutex;
        uint32_t after;
    } guarded = {0x11223344, 0x4000, 0x55667788};

    assert(_pthread_mutex_lock(&guarded.mutex) == 0);
    assert(_pthread_mutex_trylock(&guarded.mutex) == 0);
    uint32_t value = guarded.mutex;
    assert(_pthread_mutex_destroy(&guarded.mutex) == EBUSY && guarded.mutex == value);
    assert(_pthread_mutex_unlock(&guarded.mutex) == 0 && _pthread_mutex_unlock(&guarded.mutex) == 0);
    __fail_destroy = true;
    assert(_pthread_mutex_destroy(&guarded.mutex) == EIO && guarded.mutex == value);
    __fail_destroy = false;
    assert(_pthread_mutex_trylock(&guarded.mutex) == 0 && _pthread_mutex_unlock(&guarded.mutex) == 0);
    assert(_pthread_mutex_destroy(&guarded.mutex) == 0);
    assert(guarded.before == 0x11223344 && guarded.after == 0x55667788);
    assert(_pthread_mutex_lock(&guarded.mutex) == EINVAL);

    guarded.mutex = 0x8000;
    errno = 95;
    assert(bridge_guest_pthread_mutex_lock(&guarded.mutex) == 0 && errno == 95);
    assert(bridge_guest_pthread_mutex_lock(&guarded.mutex) == 35 && errno == 95);
    assert(_pthread_mutex_unlock(&guarded.mutex) == 0 && _pthread_mutex_destroy(&guarded.mutex) == 0);

    struct {
        uint32_t before;
        bridge_mutexattr_t attributes;
        uint32_t after;
    } attributes = {11, 0, 22};

    int type;
    assert(_pthread_mutexattr_init(&attributes.attributes) == 0);
    assert(_pthread_mutexattr_settype(&attributes.attributes, 1) == 0);
    assert(attributes.attributes == 1 && _pthread_mutexattr_gettype(&attributes.attributes, &type) == 0 && type == 1);
    assert(_pthread_mutex_init(&guarded.mutex, &attributes.attributes) == 0);
    assert(_pthread_mutex_lock(&guarded.mutex) == 0 && _pthread_mutex_trylock(&guarded.mutex) == 0);
    assert(_pthread_mutex_unlock(&guarded.mutex) == 0 && _pthread_mutex_unlock(&guarded.mutex) == 0);
    assert(_pthread_mutex_destroy(&guarded.mutex) == 0);
    assert(_pthread_mutexattr_setpshared(&attributes.attributes, 1) == 0);
    assert(_pthread_mutex_init(&guarded.mutex, &attributes.attributes) == ENOTSUP);
    assert(
        _pthread_mutexattr_destroy(&attributes.attributes) == 0 && attributes.before == 11 && attributes.after == 22);
    assert(_pthread_mutexattr_settype(&attributes.attributes, 0) == EINVAL);
    guarded.mutex = 0;
    __fail_allocation = true;
    assert(_pthread_mutex_trylock(&guarded.mutex) == ENOMEM && guarded.mutex == 0);
    assert(_pthread_mutex_trylock(&guarded.mutex) == 0 && _pthread_mutex_unlock(&guarded.mutex) == 0);
    pthread_t thread;
    assert(_pthread_mutex_lock(&guarded.mutex) == 0);
    assert(!pthread_create(&thread, NULL, __timed_lock, &guarded.mutex));
    assert(!pthread_join(thread, NULL));
    assert(_pthread_mutex_unlock(&guarded.mutex) == 0 && _pthread_mutex_destroy(&guarded.mutex) == 0);
    assert(atomic_load(&__allocations) == 0);
}

static void __check_conditions(void)
{
    pthread_t threads[4];
    for(unsigned index = 0; index < 4; ++index) {
        assert(!pthread_create(&threads[index], NULL, __waiter, NULL));
    }

    __await(4);
    assert(_pthread_mutex_lock(&__mutex) == 0);
    assert(_pthread_cond_destroy(&__condition) == EBUSY && _pthread_mutex_destroy(&__mutex) == EBUSY);
    __ready = true;
    assert(_pthread_cond_broadcast(&__condition) == 0);
    assert(_pthread_mutex_unlock(&__mutex) == 0);
    for(unsigned index = 0; index < 4; ++index) {
        assert(!pthread_join(threads[index], NULL));
    }

    assert(__counter == 8004);
    assert(_pthread_cond_destroy(&__condition) == 0 && _pthread_mutex_destroy(&__mutex) == 0);

    __mutex = __condition = 0;
    __ready = false;
    atomic_store(&__waiting, 0);
    assert(!pthread_create(&threads[0], NULL, __waiter, NULL));
    __await(1);
    assert(_pthread_mutex_lock(&__mutex) == 0 && _pthread_mutex_unlock(&__mutex) == 0);
    assert(!pthread_cancel(threads[0]));
    void* result;
    assert(!pthread_join(threads[0], &result) && result == PTHREAD_CANCELED);
    assert(_pthread_mutex_trylock(&__mutex) == 0 && _pthread_mutex_unlock(&__mutex) == 0);
    assert(_pthread_cond_destroy(&__condition) == 0 && _pthread_mutex_destroy(&__mutex) == 0);
    assert(atomic_load(&__allocations) == 0);

    __mutex = 0;
    bridge_condattr_t attributes;
    assert(
        _pthread_condattr_init(&attributes) == 0 && _pthread_condattr_setclock(&attributes, 1) == 0 && attributes == 2);
    assert(_pthread_cond_init(&__condition, &attributes) == 0 && _pthread_condattr_destroy(&attributes) == 0);
    assert(_pthread_mutex_lock(&__mutex) == 0);
    struct timespec deadline;
    assert(clock_gettime(CLOCK_MONOTONIC, &deadline) == 0);
    deadline.tv_sec -= 1;
    assert(_pthread_cond_timedwait(&__condition, &__mutex, &deadline) == ETIMEDOUT);
    assert(_pthread_mutex_unlock(&__mutex) == 0);
    assert(_pthread_cond_destroy(&__condition) == 0 && _pthread_mutex_destroy(&__mutex) == 0);
}

static void __check_ranges(void)
{
    uint32_t controls[3] = {0};

    assert(_pthread_mutex_lock(&controls[0]) == 0 && _pthread_cond_init(&controls[1], NULL) == 0);
    assert(bridge_sync_destroy_range((uintptr_t)controls, sizeof(uint32_t) * 2) == EBUSY);
    assert(_pthread_cond_signal(&controls[1]) == 0);
    assert(_pthread_mutex_unlock(&controls[0]) == 0);
    assert(_pthread_mutex_lock(&controls[2]) == 0);
    assert(bridge_sync_range_busy((uintptr_t)controls, sizeof(uint32_t) * 2) == 0);
    assert(bridge_sync_destroy_range((uintptr_t)controls, sizeof(uint32_t) * 2) == 0);
    assert(_pthread_mutex_trylock(&controls[2]) == EBUSY);
    assert(_pthread_mutex_unlock(&controls[2]) == 0 && _pthread_mutex_destroy(&controls[2]) == 0);
    assert(atomic_load(&__allocations) == 0);
}

int main(void)
{
    __check_mutex_types();
    pthread_t threads[8];
    for(unsigned index = 0; index < 8; ++index) {
        assert(!pthread_create(&threads[index], NULL, __increment, NULL));
    }

    for(unsigned index = 0; index < 8; ++index) {
        assert(!pthread_join(threads[index], NULL));
    }

    assert(__counter == 8000 && _pthread_mutex_destroy(&__mutex) == 0);
    __mutex = 0;
    __check_conditions();
    __check_ranges();
    puts(
        "Guest mutex/condition ABI passed: canaries, static/attribute types, lazy contention, busy/error preservation, deadlines, wake/cancel and range cleanup.");
}
