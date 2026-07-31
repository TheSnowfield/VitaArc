#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>

#include <jni/runtime.h>

static JavaVM* __vm;
static JNIEnv* __main_env;
static jobject __object;
static jstring __foreign_local;
static jfieldID __counter;
static pthread_barrier_t __barrier;
static atomic_int __attached;
static atomic_int __detached;
static atomic_bool __release;

static void __expect_exception(JNIEnv* env, const char* name)
{
    jthrowable exception = (*env)->ExceptionOccurred(env);
    assert(exception != NULL);
    assert((*env)->IsInstanceOf(env, exception, (*env)->FindClass(env, name)));
    (*env)->ExceptionClear(env);
}

static void* __increment(void* value)
{
    (void)value;
    JNIEnv* env = (JNIEnv*)1;
    assert((*__vm)->GetEnv(__vm, (void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED && env == NULL);
    JavaVMAttachArgs options = {.version = -1};

    assert((*__vm)->AttachCurrentThread(__vm, &env, &options) == JNI_EVERSION && env == NULL);
    options.version = JNI_VERSION_1_6;
    assert((*__vm)->AttachCurrentThread(__vm, &env, &options) == JNI_OK && env != __main_env);
    JNIEnv* again = NULL;
    assert((*__vm)->AttachCurrentThreadAsDaemon(__vm, &again, NULL) == JNI_OK && again == env);
    assert((*__vm)->GetEnv(__vm, (void**)&again, JNI_VERSION_1_6) == JNI_OK && again == env);
    assert((*__main_env)->GetVersion(__main_env) == 0);
    assert((*__main_env)->MonitorEnter(__main_env, __object) == JNI_ERR);
    assert((*env)->GetObjectRefType(env, __foreign_local) == JNIInvalidRefType);
    assert((*env)->NewLocalRef(env, __foreign_local) == NULL);
    assert((*env)->ThrowNew(env, (*env)->FindClass(env, "java/io/IOException"), "worker exception") == JNI_OK);
    pthread_barrier_wait(&__barrier);
    pthread_barrier_wait(&__barrier);
    __expect_exception(env, "java/io/IOException");
    for(int index = 0; index < 500; ++index) {
        assert((*env)->PushLocalFrame(env, 8) == JNI_OK);
        assert((*env)->NewStringUTF(env, "worker temporary") != NULL);
        assert((*env)->MonitorEnter(env, __object) == JNI_OK);
        assert((*env)->MonitorEnter(env, __object) == JNI_OK);
        jint current = (*env)->GetIntField(env, __object, __counter);
        (*env)->SetIntField(env, __object, __counter, current + 1);
        assert((*env)->MonitorExit(env, __object) == JNI_OK);
        (*env)->PopLocalFrame(env, NULL);
        assert((*env)->MonitorExit(env, __object) == JNI_OK);
    }

    assert((*env)->MonitorExit(env, __object) == JNI_ERR);
    __expect_exception(env, "java/lang/IllegalMonitorStateException");
    assert((*__vm)->DetachCurrentThread(__vm) == JNI_OK);
    again = (JNIEnv*)1;
    assert((*__vm)->GetEnv(__vm, (void**)&again, JNI_VERSION_1_6) == JNI_EDETACHED && again == NULL);
    assert((*__vm)->DetachCurrentThread(__vm) == JNI_OK);
    return NULL;
}

static void* __leave_owned_monitor(void* value)
{
    JNIEnv* env = NULL;
    assert((*__vm)->AttachCurrentThread(__vm, &env, NULL) == JNI_OK);
    assert((*env)->MonitorEnter(env, __object) == JNI_OK);
    if(value) {
        assert((*__vm)->DetachCurrentThread(__vm) == JNI_OK);
    }

    return NULL;
}

static void __test_threads(void)
{
    jclass clazz = (*__main_env)->FindClass(__main_env, "test/SharedCounter");
    __foreign_local = (*__main_env)->NewStringUTF(__main_env, "owner thread only");
    jobject local = (*__main_env)->AllocObject(__main_env, clazz);
    __object = (*__main_env)->NewGlobalRef(__main_env, local);
    __counter = (*__main_env)->GetFieldID(__main_env, clazz, "count", "I");
    assert((*__main_env)->MonitorEnter(__main_env, NULL) == JNI_ERR);
    __expect_exception(__main_env, "java/lang/NullPointerException");
    pthread_t workers[4];
    assert(pthread_barrier_init(&__barrier, NULL, 5) == 0);
    for(size_t index = 0; index < 4; ++index) {
        assert(pthread_create(&workers[index], NULL, __increment, NULL) == 0);
    }

    pthread_barrier_wait(&__barrier);
    assert(!(*__main_env)->ExceptionCheck(__main_env));
    (*__main_env)
        ->ThrowNew(__main_env, (*__main_env)->FindClass(__main_env, "java/lang/RuntimeException"), "main exception");
    pthread_barrier_wait(&__barrier);
    for(size_t index = 0; index < 4; ++index) {
        assert(pthread_join(workers[index], NULL) == 0);
    }

    __expect_exception(__main_env, "java/lang/RuntimeException");
    assert((*__main_env)->GetIntField(__main_env, __object, __counter) == 2000);
    pthread_barrier_destroy(&__barrier);
    for(int explicit_detach = 0; explicit_detach < 2; ++explicit_detach) {
        pthread_t worker;
        assert(pthread_create(&worker, NULL, __leave_owned_monitor, (void*)(intptr_t)explicit_detach) == 0);
        assert(pthread_join(worker, NULL) == 0);
        assert((*__main_env)->MonitorEnter(__main_env, __object) == JNI_OK);
        assert((*__main_env)->MonitorExit(__main_env, __object) == JNI_OK);
    }

    (*__main_env)->DeleteGlobalRef(__main_env, __object);
}

static void* __shutdown_worker(void* daemon)
{
    JNIEnv* env = NULL;
    jint result = daemon ? (*__vm)->AttachCurrentThreadAsDaemon(__vm, &env, NULL)
                         : (*__vm)->AttachCurrentThread(__vm, &env, NULL);
    assert(result == JNI_OK);
    atomic_fetch_add(&__attached, 1);
    while(!atomic_load(&__release)) {
        struct timespec wait = {.tv_nsec = 1000000};

        nanosleep(&wait, NULL);
    }

    assert((*__vm)->DetachCurrentThread(__vm) == JNI_OK);
    atomic_fetch_add(&__detached, 1);
    return NULL;
}

static void* __release_worker(void* unused)
{
    (void)unused;
    struct timespec wait = {.tv_nsec = 50000000};

    nanosleep(&wait, NULL);
    atomic_store(&__release, true);
    return NULL;
}

static void __test_shutdown(bool daemon)
{
    jobject value = (*__main_env)->NewStringUTF(__main_env, "VM lifetime");
    assert((*__main_env)->NewGlobalRef(__main_env, value) != NULL);
    pthread_t worker, releaser;
    assert(pthread_create(&worker, NULL, __shutdown_worker, (void*)(intptr_t)daemon) == 0);
    while(!atomic_load(&__attached)) {
        struct timespec wait = {.tv_nsec = 1000000};

        nanosleep(&wait, NULL);
    }

    if(!daemon) {
        assert(pthread_create(&releaser, NULL, __release_worker, NULL) == 0);
    }

    assert((*__vm)->DestroyJavaVM(__vm) == JNI_OK);
    JNIEnv* env = (JNIEnv*)1;
    assert((*__vm)->GetEnv(__vm, (void**)&env, JNI_VERSION_1_6) == JNI_ERR && env == NULL);
    assert((*__vm)->AttachCurrentThread(__vm, &env, NULL) == JNI_ERR && env == NULL);
    if(daemon) {
        assert(atomic_load(&__detached) == 0);
        atomic_store(&__release, true);
    } else {
        assert(pthread_join(releaser, NULL) == 0);
    }

    assert(pthread_join(worker, NULL) == 0);
    assert(atomic_load(&__detached) == 1);
    jni_runtime_stats_t stats;
    jni_runtime_get_stats(&stats);
    assert(stats.objects == 0 && stats.references == 0);
}

int main(int argc, char** argv)
{
    __vm = jni_runtime_get_java_vm();
    __main_env = jni_runtime_get_env();
    assert(__main_env != NULL);
    if(argc == 1) {
        __test_threads();
        assert(!(*__main_env)->ExceptionCheck(__main_env));
        assert((*__vm)->DetachCurrentThread(__vm) == JNI_OK);
    } else {
        __test_shutdown(strcmp(argv[1], "daemon") == 0);
    }

    return 0;
}
