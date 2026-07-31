#include <stdarg.h>
#include <logcat/logcat.h>

#include "runtime/object.h"
#include "runtime/entry.h"
#include "runtime/reference.h"

jint jni_entry_get_java_vm(JNIEnv* env, JavaVM** vm)
{
    (void)env;
    if(!vm) {
        return JNI_EINVAL;
    }

    *vm = jni_runtime_get_java_vm();
    return JNI_OK;
}

jint jni_entry_get_version(JNIEnv* env)
{
    (void)env;
    return JNI_VERSION_1_6;
}

static jint __destroy_java_vm(JavaVM* java_vm)
{
    (void)java_vm;
    return jni_thread_destroy();
}

static jint __attach_current_thread(JavaVM* java_vm, JNIEnv** jni_env, void* args)
{
    (void)java_vm;
    if(!jni_env) {
        return JNI_EINVAL;
    }

    *jni_env = NULL;
    JavaVMAttachArgs* options = args;
    if(options && !jni_thread_version_supported(options->version)) {
        return JNI_EVERSION;
    }

    return jni_thread_attach(jni_runtime_interface(), false, jni_env);
}

static jint __detach_current_thread(JavaVM* java_vm)
{
    (void)java_vm;
    return jni_thread_detach();
}

static jint __attach_current_thread_as_daemon(JavaVM* java_vm, JNIEnv** jni_env, void* args)
{
    (void)java_vm;
    if(!jni_env) {
        return JNI_EINVAL;
    }

    *jni_env = NULL;
    JavaVMAttachArgs* options = args;
    if(options && !jni_thread_version_supported(options->version)) {
        return JNI_EVERSION;
    }

    return jni_thread_attach(jni_runtime_interface(), true, jni_env);
}

static jint __get_env(JavaVM* java_vm, void** jni_env, jint version)
{
    (void)java_vm;
    if(!jni_env) {
        return JNI_EINVAL;
    }

    *jni_env = NULL;
    if(!jni_thread_version_supported(version)) {
        return JNI_EVERSION;
    }

    jni_thread_lock();
    jni_thread_t* thread = jni_thread_current();
    jint result = JNI_EDETACHED;
    if(!jni_thread_vm_alive()) {
        result = JNI_ERR;
    } else if(thread) {
        *jni_env = &thread->env;
        result = JNI_OK;
    }

    jni_thread_unlock();
    return result;
}

#include "api.h"

static struct JNINativeInterface __fake_native_interface = {
#define JNI_VALUE(type, entry, name, implementation, parameters, arguments) .entry = jni_entry_api_##name,
#define JNI_STATUS JNI_VALUE
#define JNI_REF JNI_VALUE
#define JNI_VOID(entry, name, implementation, parameters, arguments) .entry = jni_entry_api_##name,
#define JNI_VAR_VALUE(type, entry, name, implementation, parameters, arguments, last) .entry = jni_entry_api_##name,
#define JNI_VAR_REF JNI_VAR_VALUE
#define JNI_VAR_VOID(entry, name, implementation, parameters, arguments, last) .entry = jni_entry_api_##name,
#include "api.def"
#undef JNI_VALUE
#undef JNI_STATUS
#undef JNI_REF
#undef JNI_VOID
#undef JNI_VAR_VALUE
#undef JNI_VAR_REF
#undef JNI_VAR_VOID
};

static struct JNIInvokeInterface __fake_invoke_interface = {.DestroyJavaVM = __destroy_java_vm,
    .AttachCurrentThread = __attach_current_thread,
    .DetachCurrentThread = __detach_current_thread,
    .GetEnv = __get_env,
    .AttachCurrentThreadAsDaemon = __attach_current_thread_as_daemon};

static JavaVM __fake_java_vm = &__fake_invoke_interface;

const struct JNINativeInterface* jni_runtime_interface(void)
{
    return &__fake_native_interface;
}

JNIEnv* jni_runtime_get_env(void)
{
    JNIEnv* env = NULL;
    jni_thread_attach(&__fake_native_interface, false, &env);
    return env;
}

JavaVM* jni_runtime_get_java_vm(void)
{
    return &__fake_java_vm;
}
