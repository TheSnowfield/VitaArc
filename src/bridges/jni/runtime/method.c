#include <stdarg.h>
#include <string.h>
#include <logcat/logcat.h>

#include "object.h"
#include "entry.h"
#include "class.h"
#include "exception.h"
#include "method.h"
#include "services.h"
#include "native.h"
#include "../cocos.h"
#include "../descriptor.h"

bool jni_sig_check(JNIEnv* env, const char* name, const char* signature)
{
    jni_sig_t sig;
    if(!name || !jni_sig_parse(signature, &sig)) {
        jni_raise(env, JEX_ARG, "invalid JNI method descriptor");
        return false;
    }

    if(strcmp(name, "<init>") == 0 && sig.ret != 'V') {
        jni_raise(env, JEX_ARG, "constructor must return void");
        return false;
    }

    return true;
}

static jmethodID __lookup_method(JNIEnv* env, jclass clazz, const char* name, const char* signature, jboolean is_static)
{
    jni_sig_t sig;
    if(!jni_sig_check(env, name, signature)) {
        return NULL;
    }

    jni_sig_parse(signature, &sig);
    if(!is_static && sig.slots == 255) {
        jni_raise(env, JEX_ARG, "instance method exceeds parameter slot limit");
        return NULL;
    }

    /* Reuse declarations known in the supported parent hierarchy. Constructors
     * are never inherited. Native declarations have no static bit until looked up. */
    for(jni_obj_t* owner = jni_obj_get(clazz); owner; owner = jni_super(env, owner)) {
        if(jni_binding(owner, name, signature)) {
            return (jmethodID)jni_member((jclass)owner, name, signature, JNI_KIND_METHOD, is_static);
        }

        for(jni_obj_t* method = jni_heap_head(); method; method = method->next) {
            if(method->kind != JNI_KIND_METHOD)
                continue;

            if(method->clazz != owner)
                continue;

            if(method->is_static != is_static)
                continue;

            if(!jni_named(method, name, signature))
                continue;

            return (jmethodID)jni_native_decl(env, method);
        }

        if(strcmp(name, "<init>") == 0) {
            break;
        }
    }

    return (jmethodID)jni_member(clazz, name, signature, JNI_KIND_METHOD, is_static);
}

jmethodID jni_entry_get_method_id(JNIEnv* env, jclass clazz, const char* name, const char* signature)
{
    (void)env;
    if(!clazz || !name || !signature) {
        return NULL;
    }

    log_v(TAG, "JNI GetMethodID(\"%s\", \"%s\")", name, signature);
    return __lookup_method(env, clazz, name, signature, JNI_FALSE);
}

jmethodID jni_entry_get_static_method_id(JNIEnv* env, jclass clazz, const char* name, const char* signature)
{
    (void)env;
    if(!clazz || !name || !signature) {
        return NULL;
    }

    log_v(TAG, "JNI GetStaticMethodID(\"%s\", \"%s\")", name, signature);
    return __lookup_method(env, clazz, name, signature, JNI_TRUE);
}

bool jni_method_is(jni_obj_t* method, const char* name, const char* signature)
{
    if(strcmp(method->name, name) != 0) {
        return false;
    }

    return strcmp(method->signature, signature) == 0;
}

bool jni_owner_is(JNIEnv* env, jni_obj_t* method, const char* class_name)
{
    jni_obj_t* clazz = jni_obj_get(jni_entry_find_class(env, class_name));
    return jni_assignable(env, method->clazz, clazz);
}

static jni_obj_t* __require_method(
    JNIEnv* env, jmethodID method_id, jboolean is_static, char result_type, jni_sig_t* sig)
{
    jni_obj_t* method = jni_obj_get(method_id);
    if(!method || method->kind != JNI_KIND_METHOD) {
        jni_raise(env, JEX_ARG, "invalid JNI method");
        return NULL;
    }

    if(method->is_static != is_static) {
        jni_raise(env, JEX_ARG, "static/instance method mismatch");
        return NULL;
    }

    if(!jni_sig_parse(method->signature, sig)) {
        jni_raise(env, JEX_ARG, "invalid JNI method descriptor");
        return NULL;
    }

    if(sig->ret != result_type) {
        jni_raise(env, JEX_ARG, "JNI return type mismatch");
        return NULL;
    }

    return jni_native_decl(env, method);
}

