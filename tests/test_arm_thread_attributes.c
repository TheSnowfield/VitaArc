#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <libc/thread_attributes.h>

typedef void* (*resolve_t)(const char*);
typedef struct native_api {
    int (*getdetach)(const pthread_attr_t*, int*);
    int (*getstack)(const pthread_attr_t*, size_t*);
    int (*getguard)(const pthread_attr_t*, size_t*);
    int (*getscope)(const pthread_attr_t*, int*);
} native_api_t;

#define CALL(name, signature, ...) (((int(*) signature)resolve(#name))(__VA_ARGS__))
#define CHECK(value)                                                                                                   \
    do {                                                                                                               \
        if(!(value))                                                                                                   \
            return __LINE__;                                                                                           \
    } while(0)

const uint32_t thread_native_sizes[] = {
    sizeof(pthread_t), sizeof(pthread_key_t), sizeof(pthread_attr_t), sizeof(bridge_thread_attr_t)};

struct {
    uint32_t before;
    bridge_thread_attr_t value;
    uint32_t after;
} thread_attributes_guard;

struct {
    uint32_t before;
    pthread_t value;
    uint32_t after;
} thread_id_guard;

static native_api_t* __native;
static unsigned __creates;
static int __detached, __fail_create;

static void* __worker(void* argument)
{
    return argument;
}

/* This fixture replaces only the native thread-creation endpoint. All native
 * attribute getters below and the production guest conversion execute. */
int test_native_create(pthread_t* thread, const pthread_attr_t* attributes, void* (*start)(void*), void* argument)
{
    ++__creates;
    CHECK(start == __worker && argument == &thread_attributes_guard);
    if(__fail_create) {
        *thread = 0;
        return EAGAIN;
    }

    if(attributes) {
        CHECK((const void*)attributes != (const void*)&thread_attributes_guard.value);
        int detached = -1, scope = -1;
        size_t stack = 0, guard = 1;
        CHECK(__native->getdetach(attributes, &detached) == 0 &&
            detached == (__detached ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE));
        CHECK(__native->getstack(attributes, &stack) == 0 && stack == 1024 * 1024);
        CHECK(__native->getguard(attributes, &guard) == 0 && guard == 0);
        CHECK(PTHREAD_SCOPE_SYSTEM == 1);
        CHECK(__native->getscope(attributes, &scope) == ENOSYS && scope == -1);
    }

    *thread = 0x12340000;
    return 0;
}

int test_native_join(pthread_t thread, void** result)
{
    CHECK(thread == 0x12340000);
    *result = (void*)(uintptr_t)0x87650000;
    return 0;
}

