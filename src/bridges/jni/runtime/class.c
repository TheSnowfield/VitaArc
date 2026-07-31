#include <string.h>
#include <logcat/logcat.h>

#include "object.h"
#include "entry.h"
#include "class.h"
#include "exception.h"

typedef struct {
    const char* name;
    const char* base;
} jni_parent_t;

static const jni_parent_t __parents[] = {
    {"org/cocos2dx/cpp/AppActivity", "org/cocos2dx/lib/Cocos2dxActivity"},
    {"org/cocos2dx/lib/Cocos2dxActivity", "android/app/Activity"},
    {"android/app/Activity", "android/view/ContextThemeWrapper"},
    {"android/view/ContextThemeWrapper", "android/content/ContextWrapper"},
    {"android/content/ContextWrapper", "android/content/Context"},

    {JEX_BASE, JEX_THROWABLE},
    {JEX_RUNTIME, JEX_BASE},
    {JEX_IO, JEX_BASE},
    {JEX_ARG, JEX_RUNTIME},
    {JEX_MONITOR, JEX_RUNTIME},
    {JEX_NULL, JEX_RUNTIME},
    {JEX_STORE, JEX_RUNTIME},
    {JEX_ARRAY_SIZE, JEX_RUNTIME},
    {JEX_UNSUPPORTED, JEX_RUNTIME},
    {JEX_BOUNDS, JEX_RUNTIME},
    {JEX_ARRAY_BOUNDS, JEX_BOUNDS},
    {JEX_STRING_BOUNDS, JEX_BOUNDS},
};

jclass jni_entry_find_class(JNIEnv* env, const char* name)
{
    (void)env;
    if(!name) {
        return NULL;
    }

    char normalized[1024];
    if(strlen(name) >= sizeof(normalized)) {
        return NULL;
    }

    strcpy(normalized, name);
    for(char* cursor = normalized; *cursor; ++cursor) {
        if(*cursor == '.') {
            *cursor = '/';
        }
    }

    name = normalized;

    for(jni_obj_t* object = jni_heap_head(); object; object = object->next) {
        if(object->kind == JNI_KIND_CLASS && object->name && strcmp(object->name, name) == 0) {
            return (jclass)object;
        }
    }

    jni_obj_t* clazz = jni_heap_new(JNI_KIND_CLASS);
    if(!clazz) {
        return NULL;
    }

    clazz->name = jni_text_dup(name);
    if(!clazz->name) {
        return NULL;
    }

    log_v(TAG, "JNI FindClass(\"%s\")", name);
    return (jclass)clazz;
}

bool jni_named(const jni_obj_t* object, const char* name, const char* signature)
{
    if(!object->name)
        return false;

    if(!object->signature)
        return false;

    if(strcmp(object->name, name) != 0)
        return false;

    return strcmp(object->signature, signature) == 0;
}

jni_obj_t* jni_member(jclass clazz, const char* name, const char* signature, jni_kind_t kind, jboolean is_static)
{
    jni_obj_t* class_object = jni_obj_get(clazz);
    if(!class_object || class_object->kind != JNI_KIND_CLASS) {
        return NULL;
    }

    for(jni_obj_t* object = jni_heap_head(); object; object = object->next) {
        if(object->kind != kind)
            continue;

        if(object->decl)
            continue;

        if(object->clazz != class_object)
            continue;

        if(object->is_static != is_static)
            continue;

        if(!jni_named(object, name, signature))
            continue;

        return object;
    }

    jni_obj_t* member = jni_heap_new(kind);
    if(!member) {
        return NULL;
    }

    member->clazz = class_object;
    member->name = jni_text_dup(name);
    member->signature = jni_text_dup(signature);
    member->is_static = is_static;
    if(!member->name || !member->signature) {
        return NULL;
    }

    return member;
}

jni_obj_t* jni_super(JNIEnv* env, jni_obj_t* clazz)
{
    if(!clazz || clazz->kind != JNI_KIND_CLASS || !clazz->name) {
        return NULL;
    }

    if(strcmp(clazz->name, JCLS_OBJECT) == 0) {
        return NULL;
    }

    if(strcmp(clazz->name, JCLS_CLONEABLE) == 0 || strcmp(clazz->name, JCLS_SERIALIZABLE) == 0) {
        return NULL;
    }

    for(size_t index = 0; index < sizeof(__parents) / sizeof(__parents[0]); ++index) {
        if(strcmp(clazz->name, __parents[index].name) == 0) {
            return jni_obj_get(jni_entry_find_class(env, __parents[index].base));
        }
    }

    return jni_obj_get(jni_entry_find_class(env, JCLS_OBJECT));
}