static bool __require_arguments(JNIEnv* env, const jni_sig_t* sig, const jvalue* args)
{
    if(sig->argc && !args) {
        jni_raise(env, JEX_ARG, "missing JNI arguments");
        return false;
    }

    return true;
}

static void __unsupported_method(JNIEnv* env, jni_obj_t* method)
{
    log_w(TAG, "Unsupported JNI method: %s.%s%s", method->clazz->name, method->name, method->signature);
    jni_raise(env, JEX_UNSUPPORTED, method->name);
}

static jni_obj_t* __resolve_virtual_method(JNIEnv* env, jni_obj_t* object, jni_obj_t* method)
{
    jni_obj_t* clazz = jni_classof(env, object);
    if(!clazz) {
        return NULL;
    }

    for(jni_obj_t* owner = clazz; owner; owner = jni_super(env, owner)) {
        if(jni_binding(owner, method->name, method->signature)) {
            return jni_member((jclass)owner, method->name, method->signature, JNI_KIND_METHOD, JNI_FALSE);
        }

        if(owner == method->clazz) {
            break;
        }
    }

    bool overrides = false;
    if(object->kind == JNI_KIND_STRING) {
        overrides = jni_method_is(method, "equals", "(Ljava/lang/Object;)Z") ||
            jni_method_is(method, "toString", "()Ljava/lang/String;");
    } else if(strcmp(clazz->name, JCLS_FILE) == 0) {
        overrides = jni_method_is(method, "toString", "()Ljava/lang/String;");
    }

    if(!overrides) {
        return method;
    }

    return jni_member((jclass)clazz, method->name, method->signature, JNI_KIND_METHOD, JNI_FALSE);
}

static jvalue __dispatch_instance(
    JNIEnv* env, jobject receiver, jclass clazz, jmethodID method_id, const jvalue* args, char result_type)
{
    jvalue result = {0};

    if(JNI_PENDING_EXCEPTION) {
        return result;
    }

    jni_sig_t sig;
    jni_obj_t* method = __require_method(env, method_id, JNI_FALSE, result_type, &sig);
    if(!method) {
        return result;
    }

    if(!__require_arguments(env, &sig, args)) {
        return result;
    }

    if(!receiver) {
        jni_raise(env, JEX_NULL, "JNI receiver is null");
        return result;
    }

    jni_obj_t* object = jni_obj_get(receiver);
    if(!jni_instanceof(env, receiver, method->clazz)) {
        jni_raise(env, JEX_ARG, "JNI receiver class mismatch");
        return result;
    }

    if(clazz) {
        jni_obj_t* owner = jni_obj_get(clazz);
        if(!jni_assignable(env, owner, method->clazz)) {
            jni_raise(env, JEX_ARG, "nonvirtual declaring class mismatch");
            return result;
        }

        if(!jni_instanceof(env, receiver, owner)) {
            jni_raise(env, JEX_ARG, "nonvirtual declaring class mismatch");
            return result;
        }
    }

    if(!clazz) {
        method = __resolve_virtual_method(env, object, method);
        if(!method) {
            return result;
        }
    }

    if(jni_native_run(env, receiver, method, &sig, args, &result)) {
        return result;
    }

    if(jni_service_call(env, object, method, args, &result)) {
        return result;
    }

    __unsupported_method(env, method);

    return result;
}

static jvalue __dispatch_virtual(JNIEnv* env, jobject receiver, jmethodID method, const jvalue* args, char result_type)
{
    return __dispatch_instance(env, receiver, NULL, method, args, result_type);
}

static jvalue __dispatch_nonvirtual(
    JNIEnv* env, jobject receiver, jclass clazz, jmethodID method, const jvalue* args, char result_type)
{
    if(!clazz) {
        jni_raise(env, JEX_ARG, "nonvirtual class is null");
        return (jvalue){0};
    }

    return __dispatch_instance(env, receiver, clazz, method, args, result_type);
}

static jvalue __dispatch_static(JNIEnv* env, jclass clazz, jmethodID method_id, const jvalue* args, char result_type)
{
    jvalue result = {0};

    if(JNI_PENDING_EXCEPTION) {
        return result;
    }

    jni_sig_t sig;
    jni_obj_t* method = __require_method(env, method_id, JNI_TRUE, result_type, &sig);
    if(!method) {
        return result;
    }

    if(!__require_arguments(env, &sig, args)) {
        return result;
    }

    if(!jni_assignable(env, jni_obj_get(clazz), method->clazz)) {
        jni_raise(env, JEX_ARG, "static JNI class mismatch");
        return result;
    }

    if(jni_native_run(env, (jobject)method->clazz, method, &sig, args, &result)) {
        return result;
    }

    if(jni_cocos_dispatch(env, method->clazz->name, method->name, method->signature, args, &result)) {
        return result;
    }

    __unsupported_method(env, method);

    return result;
}

