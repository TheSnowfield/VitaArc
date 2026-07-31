#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <libc/once.h>

typedef struct once_api {
    int (*once)(bridge_once_t*, void (*)(void));
    int* (*error)(void);
} once_api_t;

struct {
    uint32_t before;
    bridge_once_t control;
    uint32_t after;
    uint32_t tail;
} once_guard;

static once_api_t* __api;
static bridge_once_t __inner;
static unsigned __outer_calls, __inner_calls;
static int __failure;

static void __inner_initialize(void)
{
    ++__inner_calls;
    *__api->error() = 321;
}

static void __outer_initialize(void)
{
    ++__outer_calls;
    int status = __api->once(&__inner, __inner_initialize);
    if(status || *__api->error() != 321) {
        __failure = 100;
    }

    *__api->error() = 654;
}

unsigned test_native_once_size(void)
{
    return sizeof(pthread_once_t);
}

unsigned test_guest_once_size(void)
{
    return sizeof(bridge_once_t);
}

int test_arm_once_entry(once_api_t* api)
{
    __api = api;
    __inner = 0;
    __outer_calls = __inner_calls = 0;
    __failure = 0;
    once_guard.before = 0x11223344;
    once_guard.control = 0;
    once_guard.after = 0x55667788;
    once_guard.tail = 0x99aabbcc;
    *api->error() = 95;
    int status = api->once(&once_guard.control, __outer_initialize);
    if(status) {
        return status;
    }

    if(*api->error() != 654 || __failure || __outer_calls != 1 || __inner_calls != 1 || once_guard.control != 2 ||
        __inner != 2) {
        return 101;
    }

    *api->error() = 95;
    if(api->once(&once_guard.control, __outer_initialize) || *api->error() != 95 || __outer_calls != 1) {
        return 102;
    }

    once_guard.control = 0;
    *api->error() = 321;
    if(api->once(&once_guard.control, __outer_initialize) || __outer_calls != 2 || __inner_calls != 1 || __failure) {
        return 103;
    }

    if(once_guard.before != 0x11223344 || once_guard.after != 0x55667788 || once_guard.tail != 0x99aabbcc) {
        return 104;
    }

    return 0;
}
