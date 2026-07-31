#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

#include "sync.h"
#ifdef __vita__
#include "posix.h"
#endif

#define MUTEX_MARKER UINT32_C(0x56414d58)
#define COND_MARKER UINT32_C(0x56414356)
#define DESTROYED UINT32_C(0xffffffff)

typedef struct sync_entry {
    struct sync_entry* next;
    uint32_t* guest;
    bool condition;
    bool monotonic;
    unsigned users;
    unsigned holds;
    union {
        pthread_mutex_t mutex;
        pthread_cond_t condition;
    } native;
} sync_entry_t;

static pthread_mutex_t __sync_mutex = PTHREAD_MUTEX_INITIALIZER;
static sync_entry_t* __sync_entries;

static bool __valid(const void* pointer)
{
    return pointer && !((uintptr_t)pointer & 3);
}

static sync_entry_t* __find(const void* guest)
{
    sync_entry_t* entry = __sync_entries;
    while(entry && entry->guest != guest) {
        entry = entry->next;
    }

    return entry;
}

static int __static_attributes(uint32_t value, bool condition, uint32_t* attributes)
{
    if(condition) {
        if(value & ~3u) {
            return EINVAL;
        }

        if(value & 1) {
            return ENOTSUP;
        }

        *attributes = value;
        return 0;
    }

    if(value & ~(UINT32_C(0xc000) | UINT32_C(0x2000))) {
        return EINVAL;
    }

    if(value & 0x2000) {
        return ENOTSUP;
    }

    *attributes = value >> 14;
    return *attributes <= 2 ? 0 : EINVAL;
}

static int __mutex_attributes(uint32_t attributes)
{
    if(attributes & ~0x1fu || (attributes & 15) > 2) {
        return EINVAL;
    }

    return attributes & 16 ? ENOTSUP : 0;
}

static int __initialize(sync_entry_t* entry, uint32_t attributes)
{
    if(entry->condition) {
#ifdef __vita__
        /* This SDK accepts condattr_setclock but cond_init/timedwait ignore it.
         * Keep its native condition on realtime and convert bounded waits below. */
        return pthread_cond_init(&entry->native.condition, NULL);

#else
        if(!(attributes & 2)) {
            return pthread_cond_init(&entry->native.condition, NULL);
        }

        pthread_condattr_t native;
        int result = pthread_condattr_init(&native);
        if(result) {
            return result;
        }

        result = pthread_condattr_setclock(&native, CLOCK_MONOTONIC);
        if(!result) {
            result = pthread_cond_init(&entry->native.condition, &native);
        }

        pthread_condattr_destroy(&native);
        return result;

#endif
    }

    static const int types[] = {PTHREAD_MUTEX_NORMAL, PTHREAD_MUTEX_RECURSIVE, PTHREAD_MUTEX_ERRORCHECK};

    pthread_mutexattr_t native;
    int result = pthread_mutexattr_init(&native);
    if(result) {
        return result;
    }

    result = pthread_mutexattr_settype(&native, types[attributes & 15]);
    if(!result) {
        result = pthread_mutex_init(&entry->native.mutex, &native);
    }

    pthread_mutexattr_destroy(&native);
    return result;
}

/* Called with __sync_mutex held. Only a complete native object is published. */
static sync_entry_t* __create(uint32_t* guest, bool condition, uint32_t attributes, int* error)
{
    sync_entry_t* entry = calloc(1, sizeof(*entry));
    if(!entry) {
        *error = ENOMEM;
        return NULL;
    }

    entry->guest = guest;
    entry->condition = condition;
    entry->monotonic = condition && (attributes & 2);
    *error = __initialize(entry, attributes);
    if(*error) {
        free(entry);
        return NULL;
    }

    entry->next = __sync_entries;
    __sync_entries = entry;
    __atomic_store_n(guest, condition ? COND_MARKER : MUTEX_MARKER, __ATOMIC_RELEASE);
    return entry;
}

