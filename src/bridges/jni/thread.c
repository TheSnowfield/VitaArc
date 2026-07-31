#include <stdlib.h>
#include <logcat/logcat.h>

#include "thread.h"

static pthread_once_t __once = PTHREAD_ONCE_INIT;
static pthread_key_t __key;
static pthread_mutex_t __mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t __changed = PTHREAD_COND_INITIALIZER;
static jni_thread_t* __threads;
static bool __destroyed;
static jni_thread_t* __destroyer;

static void __remove_thread(jni_thread_t* thread)
{
    jni_runtime_thread_cleanup(thread);
    jni_thread_t** cursor = &__threads;
    while(*cursor && *cursor != thread) {
        cursor = &(*cursor)->next;
    }

    if(*cursor) {
        *cursor = thread->next;
    }

    free(thread);
    if(__destroyed && !__threads) {
        jni_runtime_vm_cleanup();
    }

    pthread_cond_broadcast(&__changed);
}

static void __thread_exited(void* value)
{
    pthread_mutex_lock(&__mutex);
    __remove_thread(value);
    pthread_mutex_unlock(&__mutex);
}

static void __initialize(void)
{
    if(pthread_key_create(&__key, __thread_exited) != 0) {
        abort();
    }
}

jni_thread_t* jni_thread_current(void)
{
    pthread_once(&__once, __initialize);
    return pthread_getspecific(__key);
}

jni_thread_t* jni_thread_list(void)
{
    return __threads;
}

void jni_thread_lock(void)
{
    jni_thread_t* thread = jni_thread_current();
    if(thread && thread->lock_depth) {
        ++thread->lock_depth;
        return;
    }

    pthread_mutex_lock(&__mutex);
    if(thread) {
        thread->lock_depth = 1;
    }
}

void jni_thread_unlock(void)
{
    jni_thread_t* thread = jni_thread_current();
    if(thread && thread->lock_depth > 1) {
        --thread->lock_depth;
        return;
    }

    if(thread) {
        thread->lock_depth = 0;
    }

    pthread_mutex_unlock(&__mutex);
}

void jni_thread_wait(void)
{
    pthread_cond_wait(&__changed, &__mutex);
}

unsigned jni_thread_suspend(void)
{
    jni_thread_t* thread = jni_thread_current();
    unsigned depth = thread->lock_depth;
    thread->lock_depth = 0;
    pthread_mutex_unlock(&__mutex);
    return depth;
}

void jni_thread_resume(unsigned depth)
{
    pthread_mutex_lock(&__mutex);
    jni_thread_current()->lock_depth = depth;
}

void jni_thread_notify(void)
{
    pthread_cond_broadcast(&__changed);
}

bool jni_thread_enter(JNIEnv* env)
{
    jni_thread_lock();
    jni_thread_t* thread = jni_thread_current();
    if(__destroyed || !thread || env != &thread->env) {
        log_e(TAG, "JNIEnv used by a detached or different thread");
        jni_thread_unlock();
        return false;
    }

    if(!thread->calls++) {
        ++thread->call_serial;
    }

    return true;
}

void jni_thread_leave(void)
{
    jni_thread_t* thread = jni_thread_current();
    --thread->calls;
    /* JNI reentry from an unlocked native body is another safe collection
     * boundary. Internal nested calls still hold a deeper runtime lock. */
    if(!thread->calls || thread->lock_depth == 1) {
        jni_runtime_collect();
    }

    jni_thread_unlock();
}

bool jni_thread_version_supported(jint version)
{
    return version == JNI_VERSION_1_1 || version == JNI_VERSION_1_2 || version == JNI_VERSION_1_4 ||
        version == JNI_VERSION_1_6;
}

jint jni_thread_attach(JNIEnv interface, bool daemon, JNIEnv** env)
{
    if(!env) {
        return JNI_EINVAL;
    }

    *env = NULL;
    jni_thread_lock();
    jni_thread_t* thread = jni_thread_current();
    if(__destroyed || (__destroyer && !thread)) {
        jni_thread_unlock();
        return JNI_ERR;
    }

    if(!thread) {
        thread = calloc(1, sizeof(*thread));
        if(!thread) {
            jni_thread_unlock();
            return JNI_ENOMEM;
        }

        thread->env = interface;
        thread->daemon = daemon;
        thread->lock_depth = 1;
        if(pthread_setspecific(__key, thread) != 0) {
            free(thread);
            jni_thread_unlock();
            return JNI_ENOMEM;
        }

        thread->next = __threads;
        __threads = thread;
    }

    *env = &thread->env;
    jni_thread_unlock();
    return JNI_OK;
}

jint jni_thread_detach(void)
{
    jni_thread_lock();
    jni_thread_t* thread = jni_thread_current();
    if(thread && thread->calls) {
        jni_thread_unlock();
        return JNI_ERR;
    }

    if(thread) {
        pthread_setspecific(__key, NULL);
        __remove_thread(thread);
    }

    jni_thread_unlock();
    return JNI_OK;
}

bool jni_thread_vm_alive(void)
{
    return !__destroyed;
}

static bool __has_user_thread(jni_thread_t* current)
{
    for(jni_thread_t* thread = __threads; thread; thread = thread->next) {
        if(thread != current && !thread->daemon) {
            return true;
        }
    }

    return false;
}

jint jni_thread_destroy(void)
{
    jni_thread_lock();
    jni_thread_t* thread = jni_thread_current();
    if(!thread || thread->calls || __destroyed || __destroyer) {
        jni_thread_unlock();
        return JNI_ERR;
    }

    __destroyer = thread;
    while(__has_user_thread(thread)) {
        jni_thread_wait();
    }

    __destroyed = true;
    __destroyer = NULL;
    jni_thread_unlock();
    return jni_thread_detach();
}