/* Validate the complete descriptor before consuming any variadic arguments. */
static bool __decode_arguments(JNIEnv* env, jmethodID method, jboolean is_static, char result_type, va_list incoming,
    jvalue args[JNI_MAX_CALL_ARGS])
{
    if(JNI_PENDING_EXCEPTION) {
        return false;
    }

    jni_sig_t sig;
    if(!__require_method(env, method, is_static, result_type, &sig)) {
        return false;
    }

    va_list values;
    va_copy(values, incoming);
    for(size_t index = 0; index < sig.argc; ++index) {
        switch(sig.args[index]) {
            case 'Z':
                args[index].z = (jboolean)va_arg(values, int);
                break;

            case 'B':
                args[index].b = (jbyte)va_arg(values, int);
                break;

            case 'C':
                args[index].c = (jchar)va_arg(values, int);
                break;

            case 'S':
                args[index].s = (jshort)va_arg(values, int);
                break;

            case 'I':
                args[index].i = va_arg(values, jint);
                break;

            case 'J':
                args[index].j = va_arg(values, jlong);
                break;

            case 'F':
                args[index].f = (jfloat)va_arg(values, double);
                break;

            case 'D':
                args[index].d = va_arg(values, jdouble);
                break;

            case 'L':
                args[index].l = va_arg(values, jobject);
                break;
        }
    }

    va_end(values);
    return true;
}

jobject jni_entry_new_object_a(JNIEnv* env, jclass clazz, jmethodID method_id, const jvalue* args)
{
    if(JNI_PENDING_EXCEPTION) {
        return NULL;
    }

    jni_sig_t sig;
    jni_obj_t* method = __require_method(env, method_id, JNI_FALSE, 'V', &sig);
    if(!method || !__require_arguments(env, &sig, args)) {
        return NULL;
    }

    if(strcmp(method->name, "<init>") != 0 || method->clazz != jni_obj_get(clazz)) {
        jni_raise(env, JEX_ARG, "invalid constructor");
        return NULL;
    }

    jni_obj_t* object = jni_obj_new(env, clazz);
    if(!object) {
        return NULL;
    }

    __dispatch_instance(env, (jobject)object, clazz, method_id, args, 'V');
    return JNI_PENDING_EXCEPTION ? NULL : (jobject)object;
}

jobject jni_entry_new_object_v(JNIEnv* env, jclass clazz, jmethodID method, va_list values)
{
    jvalue args[JNI_MAX_CALL_ARGS] = {{0}};

    if(!__decode_arguments(env, method, JNI_FALSE, 'V', values, args)) {
        return NULL;
    }

    return jni_entry_new_object_a(env, clazz, method, args);
}

#define DEFINE_METHOD_CALLS(scope, name, type, member, code, receiver_type, is_static, dispatch)                       \
    VITAARC_SOFTFP type jni_entry_call_##scope##name##_method_a(                                                       \
        JNIEnv* env, receiver_type receiver, jmethodID method, const jvalue* args)                                     \
    {                                                                                                                  \
        return dispatch(env, receiver, method, args, code).member;                                                     \
    }                                                                                                                  \
    VITAARC_SOFTFP type jni_entry_call_##scope##name##_method_v(                                                       \
        JNIEnv* env, receiver_type receiver, jmethodID method, va_list values)                                         \
    {                                                                                                                  \
        jvalue args[JNI_MAX_CALL_ARGS] = {{0}};                                                                        \
        if(!__decode_arguments(env, method, is_static, code, values, args)) {                                          \
            return (type)0;                                                                                            \
        }                                                                                                              \
        return jni_entry_call_##scope##name##_method_a(env, receiver, method, args);                                   \
    }

#define DEFINE_VALUE_CALLS(name, type, member, code)                                                                   \
    DEFINE_METHOD_CALLS(static_, name, type, member, code, jclass, JNI_TRUE, __dispatch_static)                        \
    DEFINE_METHOD_CALLS(, name, type, member, code, jobject, JNI_FALSE, __dispatch_virtual)