static sync_entry_t* __acquire(uint32_t* guest, bool condition, int* error)
{
    if(!__valid(guest)) {
        *error = EINVAL;
        return NULL;
    }

    sync_entry_t* entry = __find(guest);
    if(entry && entry->condition != condition) {
        *error = EINVAL;
        return NULL;
    }

    if(!entry) {
        uint32_t attributes;
        *error = __static_attributes(__atomic_load_n(guest, __ATOMIC_ACQUIRE), condition, &attributes);
        if(*error) {
            return NULL;
        }

        entry = __create(guest, condition, attributes, error);
    }

    if(entry) {
        ++entry->users;
    }

    return entry;
}

static int __init(uint32_t* guest, const uint32_t* attributes, bool condition)
{
    if(!__valid(guest) || (attributes && !__valid(attributes))) {
        return EINVAL;
    }

    uint32_t value = attributes ? *attributes : 0;
    int error = condition ? (value & ~3u ? EINVAL : value & 1 ? ENOTSUP : 0) : __mutex_attributes(value);
    if(error) {
        return error;
    }

    pthread_mutex_lock(&__sync_mutex);
    if(__find(guest)) {
        error = EBUSY;
    } else {
        __create(guest, condition, value, &error);
    }

    pthread_mutex_unlock(&__sync_mutex);
    return error;
}

static int __destroy_entry(sync_entry_t** link)
{
    sync_entry_t* entry = *link;
    if(entry->users || entry->holds) {
        return EBUSY;
    }

    int result =
        entry->condition ? pthread_cond_destroy(&entry->native.condition) : pthread_mutex_destroy(&entry->native.mutex);
    if(!result) {
        __atomic_store_n(entry->guest, DESTROYED, __ATOMIC_RELEASE);
        *link = entry->next;
        free(entry);
    }

    return result;
}

static int __destroy(uint32_t* guest, bool condition)
{
    if(!__valid(guest)) {
        return EINVAL;
    }

    pthread_mutex_lock(&__sync_mutex);
    sync_entry_t** link = &__sync_entries;
    while(*link && (*link)->guest != guest) {
        link = &(*link)->next;
    }

    int result;
    if(*link) {
        result = (*link)->condition == condition ? __destroy_entry(link) : EINVAL;
    } else {
        uint32_t ignored;
        result = __static_attributes(__atomic_load_n(guest, __ATOMIC_ACQUIRE), condition, &ignored);
        if(!result) {
            __atomic_store_n(guest, DESTROYED, __ATOMIC_RELEASE);
        }
    }

    pthread_mutex_unlock(&__sync_mutex);
    return result;
}

int _pthread_mutex_init(bridge_mutex_t* mutex, const bridge_mutexattr_t* attributes)
{
    return __init(mutex, attributes, false);
}

int _pthread_cond_init(bridge_cond_t* condition, const bridge_condattr_t* attributes)
{
    return __init(condition, attributes, true);
}

int _pthread_mutex_destroy(bridge_mutex_t* mutex)
{
    return __destroy(mutex, false);
}

int _pthread_cond_destroy(bridge_cond_t* condition)
{
    return __destroy(condition, true);
}

static int __lock(bridge_mutex_t* mutex, int operation, const struct timespec* deadline)
{
    if(operation == 2 && (!deadline || deadline->tv_nsec < 0 || deadline->tv_nsec >= 1000000000)) {
        return EINVAL;
    }

    int error = 0;
    pthread_mutex_lock(&__sync_mutex);
    sync_entry_t* entry = __acquire(mutex, false, &error);
    pthread_mutex_unlock(&__sync_mutex);
    if(!entry) {
        return error;
    }

    int result = operation == 0 ? pthread_mutex_lock(&entry->native.mutex)
        : operation == 1        ? pthread_mutex_trylock(&entry->native.mutex)
                                : pthread_mutex_timedlock(&entry->native.mutex, deadline);
    pthread_mutex_lock(&__sync_mutex);
    if(!result) {
        ++entry->holds;
    }

    --entry->users;
    pthread_mutex_unlock(&__sync_mutex);
    return result;
}

int _pthread_mutex_lock(bridge_mutex_t* mutex)
{
    return __lock(mutex, 0, NULL);
}

int _pthread_mutex_trylock(bridge_mutex_t* mutex)
{
    return __lock(mutex, 1, NULL);
}

int _pthread_mutex_timedlock(bridge_mutex_t* mutex, const struct timespec* deadline)
{
    return __lock(mutex, 2, deadline);
}

