#include <errno.h>
#include <stddef.h>
#include <string.h>

#include "thread_attributes.h"

_Static_assert(sizeof(bridge_thread_attr_t) == 24, "Android ARM thread attributes");
_Static_assert(offsetof(bridge_thread_attr_t, sched_policy) == 16, "Android scheduling offset");
_Static_assert(sizeof(bridge_sched_param_t) == 4, "Android sched_param");
#ifdef __vita__
_Static_assert(
    sizeof(pthread_t) == 4 && sizeof(pthread_key_t) == 4, "Native opaque thread/key handles fit ARM guest slots");
_Static_assert(sizeof(pthread_attr_t) == 4, "Native attributes are a pointer, not the guest struct");
#endif

static int __valid(const bridge_thread_attr_t* attributes)
{
    return attributes && !((uintptr_t)attributes & 3) && !(attributes->flags & ~3u);
}

static int __stack_size(uint32_t size)
{
    return size >= 8192 && !(size & 4095);
}

int _pthread_attr_init(bridge_thread_attr_t* attributes)
{
    if(!attributes || ((uintptr_t)attributes & 3)) {
        return EINVAL;
    }

    *attributes = (bridge_thread_attr_t){.stack_size = 1024 * 1024, .guard_size = 4096};
    return 0;
}

int _pthread_attr_destroy(bridge_thread_attr_t* attributes)
{
    if(!__valid(attributes)) {
        return EINVAL;
    }

    memset(attributes, 0x42, sizeof(*attributes));
    return 0;
}

int _pthread_attr_getdetachstate(const bridge_thread_attr_t* attributes, int* state)
{
    if(!__valid(attributes) || !state) {
        return EINVAL;
    }

    *state = attributes->flags & 1;
    return 0;
}

int _pthread_attr_setdetachstate(bridge_thread_attr_t* attributes, int state)
{
    if(!__valid(attributes) || (state != 0 && state != 1)) {
        return EINVAL;
    }

    attributes->flags = (attributes->flags & ~1u) | (unsigned)state;
    return 0;
}

int _pthread_attr_getstacksize(const bridge_thread_attr_t* attributes, uint32_t* size)
{
    if(!__valid(attributes) || !size) {
        return EINVAL;
    }

    *size = attributes->stack_size;
    return 0;
}

int _pthread_attr_setstacksize(bridge_thread_attr_t* attributes, uint32_t size)
{
    if(!__valid(attributes) || !__stack_size(size)) {
        return EINVAL;
    }

    attributes->stack_size = size;
    return 0;
}

int _pthread_attr_getguardsize(const bridge_thread_attr_t* attributes, uint32_t* size)
{
    if(!__valid(attributes) || !size) {
        return EINVAL;
    }

    *size = attributes->guard_size;
    return 0;
}

int _pthread_attr_setguardsize(bridge_thread_attr_t* attributes, uint32_t size)
{
    if(!__valid(attributes)) {
        return EINVAL;
    }

    attributes->guard_size = size;
    return 0;
}

int _pthread_attr_getstack(const bridge_thread_attr_t* attributes, void** base, uint32_t* size)
{
    if(!__valid(attributes) || !base || !size) {
        return EINVAL;
    }

    *base = (void*)(uintptr_t)attributes->stack_base;
    *size = attributes->stack_size;
    return 0;
}

int _pthread_attr_setstack(bridge_thread_attr_t* attributes, void* base, uint32_t size)
{
    uintptr_t address = (uintptr_t)base;
    if(!__valid(attributes) || !address || address > UINT32_MAX || (address & 4095) || !__stack_size(size) ||
        size > UINT32_MAX - address) {
        return EINVAL;
    }

    attributes->stack_base = (uint32_t)address;
    attributes->stack_size = size;
    return 0;
}

int _pthread_attr_getschedpolicy(const bridge_thread_attr_t* attributes, int* policy)
{
    if(!__valid(attributes) || !policy) {
        return EINVAL;
    }

    *policy = attributes->sched_policy;
    return 0;
}

int _pthread_attr_setschedpolicy(bridge_thread_attr_t* attributes, int policy)
{
    if(!__valid(attributes) || policy < 0 || policy > 2) {
        return EINVAL;
    }

    attributes->sched_policy = policy;
    return 0;
}

int _pthread_attr_getschedparam(const bridge_thread_attr_t* attributes, bridge_sched_param_t* parameters)
{
    if(!__valid(attributes) || !parameters) {
        return EINVAL;
    }

    parameters->sched_priority = attributes->sched_priority;
    return 0;
}

int _pthread_attr_setschedparam(bridge_thread_attr_t* attributes, const bridge_sched_param_t* parameters)
{
    if(!__valid(attributes) || !parameters || parameters->sched_priority < 0 || parameters->sched_priority > 99) {
        return EINVAL;
    }

    attributes->sched_priority = parameters->sched_priority;
    return 0;
}

int _pthread_attr_getscope(const bridge_thread_attr_t* attributes, int* scope)
{
    if(!__valid(attributes) || !scope) {
        return EINVAL;
    }

    *scope = 0; /* Android PTHREAD_SCOPE_SYSTEM, opposite to this SDK's value. */
    return 0;
}

int _pthread_attr_setscope(bridge_thread_attr_t* attributes, int scope)
{
    if(!__valid(attributes) || scope < 0 || scope > 1) {
        return EINVAL;
    }

    return scope ? ENOTSUP : 0;
}

int bridge_thread_native_attributes(pthread_attr_t* native, const bridge_thread_attr_t* guest)
{
    if(!native || !__valid(guest) || !__stack_size(guest->stack_size) || guest->sched_policy < 0 ||
        guest->sched_policy > 2 || guest->sched_priority < 0 || guest->sched_priority > 99 ||
        (!guest->sched_policy && guest->sched_priority) || ((guest->flags & 2) && !guest->stack_base) ||
        (guest->stack_base && ((guest->stack_base & 4095) || guest->stack_size > UINT32_MAX - guest->stack_base))) {
        return EINVAL;
    }

#ifdef __vita__
    /* This SDK rejects nonzero guard sizes and ignores custom stack addresses
     * in pthread_create. Linux realtime priorities have no native mapping. */
    if(guest->guard_size || guest->stack_base || guest->sched_policy || guest->sched_priority) {
        return ENOTSUP;
    }

#endif
    int result = pthread_attr_init(native);
    if(result) {
        return result;
    }

    result = pthread_attr_setdetachstate(native, guest->flags & 1 ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE);
    if(!result) {
        result = pthread_attr_setstacksize(native, guest->stack_size);
    }

    if(!result) {
        result = pthread_attr_setguardsize(native, guest->guard_size);
    }

#ifndef __vita__
    if(!result && guest->stack_base) {
        result = pthread_attr_setstack(native, (void*)(uintptr_t)guest->stack_base, guest->stack_size);
    }

    if(!result) {
        static const int policies[] = {SCHED_OTHER, SCHED_FIFO, SCHED_RR};

        result = pthread_attr_setschedpolicy(native, policies[guest->sched_policy]);
    }

    if(!result) {
        struct sched_param parameter = {.sched_priority = guest->sched_priority};

        result = pthread_attr_setschedparam(native, &parameter);
    }

    if(!result) {
        result = pthread_attr_setinheritsched(native, PTHREAD_EXPLICIT_SCHED);
    }

#endif
    if(result) {
        pthread_attr_destroy(native);
    }

    return result;
}
