#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <libc/sync.h>

typedef void* (*resolve_t)(const char*);
#define CALL(name, signature, ...) (((int(*) signature)resolve(#name))(__VA_ARGS__))
#define CHECK(expression)                                                                                              \
    do {                                                                                                               \
        if(!(expression))                                                                                              \
            return __LINE__;                                                                                           \
    } while(0)

struct {
    uint32_t before;
    bridge_mutex_t mutex;
    uint32_t middle;
    bridge_cond_t condition;
    uint32_t after;
} sync_guard;

/* Independent SDK constants are consumed only by the modeled native backend. */
const unsigned sync_native_constants[] = {PTHREAD_MUTEX_NORMAL,
    PTHREAD_MUTEX_RECURSIVE,
    PTHREAD_MUTEX_ERRORCHECK,
    EBUSY,
    EDEADLK,
    ETIMEDOUT,
    EIO,
    sizeof(pthread_mutex_t),
    sizeof(pthread_cond_t),
    sizeof(pthread_mutexattr_t),
    sizeof(pthread_condattr_t)};

int test_arm_sync_entry(resolve_t resolve)
{
    int* error = ((int* (*)(void))resolve("__errno"))();
    sync_guard.before = 0x12345678;
    sync_guard.middle = 0x87654321;
    sync_guard.after = 0xabcdef01;
    sync_guard.mutex = 0x4000;
    sync_guard.condition = 0;
    *error = 95;
    CHECK(CALL(pthread_mutex_lock, (bridge_mutex_t*), &sync_guard.mutex) == 0);
    CHECK(CALL(pthread_mutex_trylock, (bridge_mutex_t*), &sync_guard.mutex) == 0);
    CHECK(CALL(pthread_mutex_destroy, (bridge_mutex_t*), &sync_guard.mutex) == 16);
    CHECK(CALL(pthread_mutex_unlock, (bridge_mutex_t*), &sync_guard.mutex) == 0);
    CHECK(CALL(pthread_mutex_unlock, (bridge_mutex_t*), &sync_guard.mutex) == 0);
    CHECK(CALL(pthread_mutex_destroy, (bridge_mutex_t*), &sync_guard.mutex) == 0);
    CHECK(*error == 95);
    sync_guard.mutex = 0x8000;
    CHECK(CALL(pthread_mutex_lock, (bridge_mutex_t*), &sync_guard.mutex) == 0);
    CHECK(CALL(pthread_mutex_lock, (bridge_mutex_t*), &sync_guard.mutex) == 35);
    CHECK(CALL(pthread_mutex_unlock, (bridge_mutex_t*), &sync_guard.mutex) == 0);
    CHECK(CALL(pthread_mutex_destroy, (bridge_mutex_t*), &sync_guard.mutex) == 0);
    struct {
        uint32_t before, value, after;
    } attribute = {0xaabbccdd, 0xffffffff, 0xddccbbaa};

    CHECK(CALL(pthread_mutexattr_init, (bridge_mutexattr_t*), &attribute.value) == 0 && attribute.value == 0);
    CHECK(
        CALL(pthread_mutexattr_settype, (bridge_mutexattr_t*, int), &attribute.value, 1) == 0 && attribute.value == 1);
    int type = -1;
    CHECK(
        CALL(pthread_mutexattr_gettype, (const bridge_mutexattr_t*, int*), &attribute.value, &type) == 0 && type == 1);
    CHECK(CALL(pthread_mutex_init, (bridge_mutex_t*, const bridge_mutexattr_t*), &sync_guard.mutex, &attribute.value) ==
        0);
    CHECK(CALL(pthread_mutexattr_destroy, (bridge_mutexattr_t*), &attribute.value) == 0);
    CHECK(CALL(pthread_mutex_destroy, (bridge_mutex_t*), &sync_guard.mutex) == 0);
    CHECK(attribute.before == 0xaabbccdd && attribute.after == 0xddccbbaa);
    CHECK(CALL(pthread_cond_signal, (bridge_cond_t*), &sync_guard.condition) == 0);
    CHECK(CALL(pthread_condattr_init, (bridge_condattr_t*), &attribute.value) == 0 && attribute.value == 0);
    CHECK(CALL(pthread_condattr_setclock, (bridge_condattr_t*, int), &attribute.value, 1) == 0 && attribute.value == 2);
    CHECK(
        CALL(pthread_cond_init, (bridge_cond_t*, const bridge_condattr_t*), &sync_guard.condition, &attribute.value) ==
        0);
    CHECK(CALL(pthread_condattr_destroy, (bridge_condattr_t*), &attribute.value) == 0);
    sync_guard.mutex = 0;
    CHECK(CALL(pthread_mutex_lock, (bridge_mutex_t*), &sync_guard.mutex) == 0);
    const struct timespec deadline = {10, 250000000};

    CHECK(CALL(pthread_cond_timedwait,
              (bridge_cond_t*, bridge_mutex_t*, const struct timespec*),
              &sync_guard.condition,
              &sync_guard.mutex,
              &deadline) == 110);
    CHECK(*error == 95);
    CHECK(CALL(pthread_cond_broadcast, (bridge_cond_t*), &sync_guard.condition) == 0);
    CHECK(CALL(pthread_mutex_destroy, (bridge_mutex_t*), &sync_guard.mutex) == 16);
    CHECK(CALL(pthread_mutex_unlock, (bridge_mutex_t*), &sync_guard.mutex) == 0);
    CHECK(CALL(pthread_cond_destroy, (bridge_cond_t*), &sync_guard.condition) == 0);
    CHECK(CALL(pthread_mutex_destroy, (bridge_mutex_t*), &sync_guard.mutex) == 0);
    CHECK(sync_guard.before == 0x12345678 && sync_guard.middle == 0x87654321 && sync_guard.after == 0xabcdef01);
    CHECK(attribute.before == 0xaabbccdd && attribute.after == 0xddccbbaa);
    sync_guard.mutex = 0x2000;
    CHECK(CALL(pthread_mutex_lock, (bridge_mutex_t*), &sync_guard.mutex) == 95 && sync_guard.mutex == 0x2000);
    sync_guard.mutex = 0xc000;
    CHECK(CALL(pthread_mutex_lock, (bridge_mutex_t*), &sync_guard.mutex) == 22);
    return 0;
}
