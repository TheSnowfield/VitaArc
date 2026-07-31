#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_int ready;
static int value;

static void cleanup(void* unused)
{
    (void)unused;
    assert(pthread_mutex_lock(&lock) == 0);
    value = 1;
    assert(pthread_mutex_unlock(&lock) == 0);
}

static void* worker(void* unused)
{
    (void)unused;
    const struct timespec duration = {.tv_sec = 3600};

    pthread_cleanup_push(cleanup, 0);
    atomic_store(&ready, 1);
    nanosleep(&duration, 0);
    pthread_cleanup_pop(0);
    return 0;
}

int main(void)
{
    pthread_t thread;
    assert(pthread_create(&thread, 0, worker, 0) == 0);
    while(!atomic_load(&ready)) {
    }

    assert(pthread_mutex_lock(&lock) == 0);
    assert(value == 0);
    assert(pthread_mutex_unlock(&lock) == 0);
    const struct timespec delay = {.tv_nsec = 10000000};

    nanosleep(&delay, 0);
    assert(pthread_cancel(thread) == 0);
    assert(pthread_join(thread, 0) == 0);
    assert(value == 1);
}
