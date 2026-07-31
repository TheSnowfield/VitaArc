#ifndef VITAARC_LIBC_THREAD_ATTRIBUTES_H
#define VITAARC_LIBC_THREAD_ATTRIBUTES_H

#include <pthread.h>
#include <stdint.h>

/* Android's 32-bit pthread_attr_t, independent of the build host's pointers. */
typedef struct bridge_thread_attr {
    uint32_t flags;
    uint32_t stack_base;
    uint32_t stack_size;
    uint32_t guard_size;
    int32_t sched_policy;
    int32_t sched_priority;
} bridge_thread_attr_t;

typedef struct bridge_sched_param {
    int32_t sched_priority;
} bridge_sched_param_t;

int _pthread_attr_init(bridge_thread_attr_t* attributes);
int _pthread_attr_destroy(bridge_thread_attr_t* attributes);
int _pthread_attr_getdetachstate(const bridge_thread_attr_t* attributes, int* state);
int _pthread_attr_setdetachstate(bridge_thread_attr_t* attributes, int state);
int _pthread_attr_getstacksize(const bridge_thread_attr_t* attributes, uint32_t* size);
int _pthread_attr_setstacksize(bridge_thread_attr_t* attributes, uint32_t size);
int _pthread_attr_getguardsize(const bridge_thread_attr_t* attributes, uint32_t* size);
int _pthread_attr_setguardsize(bridge_thread_attr_t* attributes, uint32_t size);
int _pthread_attr_getstack(const bridge_thread_attr_t* attributes, void** base, uint32_t* size);
int _pthread_attr_setstack(bridge_thread_attr_t* attributes, void* base, uint32_t size);
int _pthread_attr_getschedpolicy(const bridge_thread_attr_t* attributes, int* policy);
int _pthread_attr_setschedpolicy(bridge_thread_attr_t* attributes, int policy);
int _pthread_attr_getschedparam(const bridge_thread_attr_t* attributes, bridge_sched_param_t* parameters);
int _pthread_attr_setschedparam(bridge_thread_attr_t* attributes, const bridge_sched_param_t* parameters);
int _pthread_attr_getscope(const bridge_thread_attr_t* attributes, int* scope);
int _pthread_attr_setscope(bridge_thread_attr_t* attributes, int scope);

/* Initializes a native attribute only on success; its caller then destroys it. */
int bridge_thread_native_attributes(pthread_attr_t* native, const bridge_thread_attr_t* guest);

#endif
