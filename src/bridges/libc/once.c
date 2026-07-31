#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

#include "once.h"

/* Only the 32-bit Bionic state is stored in guest memory. Native pthread
 * synchronization stays in the bridge, independently of the SDK's once ABI. */
enum { ONCE_INITIAL = 0, ONCE_RUNNING = 1, ONCE_DONE = 2 };

typedef struct once_entry {
    struct once_entry* next;
    bridge_once_t* control;
    unsigned users;
    bool running;
} once_entry_t;

static pthread_mutex_t __once_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t __once_changed = PTHREAD_COND_INITIALIZER;
static once_entry_t* __once_entries;
static unsigned __once_waiters;

static void __release(once_entry_t* entry)
{
    if(--entry->users) {
        return;
    }

    once_entry_t** link = &__once_entries;
    while(*link != entry) {
        link = &(*link)->next;
    }

    *link = entry->next;
    free(entry);
}

static void __initializer_exited(void* argument)
{
    once_entry_t* entry = argument;
    pthread_mutex_lock(&__once_mutex);
    entry->running = false;
    __atomic_store_n(entry->control, ONCE_INITIAL, __ATOMIC_RELEASE);
    if(__once_waiters) {
        pthread_cond_broadcast(&__once_changed);
    }

    __release(entry);
    pthread_mutex_unlock(&__once_mutex);
}

static void __waiter_exited(void* argument)
{
    /* pthread_cond_wait reacquires the mutex before cancellation cleanup. */
    --__once_waiters;
    __release(argument);
    pthread_mutex_unlock(&__once_mutex);
}

int _pthread_once(bridge_once_t* control, void (*initialize)(void))
{
    if(!control || !initialize || ((uintptr_t)control & 3)) {
        return EINVAL;
    }

    if(__atomic_load_n(control, __ATOMIC_ACQUIRE) == ONCE_DONE) {
        return 0;
    }

    int saved_error = errno;
    int status = pthread_mutex_lock(&__once_mutex);
    if(status) {
        errno = saved_error;
        return status;
    }

    unsigned state = __atomic_load_n(control, __ATOMIC_ACQUIRE);
    once_entry_t* entry = __once_entries;
    while(entry && entry->control != control) {
        entry = entry->next;
    }

    if(state == ONCE_DONE) {
        pthread_mutex_unlock(&__once_mutex);
        errno = saved_error;
        return 0;
    }

    if(state > ONCE_RUNNING || (state == ONCE_RUNNING && !entry)) {
        pthread_mutex_unlock(&__once_mutex);
        errno = saved_error;
        return EINVAL;
    }

    if(!entry) {
        entry = calloc(1, sizeof(*entry));
        if(!entry) {
            pthread_mutex_unlock(&__once_mutex);
            errno = saved_error;
            return ENOMEM;
        }

        entry->control = control;
        entry->next = __once_entries;
        __once_entries = entry;
    }

    ++entry->users;
    for(;;) {
        if(__atomic_load_n(control, __ATOMIC_ACQUIRE) == ONCE_DONE) {
            __release(entry);
            pthread_mutex_unlock(&__once_mutex);
            errno = saved_error;
            return 0;
        }

        if(!entry->running) {
            break;
        }

        ++__once_waiters;
        pthread_cleanup_push(__waiter_exited, entry);
        status = pthread_cond_wait(&__once_changed, &__once_mutex);
        pthread_cleanup_pop(0);
        --__once_waiters;
        if(status) {
            __release(entry);
            pthread_mutex_unlock(&__once_mutex);
            errno = saved_error;
            return status;
        }
    }

    entry->running = true;
    __atomic_store_n(control, ONCE_RUNNING, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&__once_mutex);
    errno = saved_error;
    pthread_cleanup_push(__initializer_exited, entry);
    initialize();
    pthread_cleanup_pop(0);
    int callback_error = errno;
    pthread_mutex_lock(&__once_mutex);
    entry->running = false;
    __atomic_store_n(control, ONCE_DONE, __ATOMIC_RELEASE);
    if(__once_waiters) {
        pthread_cond_broadcast(&__once_changed);
    }

    __release(entry);
    pthread_mutex_unlock(&__once_mutex);
    errno = callback_error;
    return 0;
}