DEFINE_VALUE_CALLS(object, jobject, l, 'L')
DEFINE_VALUE_CALLS(boolean, jboolean, z, 'Z')
DEFINE_VALUE_CALLS(byte, jbyte, b, 'B')
DEFINE_VALUE_CALLS(char, jchar, c, 'C')
DEFINE_VALUE_CALLS(short, jshort, s, 'S')
DEFINE_VALUE_CALLS(int, jint, i, 'I')
DEFINE_VALUE_CALLS(long, jlong, j, 'J')
DEFINE_VALUE_CALLS(float, jfloat, f, 'F')
DEFINE_VALUE_CALLS(double, jdouble, d, 'D')

#define DEFINE_VOID_CALLS(scope, receiver_type, is_static, dispatch)                                                   \
    void jni_entry_call_##scope##void_method_a(                                                                        \
        JNIEnv* env, receiver_type receiver, jmethodID method, const jvalue* args)                                     \
    {                                                                                                                  \
        (void)dispatch(env, receiver, method, args, 'V');                                                              \
    }                                                                                                                  \
    void jni_entry_call_##scope##void_method_v(JNIEnv* env, receiver_type receiver, jmethodID method, va_list values)  \
    {                                                                                                                  \
        jvalue args[JNI_MAX_CALL_ARGS] = {{0}};                                                                        \
        if(__decode_arguments(env, method, is_static, 'V', values, args)) {                                            \
            jni_entry_call_##scope##void_method_a(env, receiver, method, args);                                        \
        }                                                                                                              \
    }

DEFINE_VOID_CALLS(static_, jclass, JNI_TRUE, __dispatch_static)
DEFINE_VOID_CALLS(, jobject, JNI_FALSE, __dispatch_virtual)

#define DEFINE_NONVIRTUAL_CALLS(name, type, member, code)                                                              \
    VITAARC_SOFTFP type jni_entry_call_nonvirtual_##name##_method_a(                                                   \
        JNIEnv* env, jobject object, jclass clazz, jmethodID method, const jvalue* args)                               \
    {                                                                                                                  \
        return __dispatch_nonvirtual(env, object, clazz, method, args, code).member;                                   \
    }                                                                                                                  \
    VITAARC_SOFTFP type jni_entry_call_nonvirtual_##name##_method_v(                                                   \
        JNIEnv* env, jobject object, jclass clazz, jmethodID method, va_list values)                                   \
    {                                                                                                                  \
        jvalue args[JNI_MAX_CALL_ARGS] = {{0}};                                                                        \
        if(!__decode_arguments(env, method, JNI_FALSE, code, values, args)) {                                          \
            return (type)0;                                                                                            \
        }                                                                                                              \
        return jni_entry_call_nonvirtual_##name##_method_a(env, object, clazz, method, args);                          \
    }

DEFINE_NONVIRTUAL_CALLS(object, jobject, l, 'L')
DEFINE_NONVIRTUAL_CALLS(boolean, jboolean, z, 'Z')
DEFINE_NONVIRTUAL_CALLS(byte, jbyte, b, 'B')
DEFINE_NONVIRTUAL_CALLS(char, jchar, c, 'C')
DEFINE_NONVIRTUAL_CALLS(short, jshort, s, 'S')
DEFINE_NONVIRTUAL_CALLS(int, jint, i, 'I')
DEFINE_NONVIRTUAL_CALLS(long, jlong, j, 'J')
DEFINE_NONVIRTUAL_CALLS(float, jfloat, f, 'F')
DEFINE_NONVIRTUAL_CALLS(double, jdouble, d, 'D')

void jni_entry_call_nonvirtual_void_method_a(
    JNIEnv* env, jobject object, jclass clazz, jmethodID method, const jvalue* args)
{
    (void)__dispatch_nonvirtual(env, object, clazz, method, args, 'V');
}

void jni_entry_call_nonvirtual_void_method_v(
    JNIEnv* env, jobject object, jclass clazz, jmethodID method, va_list values)
{
    jvalue args[JNI_MAX_CALL_ARGS] = {{0}};

    if(__decode_arguments(env, method, JNI_FALSE, 'V', values, args)) {
        jni_entry_call_nonvirtual_void_method_a(env, object, clazz, method, args);
    }
}