int _pthread_mutex_unlock(bridge_mutex_t* mutex)
{
    int error = 0;
    pthread_mutex_lock(&__sync_mutex);
    sync_entry_t* entry = __acquire(mutex, false, &error);
    pthread_mutex_unlock(&__sync_mutex);
    if(!entry) {
        return error;
    }

    int result = pthread_mutex_unlock(&entry->native.mutex);
    pthread_mutex_lock(&__sync_mutex);
    if(!result && entry->holds) {
        --entry->holds;
    }

    --entry->users;
    pthread_mutex_unlock(&__sync_mutex);
    return result;
}

static int __pulse(bridge_cond_t* condition, bool all)
{
    if(!__valid(condition)) {
        return EINVAL;
    }

    int error = 0;
    pthread_mutex_lock(&__sync_mutex);
    sync_entry_t* entry = __find(condition);
    if(!entry) {
        uint32_t ignored;
        error = __static_attributes(__atomic_load_n(condition, __ATOMIC_ACQUIRE), true, &ignored);
    } else if(!entry->condition) {
        error = EINVAL;
        entry = NULL;
    } else {
        ++entry->users;
    }

    pthread_mutex_unlock(&__sync_mutex);
    if(!entry) {
        return error;
    }

    int result = all ? pthread_cond_broadcast(&entry->native.condition) : pthread_cond_signal(&entry->native.condition);
    pthread_mutex_lock(&__sync_mutex);
    --entry->users;
    pthread_mutex_unlock(&__sync_mutex);
    return result;
}

int _pthread_cond_signal(bridge_cond_t* condition)
{
    return __pulse(condition, false);
}

int _pthread_cond_broadcast(bridge_cond_t* condition)
{
    return __pulse(condition, true);
}

typedef struct wait_scope {
    sync_entry_t* condition;
    sync_entry_t* mutex;
} wait_scope_t;

static void __wait_finished(void* argument)
{
    wait_scope_t* scope = argument;
    pthread_mutex_lock(&__sync_mutex);
    --scope->condition->users;
    --scope->mutex->users;
    pthread_mutex_unlock(&__sync_mutex);
}

static int __timed_wait(wait_scope_t* scope, const struct timespec* deadline)
{
#ifdef __vita__
    if(scope->condition->monotonic) {
        for(;;) {
            struct timespec now, realtime;
            if(bridge_clock_gettime(1, &now) || bridge_clock_gettime(0, &realtime)) {
                return errno ? errno : EIO;
            }

            int64_t remaining =
                ((int64_t)deadline->tv_sec - now.tv_sec) * INT64_C(1000000000) + deadline->tv_nsec - now.tv_nsec;
            /* Rebase long waits regularly. Native timedwait converts realtime to
             * a relative kernel wait; there remains a wall-clock sampling race. */
            int64_t duration = remaining <= 0 ? 0 : remaining < 100000000 ? remaining : 100000000;
            int64_t nanoseconds = realtime.tv_nsec + duration;
            int64_t seconds = (int64_t)realtime.tv_sec + nanoseconds / 1000000000;
            if(seconds > INT32_MAX) {
                return EOVERFLOW;
            }

            struct timespec converted = {(time_t)seconds, (long)(nanoseconds % 1000000000)};

            int result =
                pthread_cond_timedwait(&scope->condition->native.condition, &scope->mutex->native.mutex, &converted);
            if(result != ETIMEDOUT || remaining <= 0) {
                return result;
            }
        }
    }

#endif
    return pthread_cond_timedwait(&scope->condition->native.condition, &scope->mutex->native.mutex, deadline);
}

static int __wait(bridge_cond_t* condition, bridge_mutex_t* mutex, const struct timespec* deadline)
{
    if(!__valid(condition) || !__valid(mutex) ||
        (deadline && (deadline->tv_nsec < 0 || deadline->tv_nsec >= 1000000000))) {
        return EINVAL;
    }

    int error = 0;
    wait_scope_t scope = {0};

    pthread_mutex_lock(&__sync_mutex);
    scope.mutex = __find(mutex);
    if(!scope.mutex || scope.mutex->condition || !scope.mutex->holds) {
        error = EPERM;
    } else {
        scope.condition = __acquire(condition, true, &error);
        if(scope.condition) {
            ++scope.mutex->users;
        }
    }

    pthread_mutex_unlock(&__sync_mutex);
    if(error) {
        return error;
    }

    int result;
    pthread_cleanup_push(__wait_finished, &scope);
    result = deadline ? __timed_wait(&scope, deadline)
                      : pthread_cond_wait(&scope.condition->native.condition, &scope.mutex->native.mutex);
    pthread_cleanup_pop(1);
    return result;
}

