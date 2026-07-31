#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libc/guest_errno.h>

static _Thread_local bool __fail_allocation;
static _Thread_local bool __initialized_here;
static atomic_int __allocations, __calls, __waits, __entered, __release_initializer, __attempts;
static bridge_once_t __control;
static int __payload;
static bool __cancel_in_sleep;

void* __real_calloc(size_t count, size_t size);
void __real_free(void* pointer);
int __real_pthread_cond_wait(pthread_cond_t* condition, pthread_mutex_t* mutex);

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

int __wrap_pthread_cond_wait(pthread_cond_t* condition, pthread_mutex_t* mutex)
{
    atomic_fetch_add(&__waits, 1);
    return __real_pthread_cond_wait(condition, mutex);
}

static void __wait_for(atomic_int* value, int minimum)
{
    for(unsigned iteration = 0; atomic_load(value) < minimum; ++iteration) {
        assert(iteration < 10000);
        const struct timespec delay = {.tv_nsec = 1000000};

        assert(nanosleep(&delay, NULL) == 0);
    }
}

static void __initialize(void)
{
    atomic_fetch_add(&__calls, 1);
    __initialized_here = true;
    atomic_store(&__entered, 1);
    __wait_for(&__release_initializer, 1);
    __payload = 42;
    errno = 9876;
}

static void* __worker(void* argument)
{
    int marker = (int)(uintptr_t)argument;
    errno = marker;
    assert(bridge_guest_pthread_once(&__control, __initialize) == 0);
    assert(__payload == 42 && errno == (__initialized_here ? 9876 : marker));
    return NULL;
}

static void __simple(void)
{
    atomic_fetch_add(&__calls, 1);
    errno = 321;
}

static void __nested(void)
{
    bridge_once_t inner = 0;
    assert(bridge_guest_pthread_once(&inner, __simple) == 0 && inner == 2 && errno == 321);
}

static void __exit_and_retry(void)
{
    if(atomic_fetch_add(&__attempts, 1) == 0) {
        atomic_store(&__entered, 1);
        if(__cancel_in_sleep) {
            __wait_for(&__release_initializer, 1);
        } else {
            while(!atomic_load(&__release_initializer)) {
                pthread_testcancel();
                sched_yield();
            }
        }

        pthread_exit((void*)123);
    }

    __payload = 73;
}

static void* __retry_worker(void* unused)
{
    (void)unused;
    assert(_pthread_once(&__control, __exit_and_retry) == 0 && __payload == 73);
    return NULL;
}

static void __check_initializer_cancel(void)
{
    pthread_t threads[2];
    void* result;
    __control = 0;
    atomic_store(&__entered, 0);
    atomic_store(&__attempts, 0);
    atomic_store(&__release_initializer, 0);
    atomic_store(&__waits, 0);
    assert(!pthread_create(&threads[0], NULL, __retry_worker, NULL));
    __wait_for(&__entered, 1);
    assert(!pthread_create(&threads[1], NULL, __retry_worker, NULL));
    __wait_for(&__waits, 1);
    assert(!pthread_cancel(threads[0]));
    assert(!pthread_join(threads[0], &result) && result == PTHREAD_CANCELED);
    assert(!pthread_join(threads[1], NULL));
    assert(__control == 2 && atomic_load(&__attempts) == 2 && atomic_load(&__allocations) == 0);
}