static jni_obj_t* __array_component_class(JNIEnv* env, jni_obj_t* clazz)
{
    if(clazz->name[0] != '[') {
        return NULL;
    }

    const char* component = clazz->name + 1;
    if(*component == '[') {
        return jni_obj_get(jni_entry_find_class(env, component));
    }

    if(*component != 'L') {
        return NULL;
    }

    char name[1024];
    size_t length = strlen(component);
    if(length < 3 || component[length - 1] != ';' || length >= sizeof(name)) {
        return NULL;
    }

    memcpy(name, component + 1, length - 2);
    name[length - 2] = 0;
    return jni_obj_get(jni_entry_find_class(env, name));
}

bool jni_assignable(JNIEnv* env, jni_obj_t* source, jni_obj_t* target)
{
    if(!source || !target) {
        return false;
    }

    if(source->kind != JNI_KIND_CLASS || target->kind != JNI_KIND_CLASS) {
        return false;
    }

    if(source == target) {
        return true;
    }

    if(!source->name || !target->name) {
        return false;
    }

    if(source->name[0] == '[') {
        if(strcmp(target->name, JCLS_CLONEABLE) == 0 || strcmp(target->name, JCLS_SERIALIZABLE) == 0) {
            return true;
        }

        if(target->name[0] == '[') {
            return jni_assignable(env, __array_component_class(env, source), __array_component_class(env, target));
        }
    }

    for(jni_obj_t* parent = jni_super(env, source); parent; parent = jni_super(env, parent)) {
        if(parent == target) {
            return true;
        }
    }

    return false;
}

jni_obj_t* jni_classof(JNIEnv* env, jni_obj_t* object)
{
    if(!object) {
        return NULL;
    }

    if(object->clazz) {
        return object->clazz;
    }

    const char* name = JCLS_OBJECT;
    switch(object->kind) {
        case JNI_KIND_CLASS:
            name = JCLS_CLASS;
            break;

        case JNI_KIND_STRING:
            name = JCLS_STRING;
            break;

        case JNI_KIND_DIRECT_BUFFER:
            name = JCLS_BUFFER;
            break;

        default:
            break;
    }

    return jni_obj_get(jni_entry_find_class(env, name));
}

bool jni_instanceof(JNIEnv* env, jobject reference, jni_obj_t* clazz)
{
    if(!reference) {
        return true;
    }

    jni_obj_t* object = jni_obj_get(reference);
    return jni_assignable(env, jni_classof(env, object), clazz);
}

jni_obj_t* jni_obj_new(JNIEnv* env, jclass reference)
{
    jni_obj_t* clazz = jni_obj_get(reference);
    if(!clazz || clazz->kind != JNI_KIND_CLASS) {
        jni_raise(env, JEX_ARG, "invalid object class");
        return NULL;
    }

    if(clazz->name[0] == '[') {
        jni_raise(env, JEX_ARG, "use an array constructor");
        return NULL;
    }

    jni_obj_t* object;
    if(strcmp(clazz->name, JCLS_STRING) == 0) {
        object = jni_obj_get(jni_entry_new_string_utf(env, ""));
    } else {
        object = jni_heap_new(JNI_KIND_OBJECT);
    }

    if(object) {
        object->clazz = clazz;
    }

    return object;
}

jclass jni_entry_define_class(JNIEnv* env, const char* name, jobject loader, const jbyte* data, jsize length)
{
    (void)loader;
    (void)data;
    (void)length;
    return jni_entry_find_class(env, name);
}

jmethodID jni_entry_from_reflected_method(JNIEnv* env, jobject method)
{
    (void)env;
    return (jmethodID)jni_obj_get(method);
}

jfieldID jni_entry_from_reflected_field(JNIEnv* env, jobject field)
{
    (void)env;
    return (jfieldID)jni_obj_get(field);
}

jobject jni_entry_to_reflected_method(JNIEnv* env, jclass clazz, jmethodID method, jboolean is_static)
{
    (void)env;
    (void)clazz;
    (void)is_static;
    return (jobject)method;
}

jobject jni_entry_to_reflected_field(JNIEnv* env, jclass clazz, jfieldID field, jboolean is_static)
{
    (void)env;
    (void)clazz;
    (void)is_static;
    return (jobject)field;
}

jclass jni_entry_get_superclass(JNIEnv* env, jclass clazz)
{
    return (jclass)jni_super(env, jni_obj_get(clazz));
}

jboolean jni_entry_is_assignable_from(JNIEnv* env, jclass first, jclass second)
{
    return jni_assignable(env, jni_obj_get(first), jni_obj_get(second));
}

jobject jni_entry_alloc_object(JNIEnv* env, jclass clazz)
{
    return (jobject)jni_obj_new(env, clazz);
}

jclass jni_entry_get_object_class(JNIEnv* env, jobject object)
{
    return (jclass)jni_classof(env, jni_obj_get(object));
}

jboolean jni_entry_is_instance_of(JNIEnv* env, jobject object, jclass clazz)
{
    return jni_instanceof(env, object, jni_obj_get(clazz));
}
