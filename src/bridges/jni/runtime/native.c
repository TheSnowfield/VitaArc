#include <stdlib.h>
#include <string.h>
#include <logcat/logcat.h>

#include "object.h"
#include "entry.h"
#include "class.h"
#include "exception.h"
#include "reference.h"
#include "native.h"
#include "../native_call.h"

jni_obj_t* jni_binding(jni_obj_t* clazz, const char* name, const char* signature)
{
    for(jni_obj_t* object = jni_heap_head(); object; object = object->next) {
        if(object->kind != JNI_KIND_NATIVE)
            continue;

        if(object->clazz != clazz)
            continue;

        if(!jni_named(object, name, signature))
            continue;

        return object;
    }

    return NULL;
}

jni_obj_t* jni_native_decl(JNIEnv* env, jni_obj_t* method)
{
    if(method->decl) {
        return method->decl;
    }

    if(strcmp(method->name, "<init>") == 0) {
        return method;
    }

    for(jni_obj_t* owner = method->clazz; owner; owner = jni_super(env, owner)) {
        if(!jni_binding(owner, method->name, method->signature)) {
            continue;
        }

        if(owner == method->clazz) {
            return method;
        }

        /* A method ID synthesized before registration can discover its inherited
         * declaration later. Keep it as a stable alias, separate from any future
         * declaration introduced by an override in the original lookup class. */
        jni_obj_t* declared =
            jni_member((jclass)owner, method->name, method->signature, JNI_KIND_METHOD, method->is_static);
        if(!declared) {
            jni_raise(env, JEX_MEMORY, "native declaration allocation failed");
            return NULL;
        }

        method->decl = declared;
        return declared;
    }

    return method;
}

bool jni_native_run(
    JNIEnv* env, jobject receiver, jni_obj_t* method, const jni_sig_t* sig, const jvalue* arguments, jvalue* result)
{
    jni_obj_t* binding = jni_binding(method->clazz, method->name, method->signature);
    if(!binding) {
        return false;
    }

    void* proc = binding->proc;
    if(!proc) {
        jni_raise(env, JEX_LINK, method->name);
        return true;
    }

    jni_thread_t* thread = jni_thread_current();
    if(!jni_refs_enter_native(thread, (jint)sig->argc + 17)) {
        jni_raise(env, JEX_MEMORY, "native local frame allocation failed");
        return true;
    }

    /* Operand roots survive deletion of either caller or callee local handles. */
    jni_native_roots_t roots = {.parent = thread->native_roots};

    roots.objects[roots.count++] = jni_obj_get(receiver);
    thread->native_roots = &roots;
    jobject native_receiver = jni_refs_new(roots.objects[0], JNILocalRefType);
    jvalue native_args[JNI_MAX_CALL_ARGS];
    bool ready = native_receiver != NULL;
    for(size_t index = 0; ready && index < sig->argc; ++index) {
        native_args[index] = arguments[index];
        if(sig->args[index] != 'L') {
            continue;
        }

        jni_obj_t* object = jni_obj_get(arguments[index].l);
        if(arguments[index].l && !object && jni_refs_type(arguments[index].l) != JNIWeakGlobalRefType) {
            jni_raise(env, JEX_ARG, "invalid native object argument");
            ready = false;
            break;
        }

        roots.objects[roots.count++] = object;
        native_args[index].l = jni_refs_new(object, JNILocalRefType);
        ready = !object || native_args[index].l;
    }

    if(!ready) {
        jni_raise(env, JEX_MEMORY, "native argument reference allocation failed");
    } else {
        unsigned depth = jni_thread_suspend();
        bool called = jni_native_call(proc, env, native_receiver, sig, native_args, result);
        jni_thread_resume(depth);
        if(!called) {
            jni_raise(env, JEX_ARG, "unsupported native call descriptor");
        }
    }

    jni_obj_t* returned = NULL;
    if(!JNI_PENDING_EXCEPTION && sig->ret == 'L') {
        returned = jni_obj_get(result->l);
        if(result->l && !returned && jni_refs_type(result->l) != JNIWeakGlobalRefType) {
            jni_raise(env, JEX_ARG, "invalid native result reference");
        }
    }

    jni_refs_leave_native(thread);
    if(JNI_PENDING_EXCEPTION) {
        memset(result, 0, sizeof(*result));
    } else if(sig->ret == 'L') {
        result->l = jni_local(env, (jobject)returned);
    }

    thread->native_roots = roots.parent;
    return true;
}

jint jni_entry_register_natives(JNIEnv* env, jclass clazz, const JNINativeMethod* methods, jint method_count)
{
    (void)env;
    jni_obj_t* class_object = jni_obj_get(clazz);
    if(!class_object || class_object->kind != JNI_KIND_CLASS || (!methods && method_count) || method_count < 0) {
        return JNI_EINVAL;
    }

    for(jint index = 0; index < method_count; ++index) {
        jni_sig_t sig;
        if(!methods[index].name || !*methods[index].name || *methods[index].name == '<' || !methods[index].fnPtr ||
            !jni_sig_parse(methods[index].signature, &sig)) {
            return JNI_EINVAL;
        }
    }

    if(!method_count) {
        return JNI_OK;
    }

    if((size_t)method_count > SIZE_MAX / sizeof(jni_obj_t*)) {
        return JNI_ENOMEM;
    }

    jni_obj_t** bindings = calloc((size_t)method_count, sizeof(*bindings));
    if(!bindings) {
        return JNI_ENOMEM;
    }

    for(jint index = 0; index < method_count; ++index) {
        bindings[index] = jni_member(clazz, methods[index].name, methods[index].signature, JNI_KIND_NATIVE, JNI_FALSE);
        if(!bindings[index]) {
            free(bindings);
            return JNI_ENOMEM;
        }
    }

    for(jint index = 0; index < method_count; ++index) {
        bindings[index]->proc = methods[index].fnPtr;
        log_v(TAG,
            "JNI RegisterNatives: %s %s -> %p",
            methods[index].name,
            methods[index].signature,
            methods[index].fnPtr);
    }

    free(bindings);
    return JNI_OK;
}

void* jni_runtime_find_native(const char* class_name, const char* name, const char* signature)
{
    jni_thread_lock();
    void* proc = NULL;
    for(jni_obj_t* method = jni_heap_head(); method; method = method->next) {
        if(method->kind == JNI_KIND_NATIVE && method->proc && method->clazz && method->clazz->name &&
            strcmp(method->clazz->name, class_name) == 0 && strcmp(method->name, name) == 0 &&
            strcmp(method->signature, signature) == 0) {
            proc = method->proc;
            break;
        }
    }

    jni_thread_unlock();
    return proc;
}

void jni_runtime_clear_natives(void)
{
    jni_thread_lock();
    for(jni_obj_t* method = jni_heap_head(); method; method = method->next) {
        method->proc = NULL;
    }

    jni_thread_unlock();
}

jint jni_entry_unregister_natives(JNIEnv* env, jclass clazz)
{
    (void)env;
    jni_obj_t* class_object = jni_obj_get(clazz);
    if(!class_object || class_object->kind != JNI_KIND_CLASS) {
        return JNI_EINVAL;
    }

    for(jni_obj_t* object = jni_heap_head(); object; object = object->next) {
        if(object->kind == JNI_KIND_NATIVE && object->clazz == class_object) {
            object->proc = NULL;
        }
    }

    return JNI_OK;
}