int main(int argc, char** argv)
{
    if(argc > 1) {
        assert(argc == 2 && !strcmp(argv[1], "sleep-cancel"));
        __cancel_in_sleep = true;
        __check_initializer_cancel();
        puts("Once initialization canceled inside nanosleep retried successfully with no retained entry.");
        return 0;
    }

    struct {
        uint32_t before;
        bridge_once_t control;
        uint32_t after;
    } guarded = {.before = 0x11223344, .after = 0x55667788};

    errno = 95;
    __fail_allocation = true;
    assert(bridge_guest_pthread_once(&guarded.control, __simple) == 12 && !guarded.control && errno == 95);
    assert(_pthread_once(NULL, __simple) == EINVAL);
    assert(_pthread_once((void*)1, __simple) == EINVAL);
    assert(_pthread_once(&guarded.control, NULL) == EINVAL);
    guarded.control = 3;
    assert(_pthread_once(&guarded.control, __simple) == EINVAL);
    guarded.control = 1;
    assert(_pthread_once(&guarded.control, __simple) == EINVAL);
    guarded.control = 0;
    assert(bridge_guest_pthread_once(&guarded.control, __simple) == 0 && guarded.control == 2 && errno == 321);
    errno = 95;
    assert(bridge_guest_pthread_once(&guarded.control, __simple) == 0 && errno == 95);
    assert(guarded.before == 0x11223344 && guarded.after == 0x55667788 && atomic_load(&__calls) == 1);
    guarded.control = 0;
    assert(bridge_guest_pthread_once(&guarded.control, __nested) == 0 && errno == 321);
    assert(atomic_load(&__calls) == 2 && atomic_load(&__allocations) == 0);

    pthread_t threads[8];
    int previous = atomic_load(&__calls);
    for(unsigned index = 0; index < 8; ++index) {
        assert(!pthread_create(&threads[index], NULL, __worker, (void*)(uintptr_t)(100 + index)));
    }

    __wait_for(&__entered, 1);
    __wait_for(&__waits, 7);
    atomic_store(&__release_initializer, 1);
    for(unsigned index = 0; index < 8; ++index) {
        assert(!pthread_join(threads[index], NULL));
    }

    assert(atomic_load(&__calls) == previous + 1 && atomic_load(&__allocations) == 0);

    /* Cancel a blocked waiter; its cleanup must release the registry mutex and
     * reference while leaving the initializer and another waiter operational. */
    __control = 0;
    atomic_store(&__entered, 0);
    atomic_store(&__release_initializer, 0);
    atomic_store(&__waits, 0);
    assert(!pthread_create(&threads[0], NULL, __worker, (void*)200));
    __wait_for(&__entered, 1);
    assert(!pthread_create(&threads[1], NULL, __worker, (void*)201));
    __wait_for(&__waits, 1);
    assert(!pthread_cancel(threads[1]));
    void* result;
    assert(!pthread_join(threads[1], &result) && result == PTHREAD_CANCELED);
    assert(!pthread_create(&threads[2], NULL, __worker, (void*)202));
    __wait_for(&__waits, 2);
    atomic_store(&__release_initializer, 1);
    assert(!pthread_join(threads[0], NULL) && !pthread_join(threads[2], NULL));
    assert(atomic_load(&__allocations) == 0);

    /* An initializer that exits does not permanently mark the guest control as
     * running. One waiting thread retries and publishes the completed state. */
    __control = 0;
    atomic_store(&__entered, 0);
    atomic_store(&__release_initializer, 0);
    atomic_store(&__waits, 0);
    assert(!pthread_create(&threads[0], NULL, __retry_worker, NULL));
    __wait_for(&__entered, 1);
    for(unsigned index = 1; index < 4; ++index) {
        assert(!pthread_create(&threads[index], NULL, __retry_worker, NULL));
    }

    __wait_for(&__waits, 3);
    atomic_store(&__release_initializer, 1);
    assert(!pthread_join(threads[0], &result) && result == (void*)123);
    for(unsigned index = 1; index < 4; ++index) {
        assert(!pthread_join(threads[index], NULL));
    }

    assert(__control == 2 && atomic_load(&__attempts) == 2 && atomic_load(&__allocations) == 0);

    __check_initializer_cancel();
    puts(
        "ARM32 once storage/canaries, OOM/retry, errno, nested/concurrent initialization, waiter cancellation and initializer-exit recovery passed.");
}
