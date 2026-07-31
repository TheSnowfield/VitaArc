#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

#include "cxx.h"

typedef struct exit_entry {
    struct exit_entry* next;
    void (*callback)(void*);
    void* argument;
    void* dso;
} exit_entry_t;

static pthread_mutex_t __exit_mutex = PTHREAD_MUTEX_INITIALIZER;
static exit_entry_t* __exit_entries;

/* Arm RTABI orders the object before its destructor, unlike __cxa_atexit. */
int bridge_aeabi_atexit(void* argument, void (*callback)(void*), void* dso)
{
    return bridge_cxa_atexit(callback, argument, dso);
}

int bridge_cxa_atexit(void (*callback)(void*), void* argument, void* dso)
{
    if(!callback) {
        errno = EINVAL;
        return -1;
    }

    exit_entry_t* entry = malloc(sizeof(*entry));
    if(!entry) {
        errno = ENOMEM;
        return -1;
    }

    entry->callback = callback;
    entry->argument = argument;
    entry->dso = dso;
    pthread_mutex_lock(&__exit_mutex);
    entry->next = __exit_entries;
    __exit_entries = entry;
    pthread_mutex_unlock(&__exit_mutex);
    return 0;
}

static int __inside(uintptr_t value, uintptr_t base, size_t size)
{
    return value >= base && value - base < size;
}

static void __finalize(void* dso, uintptr_t base, size_t size)
{
    for(;;) {
        pthread_mutex_lock(&__exit_mutex);
        exit_entry_t** link = &__exit_entries;
        while(*link) {
            exit_entry_t* entry = *link;
            int matches = size
                ? (__inside((uintptr_t)entry->dso, base, size) || __inside((uintptr_t)entry->callback, base, size))
                : (!dso || entry->dso == dso);
            if(matches) {
                break;
            }

            link = &entry->next;
        }

        exit_entry_t* entry = *link;
        if(entry) {
            *link = entry->next;
        }

        pthread_mutex_unlock(&__exit_mutex);
        if(!entry) {
            return;
        }

        void (*callback)(void*) = entry->callback;
        void* argument = entry->argument;
        free(entry);
        /* Remove before calling, and allow nested finalization, registration or
         * another thread's callbacks without holding the registry mutex. */
        callback(argument);
    }
}

void bridge_cxa_finalize(void* dso)
{
    __finalize(dso, 0, 0);
}

void bridge_cxa_finalize_range(uintptr_t base, size_t size)
{
    if(size) {
        __finalize(NULL, base, size);
    }
}

void bridge_exit(int status)
{
    bridge_cxa_finalize(NULL);
    exit(status);
}