int test_arm_thread_attributes_entry(resolve_t resolve, native_api_t* native)
{
    __native = native;
    __creates = 0;
    __detached = __fail_create = 0;
    bridge_thread_attr_t* attributes = &thread_attributes_guard.value;
    thread_attributes_guard.before = 0xaabbccdd;
    thread_attributes_guard.after = 0xddccbbaa;
    thread_id_guard.before = 0x11223344;
    thread_id_guard.value = 0x55555555;
    thread_id_guard.after = 0x55667788;
    int* error = ((int* (*)(void))resolve("__errno"))();
    *error = 95;
    CHECK(CALL(pthread_attr_init, (bridge_thread_attr_t*), attributes) == 0);
    CHECK(attributes->flags == 0 && attributes->stack_base == 0 && attributes->stack_size == 1024 * 1024 &&
        attributes->guard_size == 4096 && !attributes->sched_policy && !attributes->sched_priority);
    CHECK(CALL(pthread_create,
              (pthread_t*, const bridge_thread_attr_t*, void* (*)(void*), void*),
              &thread_id_guard.value,
              attributes,
              __worker,
              &thread_attributes_guard) == 95);
    CHECK(__creates == 0 && thread_id_guard.value == 0x55555555 && *error == 95);
    CHECK(CALL(pthread_attr_setguardsize, (bridge_thread_attr_t*, uint32_t), attributes, 0) == 0);
    CHECK(CALL(pthread_create,
              (pthread_t*, const bridge_thread_attr_t*, void* (*)(void*), void*),
              &thread_id_guard.value,
              attributes,
              __worker,
              &thread_attributes_guard) == 0);
    CHECK(thread_id_guard.value == 0x12340000 && __creates == 1);
    void* result = 0;
    CHECK(CALL(pthread_join, (pthread_t, void**), thread_id_guard.value, &result) == 0 &&
        result == (void*)(uintptr_t)0x87650000);
    __detached = 1;
    CHECK(
        CALL(pthread_attr_setdetachstate, (bridge_thread_attr_t*, int), attributes, 1) == 0 && attributes->flags == 1);
    CHECK(CALL(pthread_create,
              (pthread_t*, const bridge_thread_attr_t*, void* (*)(void*), void*),
              &thread_id_guard.value,
              attributes,
              __worker,
              &thread_attributes_guard) == 0);
    CHECK(__creates == 2 && thread_id_guard.value == 0x12340000);
    __fail_create = 1;
    thread_id_guard.value = 0x55555555;
    CHECK(CALL(pthread_create,
              (pthread_t*, const bridge_thread_attr_t*, void* (*)(void*), void*),
              &thread_id_guard.value,
              attributes,
              __worker,
              &thread_attributes_guard) == 11);
    CHECK(thread_id_guard.value == 0x55555555 && *error == 95);
    __fail_create = 0;
    CHECK(CALL(pthread_attr_setstacksize, (bridge_thread_attr_t*, uint32_t), attributes, 8192) == 0);
    CHECK(CALL(pthread_create,
              (pthread_t*, const bridge_thread_attr_t*, void* (*)(void*), void*),
              &thread_id_guard.value,
              attributes,
              __worker,
              &thread_attributes_guard) == 22);
    CHECK(__creates == 3 && thread_id_guard.value == 0x55555555);
    CHECK(CALL(pthread_attr_setstacksize, (bridge_thread_attr_t*, uint32_t), attributes, 1024 * 1024) == 0);
    attributes->stack_base = 0x20000000;
    CHECK(CALL(pthread_create,
              (pthread_t*, const bridge_thread_attr_t*, void* (*)(void*), void*),
              &thread_id_guard.value,
              attributes,
              __worker,
              &thread_attributes_guard) == 95);
    attributes->stack_base = 0;
    CHECK(CALL(pthread_attr_setschedpolicy, (bridge_thread_attr_t*, int), attributes, 1) == 0);
    bridge_sched_param_t parameters = {.sched_priority = 1};

    CHECK(CALL(pthread_attr_setschedparam,
              (bridge_thread_attr_t*, const bridge_sched_param_t*),
              attributes,
              &parameters) == 0);
    CHECK(CALL(pthread_create,
              (pthread_t*, const bridge_thread_attr_t*, void* (*)(void*), void*),
              &thread_id_guard.value,
              attributes,
              __worker,
              &thread_attributes_guard) == 95);
    CHECK(CALL(pthread_attr_setscope, (bridge_thread_attr_t*, int), attributes, 1) == 95);
    int scope = -1;
    CHECK(CALL(pthread_attr_getscope, (const bridge_thread_attr_t*, int*), attributes, &scope) == 0 && scope == 0);
    CHECK(CALL(pthread_attr_destroy, (bridge_thread_attr_t*), attributes) == 0);
    CHECK(CALL(pthread_attr_getscope, (const bridge_thread_attr_t*, int*), attributes, &scope) == 22);
    CHECK(thread_attributes_guard.before == 0xaabbccdd && thread_attributes_guard.after == 0xddccbbaa);
    CHECK(thread_id_guard.before == 0x11223344 && thread_id_guard.after == 0x55667788 &&
        thread_id_guard.value == 0x55555555);
    CHECK(*error == 95 && __creates == 3);
    __detached = 0;
    CHECK(CALL(pthread_create,
              (pthread_t*, const bridge_thread_attr_t*, void* (*)(void*), void*),
              &thread_id_guard.value,
              NULL,
              __worker,
              &thread_attributes_guard) == 0 &&
        __creates == 4);
    /* Current native IDs/keys are opaque 32-bit values, not SDK once objects. */
    pthread_t self = ((pthread_t (*)(void))resolve("pthread_self"))();
    CHECK(self == 0x221000 && CALL(pthread_equal, (pthread_t, pthread_t), self, self) != 0);
    CHECK(CALL(pthread_equal, (pthread_t, pthread_t), 0, 0) == 1);
    CHECK(CALL(pthread_equal, (pthread_t, pthread_t), 0, self) == 0);
    CHECK(CALL(pthread_equal, (pthread_t, pthread_t), 0xfedcba98, 0xfedcba98) == 1);
    CHECK(CALL(pthread_equal, (pthread_t, pthread_t), 0xfedcba98, 0xfedcba99) == 0);
    struct {
        uint32_t before;
        pthread_key_t value;
        uint32_t after;
    } key = {0xabcd1234, 0, 0x4321dcba};

    CHECK(CALL(pthread_key_create, (pthread_key_t*, void (*)(void*)), &key.value, NULL) == 0);
    CHECK(CALL(pthread_setspecific, (pthread_key_t, const void*), key.value, &key) == 0);
    CHECK(((void* (*)(pthread_key_t))resolve("pthread_getspecific"))(key.value) == &key);
    CHECK(key.before == 0xabcd1234 && key.after == 0x4321dcba && *error == 95);
    CHECK(CALL(pthread_key_delete, (pthread_key_t), key.value) == 0);
    return 0;
}
