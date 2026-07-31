#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <libc/guest_errno.h>

static int __attributes;
static bool __fail_init, __fail_stack, __fail_create;
static int __expected_detach;
static atomic_uint __destroyed;
static pthread_key_t __key;
static pthread_mutex_t __gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t __ready = PTHREAD_COND_INITIALIZER;
static bool __release;

void log_v(const char* tag, const char* format, ...)
{
    (void)tag;
    (void)format;
}

int __real_pthread_attr_init(pthread_attr_t* attributes);
int __real_pthread_attr_destroy(pthread_attr_t* attributes);
int __real_pthread_attr_setstacksize(pthread_attr_t* attributes, size_t size);
int __real_pthread_create(pthread_t* thread, const pthread_attr_t* attributes, void* (*start)(void*), void* argument);

int __wrap_pthread_attr_init(pthread_attr_t* attributes)
{
    if(__fail_init) {
        return ENOMEM;
    }

    int result = __real_pthread_attr_init(attributes);
    if(!result) {
        ++__attributes;
    }

    return result;
}

int __wrap_pthread_attr_destroy(pthread_attr_t* attributes)
{
    --__attributes;
    return __real_pthread_attr_destroy(attributes);
}

int __wrap_pthread_attr_setstacksize(pthread_attr_t* attributes, size_t size)
{
    return __fail_stack ? EINVAL : __real_pthread_attr_setstacksize(attributes, size);
}

int __wrap_pthread_create(pthread_t* thread, const pthread_attr_t* attributes, void* (*start)(void*), void* argument)
{
    if(__fail_create) {
        memset(thread, 0, sizeof(*thread));
        return EAGAIN;
    }

    if(attributes) {
        int state;
        size_t size, guard;
        assert(pthread_attr_getdetachstate(attributes, &state) == 0 && state == __expected_detach);
        assert(pthread_attr_getstacksize(attributes, &size) == 0 && size == 1024 * 1024);
        assert(pthread_attr_getguardsize(attributes, &guard) == 0 && guard == 4096);
    }

    return __real_pthread_create(thread, attributes, start, argument);
}

static void __destroy_key(void* value)
{
    assert(value == &__key);
    atomic_fetch_add(&__destroyed, 1);
}

static void* __worker(void* argument)
{
    assert(_pthread_equal(_pthread_self(), pthread_self()));
    errno = 95;
    assert(bridge_guest_pthread_setspecific(__key, argument) == 0 && errno == 95);
    assert(_pthread_getspecific(__key) == argument);
    assert(pthread_mutex_lock(&__gate) == 0);
    while(!__release) {
        assert(pthread_cond_wait(&__ready, &__gate) == 0);
    }

    assert(pthread_mutex_unlock(&__gate) == 0);
    return argument;
}