int _pthread_cond_wait(bridge_cond_t* condition, bridge_mutex_t* mutex)
{
    return __wait(condition, mutex, NULL);
}

int _pthread_cond_timedwait(bridge_cond_t* condition, bridge_mutex_t* mutex, const struct timespec* deadline)
{
    return deadline ? __wait(condition, mutex, deadline) : EINVAL;
}

int _pthread_mutexattr_init(bridge_mutexattr_t* attributes)
{
    if(!__valid(attributes)) {
        return EINVAL;
    }

    *attributes = 0;
    return 0;
}

int _pthread_mutexattr_destroy(bridge_mutexattr_t* attributes)
{
    if(!__valid(attributes) || __mutex_attributes(*attributes) == EINVAL) {
        return EINVAL;
    }

    *attributes = DESTROYED;
    return 0;
}

int _pthread_mutexattr_settype(bridge_mutexattr_t* attributes, int type)
{
    if(!__valid(attributes) || __mutex_attributes(*attributes) == EINVAL || type < 0 || type > 2) {
        return EINVAL;
    }

    *attributes = (*attributes & ~15u) | (unsigned)type;
    return 0;
}

int _pthread_mutexattr_gettype(const bridge_mutexattr_t* attributes, int* type)
{
    if(!__valid(attributes) || !type || __mutex_attributes(*attributes) == EINVAL) {
        return EINVAL;
    }

    *type = *attributes & 15;
    return 0;
}

int _pthread_mutexattr_setpshared(bridge_mutexattr_t* attributes, int shared)
{
    if(!__valid(attributes) || __mutex_attributes(*attributes) == EINVAL || (shared != 0 && shared != 1)) {
        return EINVAL;
    }

    *attributes = (*attributes & ~16u) | (shared ? 16u : 0);
    return 0;
}

int _pthread_condattr_init(bridge_condattr_t* attributes)
{
    if(!__valid(attributes)) {
        return EINVAL;
    }

    *attributes = 0;
    return 0;
}

int _pthread_condattr_destroy(bridge_condattr_t* attributes)
{
    if(!__valid(attributes) || (*attributes & ~3u)) {
        return EINVAL;
    }

    *attributes = DESTROYED;
    return 0;
}

int _pthread_condattr_setclock(bridge_condattr_t* attributes, int clock)
{
    if(!__valid(attributes) || (*attributes & ~3u) || (clock != 0 && clock != 1)) {
        return EINVAL;
    }

    *attributes = (*attributes & ~2u) | (clock ? 2u : 0);
    return 0;
}

static bool __in_range(sync_entry_t* entry, uintptr_t base, size_t size)
{
    uintptr_t address = (uintptr_t)entry->guest;
    return address >= base && address - base < size;
}

int bridge_sync_range_busy(uintptr_t base, size_t size)
{
    int result = 0;
    pthread_mutex_lock(&__sync_mutex);
    for(sync_entry_t* entry = __sync_entries; entry; entry = entry->next) {
        if(__in_range(entry, base, size) && (entry->users || entry->holds)) {
            result = EBUSY;
            break;
        }
    }

    pthread_mutex_unlock(&__sync_mutex);
    return result;
}

int bridge_sync_destroy_range(uintptr_t base, size_t size)
{
    int result = 0;
    pthread_mutex_lock(&__sync_mutex);
    for(sync_entry_t* entry = __sync_entries; entry; entry = entry->next) {
        if(__in_range(entry, base, size) && (entry->users || entry->holds)) {
            result = EBUSY;
            break;
        }
    }

    sync_entry_t** link = &__sync_entries;
    while(!result && *link) {
        if(__in_range(*link, base, size)) {
            result = __destroy_entry(link);
        } else {
            link = &(*link)->next;
        }
    }

    pthread_mutex_unlock(&__sync_mutex);
    return result;
}
