#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include <libc/cxx.h>

static bool __fail_malloc;
void* __real_malloc(size_t size);
void* __wrap_malloc(size_t size)
{
    return __fail_malloc ? NULL : __real_malloc(size);
}

static unsigned __trace[2048], __trace_count;
static void* __dso = (void*)0x10080;

static void __record(void* argument)
{
    __trace[__trace_count++] = (unsigned)(uintptr_t)argument;
}

static void __other_record(void* argument)
{
    __record(argument);
}

static void __test_order(void)
{
    for(unsigned index = 0; index < 1024; ++index) {
        assert(bridge_cxa_atexit(__record, (void*)(uintptr_t)index, index % 2 ? __dso : (void*)0x20080) == 0);
    }

    bridge_cxa_finalize(__dso);
    assert(__trace_count == 512);
    for(unsigned index = 0; index < 512; ++index) {
        assert(__trace[index] == 1023 - 2 * index);
    }

    bridge_cxa_finalize(__dso);
    assert(__trace_count == 512);
    bridge_cxa_finalize(NULL);
    assert(__trace_count == 1024);
    for(unsigned index = 0; index < 512; ++index) {
        assert(__trace[512 + index] == 1022 - 2 * index);
    }

    __trace_count = 0;
    assert(bridge_cxa_atexit(__record, (void*)1, __dso) == 0);
    __fail_malloc = true;
    assert(bridge_cxa_atexit(__record, (void*)2, __dso) == -1 && errno == ENOMEM);
    __fail_malloc = false;
    __trace_count = 0;
    assert(bridge_aeabi_atexit((void*)7, __record, __dso) == 0);
    assert(bridge_cxa_atexit(__record, (void*)8, __dso) == 0);
    bridge_cxa_finalize(__dso);
    assert(__trace_count == 3 && __trace[0] == 8 && __trace[1] == 7 && __trace[2] == 1);
    __trace_count = 0;
    assert(bridge_cxa_atexit(NULL, NULL, NULL) == -1 && errno == EINVAL);
    bridge_cxa_finalize(__dso);
    assert(__trace_count == 0);
}

static void __nested(void* argument)
{
    __record(argument);
    assert(bridge_cxa_atexit(__record, (void*)4, __dso) == 0);
    bridge_cxa_finalize(__dso);
}

static void* __register_worker(void* argument)
{
    assert(bridge_cxa_atexit(__record, argument, __dso) == 0);
    return NULL;
}

static void __join_worker(void* argument)
{
    __record(argument);
    pthread_t worker;
    assert(pthread_create(&worker, NULL, __register_worker, (void*)6) == 0);
    assert(pthread_join(worker, NULL) == 0);
}

static void __test_reentry(void)
{
    __trace_count = 0;
    assert(bridge_cxa_atexit(__record, (void*)1, __dso) == 0);
    assert(bridge_cxa_atexit(__record, (void*)2, __dso) == 0);
    assert(bridge_cxa_atexit(__nested, (void*)3, __dso) == 0);
    bridge_cxa_finalize(__dso);
    assert(__trace_count == 4 && __trace[0] == 3 && __trace[1] == 4 && __trace[2] == 2 && __trace[3] == 1);
    __trace_count = 0;
    assert(bridge_cxa_atexit(__record, (void*)1, __dso) == 0);
    assert(bridge_cxa_atexit(__join_worker, (void*)5, __dso) == 0);
    bridge_cxa_finalize(__dso);
    assert(__trace_count == 3 && __trace[0] == 5 && __trace[1] == 6 && __trace[2] == 1);
}

static atomic_uint __hits[1024];

static void __hit(void* argument)
{
    unsigned index = (unsigned)(uintptr_t)argument;
    assert(atomic_fetch_add(&__hits[index], 1) == 0);
}

static void* __register_many(void* argument)
{
    unsigned start = (unsigned)(uintptr_t)argument;
    for(unsigned index = start; index < start + 256; ++index) {
        assert(bridge_cxa_atexit(__hit, (void*)(uintptr_t)index, __dso) == 0);
    }

    return NULL;
}

static void* __finalize_worker(void* unused)
{
    (void)unused;
    bridge_cxa_finalize(NULL);
    return NULL;
}

static void __test_threads(void)
{
    pthread_t workers[4];
    for(unsigned index = 0; index < 4; ++index) {
        assert(pthread_create(&workers[index], NULL, __register_many, (void*)(uintptr_t)(index * 256)) == 0);
    }

    for(unsigned index = 0; index < 4; ++index) {
        assert(pthread_join(workers[index], NULL) == 0);
    }

    for(unsigned index = 0; index < 4; ++index) {
        assert(pthread_create(&workers[index], NULL, __finalize_worker, NULL) == 0);
    }

    for(unsigned index = 0; index < 4; ++index) {
        assert(pthread_join(workers[index], NULL) == 0);
    }

    bridge_cxa_finalize(NULL);
    for(unsigned index = 0; index < 1024; ++index) {
        assert(atomic_load(&__hits[index]) == 1);
    }
}

static int __exit_pipe;
static void __exit_callback(void* unused)
{
    (void)unused;
    assert(write(__exit_pipe, "x", 1) == 1);
}

static void __test_range_and_exit(void)
{
    __trace_count = 0;
    assert(bridge_cxa_atexit(__record, (void*)1, __dso) == 0);
    assert(bridge_cxa_atexit(__other_record, (void*)2, (void*)0x20080) == 0);
    bridge_cxa_finalize_range(0x10000, 0x1000);
    assert(__trace_count == 1 && __trace[0] == 1);
    assert(bridge_cxa_atexit(__record, (void*)3, NULL) == 0);
    bridge_cxa_finalize_range((uintptr_t)__record, 1);
    assert(__trace_count == 2 && __trace[1] == 3);
    bridge_cxa_finalize_range(0, 0);
    assert(__trace_count == 2);
    bridge_cxa_finalize(NULL);
    assert(__trace_count == 3 && __trace[2] == 2);
    int descriptors[2];
    assert(pipe(descriptors) == 0);
    pid_t child = fork();
    assert(child >= 0);
    if(!child) {
        close(descriptors[0]);
        __exit_pipe = descriptors[1];
        assert(bridge_cxa_atexit(__exit_callback, NULL, NULL) == 0);
        bridge_exit(23);
    }

    close(descriptors[1]);
    char value;
    assert(read(descriptors[0], &value, 1) == 1 && value == 'x');
    close(descriptors[0]);
    int status;
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 23);
}

int main(void)
{
    __test_order();
    __test_reentry();
    __test_threads();
    __test_range_and_exit();
    puts(
        "C++ finalizers passed: 1024 entries, DSO/LIFO, OOM, reentry, worker join, concurrent exactly-once, unload ranges and exit");
    return 0;
}
