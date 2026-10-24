#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

#include "rwlock.h"

#define RWLOCK_MARK UINT32_C(0x56415257)
#define RWLOCK_DEAD UINT32_C(0xffffffff)

typedef struct rwlock_t {
    struct rwlock_t* next;
    bridge_rwlock_t* guest;
    pthread_rwlock_t native;
    unsigned users;
    unsigned holds;
} rwlock_t;

static pthread_mutex_t __mutex = PTHREAD_MUTEX_INITIALIZER;
static rwlock_t* __locks;

static rwlock_t** __find(const bridge_rwlock_t* guest)
{
    rwlock_t** link = &__locks;
    while(*link && (*link)->guest != guest) {
        link = &(*link)->next;
    }

    return link;
}

static int __create(bridge_rwlock_t* guest)
{
    rwlock_t* lock = calloc(1, sizeof(*lock));
    if(!lock) {
        return ENOMEM;
    }

    int result = pthread_rwlock_init(&lock->native, NULL);
    if(result) {
        free(lock);
        return result;
    }

    lock->guest = guest;
    lock->next = __locks;
    __locks = lock;
    *guest = RWLOCK_MARK;
    return 0;
}

static int __remove(rwlock_t** link)
{
    rwlock_t* lock = *link;
    if(lock->users || lock->holds) {
        return EBUSY;
    }

    int result = pthread_rwlock_destroy(&lock->native);
    if(result) {
        return result;
    }

    *link = lock->next;
    *lock->guest = RWLOCK_DEAD;
    free(lock);
    return 0;
}

static int __valid(const bridge_rwlock_t* lock)
{
    return lock && !((uintptr_t)lock & 3);
}

int bridge_rwlock_init(bridge_rwlock_t* guest, const uint32_t* attributes)
{
    if(!__valid(guest)) {
        return EINVAL;
    }

    if(attributes && *attributes) {
        return *attributes == 1 ? ENOTSUP : EINVAL;
    }

    pthread_mutex_lock(&__mutex);
    int result = *__find(guest) ? EBUSY : __create(guest);
    pthread_mutex_unlock(&__mutex);
    return result;
}

int bridge_rwlock_destroy(bridge_rwlock_t* guest)
{
    if(!__valid(guest)) {
        return EINVAL;
    }

    pthread_mutex_lock(&__mutex);
    rwlock_t** link = __find(guest);
    int result = *link ? __remove(link) : EINVAL;
    if(!*link && !*guest) {
        *guest = RWLOCK_DEAD;
        result = 0;
    }

    pthread_mutex_unlock(&__mutex);
    return result;
}

static int __acquire(bridge_rwlock_t* guest, int write)
{
    if(!__valid(guest)) {
        return EINVAL;
    }

    pthread_mutex_lock(&__mutex);
    rwlock_t* lock = *__find(guest);
    int result = 0;
    if(!lock) {
        result = *guest ? EINVAL : __create(guest);
        lock = *__find(guest);
    }

    if(result) {
        pthread_mutex_unlock(&__mutex);
        return result;
    }

    ++lock->users;
    pthread_mutex_unlock(&__mutex);
    result = write ? pthread_rwlock_wrlock(&lock->native) : pthread_rwlock_rdlock(&lock->native);

    pthread_mutex_lock(&__mutex);
    --lock->users;
    if(!result) {
        ++lock->holds;
    }

    pthread_mutex_unlock(&__mutex);
    return result;
}

int bridge_rwlock_read(bridge_rwlock_t* lock)
{
    return __acquire(lock, 0);
}

int bridge_rwlock_write(bridge_rwlock_t* lock)
{
    return __acquire(lock, 1);
}

int bridge_rwlock_unlock(bridge_rwlock_t* guest)
{
    if(!__valid(guest)) {
        return EINVAL;
    }

    pthread_mutex_lock(&__mutex);
    rwlock_t* lock = *__find(guest);
    int result = EPERM;
    if(lock && lock->holds) {
        result = pthread_rwlock_unlock(&lock->native);
        if(!result) {
            --lock->holds;
        }
    }

    pthread_mutex_unlock(&__mutex);
    return result;
}

int bridge_rwlock_range(uintptr_t base, size_t size, int destroy)
{
    pthread_mutex_lock(&__mutex);
    for(rwlock_t* lock = __locks; lock; lock = lock->next) {
        uintptr_t address = (uintptr_t)lock->guest;
        if(address < base || address - base >= size) {
            continue;
        }

        if(lock->users || lock->holds) {
            pthread_mutex_unlock(&__mutex);
            return EBUSY;
        }
    }

    int result = 0;
    rwlock_t** link = &__locks;
    while(destroy && *link && !result) {
        uintptr_t address = (uintptr_t)(*link)->guest;
        if(address < base || address - base >= size) {
            link = &(*link)->next;
            continue;
        }

        result = __remove(link);
    }

    pthread_mutex_unlock(&__mutex);
    return result;
}