int main(void)
{
    assert(_pthread_equal((pthread_t)0, (pthread_t)0) == 1);
    assert(_pthread_equal((pthread_t)0, pthread_self()) == 0);
    struct {
        uint32_t before;
        bridge_thread_attr_t value;
        uint32_t after;
    } guarded = {0x11223344, {0}, 0x55667788};

    bridge_thread_attr_t* attributes = &guarded.value;
    assert(sizeof(*attributes) == 24);
    errno = 95;
    assert(bridge_guest_pthread_attr_init(attributes) == 0 && errno == 95);
    const uint32_t expected[] = {0, 0, 1024 * 1024, 4096, 0, 0};

    assert(!memcmp(attributes, expected, sizeof(expected)));
    int value = -1;
    assert(bridge_guest_pthread_attr_getdetachstate(attributes, &value) == 0 && value == 0);
    assert(bridge_guest_pthread_attr_setdetachstate(attributes, 1) == 0 && attributes->flags == 1);
    assert(bridge_guest_pthread_attr_getdetachstate(attributes, &value) == 0 && value == 1);
    assert(bridge_guest_pthread_attr_setdetachstate(attributes, 2) == 22 && attributes->flags == 1 && errno == 95);
    assert(bridge_guest_pthread_attr_setdetachstate(attributes, 0) == 0);
    struct {
        uint32_t value, after;
    } size = {0, 0xaabbccdd};

    assert(bridge_guest_pthread_attr_getstacksize(attributes, &size.value) == 0 && size.value == 1024 * 1024 &&
        size.after == 0xaabbccdd);
    assert(bridge_guest_pthread_attr_setstacksize(attributes, 8191) == 22 && attributes->stack_size == 1024 * 1024);
    assert(bridge_guest_pthread_attr_setstacksize(attributes, 8193) == 22);
    assert(bridge_guest_pthread_attr_setstack(attributes, (void*)(uintptr_t)0x20000000, 131072) == 0);
    void* base;
    assert(bridge_guest_pthread_attr_getstack(attributes, &base, &size.value) == 0 &&
        base == (void*)(uintptr_t)0x20000000 && size.value == 131072);
    assert(bridge_guest_pthread_attr_setstack(attributes, (void*)(uintptr_t)0xfffff000, 8192) == 22);
    assert(bridge_guest_pthread_attr_setstack(attributes, (void*)(uintptr_t)0x20000001, 8192) == 22);
    assert(bridge_guest_pthread_attr_setguardsize(attributes, 123) == 0);
    assert(bridge_guest_pthread_attr_getguardsize(attributes, &size.value) == 0 && size.value == 123 &&
        size.after == 0xaabbccdd);
    assert(bridge_guest_pthread_attr_getscope(attributes, &value) == 0 && value == 0);
    assert(bridge_guest_pthread_attr_setscope(attributes, 1) == 95 && errno == 95);
    assert(bridge_guest_pthread_attr_setscope(attributes, 0) == 0);
    assert(bridge_guest_pthread_attr_setscope(attributes, -1) == 22);
    bridge_sched_param_t parameters = {.sched_priority = 7};

    assert(bridge_guest_pthread_attr_setschedpolicy(attributes, 2) == 0);
    assert(bridge_guest_pthread_attr_getschedpolicy(attributes, &value) == 0 && value == 2);
    assert(bridge_guest_pthread_attr_setschedparam(attributes, &parameters) == 0);
    parameters.sched_priority = 0;
    assert(bridge_guest_pthread_attr_getschedparam(attributes, &parameters) == 0 && parameters.sched_priority == 7);
    parameters.sched_priority = 100;
    assert(bridge_guest_pthread_attr_setschedparam(attributes, &parameters) == 22);
    assert(guarded.before == 0x11223344 && guarded.after == 0x55667788);
    assert(bridge_guest_pthread_attr_destroy(attributes) == 0);
    assert(bridge_guest_pthread_attr_getdetachstate(attributes, &value) == 22);
    assert(bridge_guest_pthread_attr_init(attributes) == 0);
    struct {
        uint64_t before;
        pthread_t thread;
        uint64_t after;
    } thread_guard;

    memset(&thread_guard, 0x5a, sizeof(thread_guard));
    unsigned char saved[sizeof(thread_guard.thread)];
    memcpy(saved, &thread_guard.thread, sizeof(saved));
    __fail_init = true;
    assert(bridge_guest_pthread_create(&thread_guard.thread, attributes, __worker, &__key) == 12);
    __fail_init = false;
    __fail_stack = true;
    assert(bridge_guest_pthread_create(&thread_guard.thread, attributes, __worker, &__key) == 22);
    __fail_stack = false;
    __fail_create = true;
    assert(bridge_guest_pthread_create(&thread_guard.thread, attributes, __worker, &__key) == 11);
    __fail_create = false;
    assert(!memcmp(saved, &thread_guard.thread, sizeof(saved)) && __attributes == 0);
    assert(thread_guard.before == UINT64_C(0x5a5a5a5a5a5a5a5a) && thread_guard.after == thread_guard.before &&
        errno == 95);
    assert(bridge_guest_pthread_key_create(&__key, __destroy_key) == 0);
    __release = true;
    __expected_detach = PTHREAD_CREATE_JOINABLE;
    assert(bridge_guest_pthread_create(&thread_guard.thread, attributes, __worker, &__key) == 0 && __attributes == 0);
    void* result;
    assert(bridge_guest_pthread_join(thread_guard.thread, &result) == 0 && result == &__key);
    assert(atomic_load(&__destroyed) == 1);
    assert(bridge_guest_pthread_attr_setdetachstate(attributes, 1) == 0);
    __expected_detach = PTHREAD_CREATE_DETACHED;
    assert(bridge_guest_pthread_create(&thread_guard.thread, attributes, __worker, &__key) == 0 && __attributes == 0);
    for(unsigned attempts = 0; atomic_load(&__destroyed) < 2; ++attempts) {
        assert(attempts < 10000);
        const struct timespec pause = {.tv_nsec = 1000000};

        assert(!nanosleep(&pause, NULL));
    }

    assert(bridge_guest_pthread_key_delete(__key) == 0);
    assert(bridge_guest_pthread_attr_destroy(attributes) == 0);
    assert(guarded.before == 0x11223344 && guarded.after == 0x55667788);
    puts(
        "Guest thread attributes passed: 24-byte layout/canaries, getters/setters, direct errno, native attr conversion/cleanup, creation failures, actual joinable/detached workers, IDs and TLS destructors.");
}
