#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "entry.h"
#include "class.h"
#include "exception.h"

static jfieldID __lookup_field(JNIEnv* env, jclass clazz, const char* name, const char* signature, jboolean is_static)
{
    char type;
    if(!name || !jni_type_parse(signature, &type)) {
        jni_raise(env, JEX_ARG, "invalid field descriptor");
        return NULL;
    }

    for(jni_obj_t* owner = jni_obj_get(clazz); owner; owner = jni_super(env, owner)) {
        for(jni_obj_t* member = jni_heap_head(); member; member = member->next) {
            if(member->kind != JNI_KIND_FIELD)
                continue;

            if(member->clazz != owner)
                continue;

            if(member->is_static != is_static)
                continue;

            if(!jni_named(member, name, signature))
                continue;

            return (jfieldID)member;
        }
    }

    return (jfieldID)jni_member(clazz, name, signature, JNI_KIND_FIELD, is_static);
}

jfieldID jni_entry_get_field_id(JNIEnv* env, jclass clazz, const char* name, const char* signature)
{
    (void)env;
    if(!clazz || !name || !signature) {
        return NULL;
    }

    return __lookup_field(env, clazz, name, signature, JNI_FALSE);
}

jfieldID jni_entry_get_static_field_id(JNIEnv* env, jclass clazz, const char* name, const char* signature)
{
    (void)env;
    if(!clazz || !name || !signature) {
        return NULL;
    }

    return __lookup_field(env, clazz, name, signature, JNI_TRUE);
}

static jvalue* __field_value(
    JNIEnv* env, jobject receiver, jfieldID field_id, jboolean is_static, char type, bool create)
{
    jni_obj_t* object = jni_obj_get(receiver);
    jni_obj_t* field = jni_obj_get(field_id);
    if(!object) {
        jni_raise(env, JEX_NULL, "field receiver is null or invalid");
        return NULL;
    }

    if(!field || field->kind != JNI_KIND_FIELD) {
        jni_raise(env, JEX_ARG, "invalid field ID");
        return NULL;
    }

    if(field->is_static != is_static) {
        jni_raise(env, JEX_ARG, "field static/instance mismatch");
        return NULL;
    }

    char actual_type;
    if(!jni_type_parse(field->signature, &actual_type) || actual_type != type) {
        jni_raise(env, JEX_ARG, "field accessor type mismatch");
        return NULL;
    }

    jni_obj_t* owner = jni_classof(env, object);
    if(is_static) {
        owner = object;
    }

    if(!jni_assignable(env, owner, field->clazz)) {
        jni_raise(env, JEX_ARG, "field receiver class mismatch");
        return NULL;
    }

    if(is_static) {
        object = field->clazz;
    }

    for(jni_slot_t* entry = object->fields; entry; entry = entry->next) {
        if(entry->field == field_id) {
            return &entry->value;
        }
    }

    if(!create) {
        return NULL;
    }

    jni_slot_t* entry = calloc(1, sizeof(*entry));
    if(!entry) {
        return NULL;
    }

    entry->field = field_id;
    entry->next = object->fields;
    object->fields = entry;
    return &entry->value;
}

#define DEFINE_FIELD_ACCESSORS(name, type, member, code)                                                               \
    VITAARC_SOFTFP type jni_entry_get_##name##_field(JNIEnv* env, jobject object, jfieldID field)                      \
    {                                                                                                                  \
        (void)env;                                                                                                     \
        jvalue* value = __field_value(env, object, field, JNI_FALSE, code, false);                                     \
        return value ? value->member : (type)0;                                                                        \
    }                                                                                                                  \
    VITAARC_SOFTFP void jni_entry_set_##name##_field(JNIEnv* env, jobject object, jfieldID field, type input)          \
    {                                                                                                                  \
        (void)env;                                                                                                     \
        jvalue* value = __field_value(env, object, field, JNI_FALSE, code, true);                                      \
        if(value)                                                                                                      \
            value->member = input;                                                                                     \
    }                                                                                                                  \
    VITAARC_SOFTFP type jni_entry_get_static_##name##_field(JNIEnv* env, jclass clazz, jfieldID field)                 \
    {                                                                                                                  \
        (void)env;                                                                                                     \
        jvalue* value = __field_value(env, clazz, field, JNI_TRUE, code, false);                                       \
        return value ? value->member : (type)0;                                                                        \
    }                                                                                                                  \
    VITAARC_SOFTFP void jni_entry_set_static_##name##_field(JNIEnv* env, jclass clazz, jfieldID field, type input)     \
    {                                                                                                                  \
        (void)env;                                                                                                     \
        jvalue* value = __field_value(env, clazz, field, JNI_TRUE, code, true);                                        \
        if(value)                                                                                                      \
            value->member = input;                                                                                     \
    }

DEFINE_FIELD_ACCESSORS(boolean, jboolean, z, 'Z')
DEFINE_FIELD_ACCESSORS(byte, jbyte, b, 'B')
DEFINE_FIELD_ACCESSORS(char, jchar, c, 'C')
DEFINE_FIELD_ACCESSORS(short, jshort, s, 'S')
DEFINE_FIELD_ACCESSORS(int, jint, i, 'I')
DEFINE_FIELD_ACCESSORS(long, jlong, j, 'J')
DEFINE_FIELD_ACCESSORS(float, jfloat, f, 'F')
DEFINE_FIELD_ACCESSORS(double, jdouble, d, 'D')

jobject jni_entry_get_object_field(JNIEnv* env, jobject object, jfieldID field)
{
    jvalue* value = __field_value(env, object, field, JNI_FALSE, 'L', false);
    return value ? value->l : NULL;
}

jobject jni_entry_get_static_object_field(JNIEnv* env, jclass clazz, jfieldID field)
{
    jvalue* value = __field_value(env, clazz, field, JNI_TRUE, 'L', false);
    return value ? value->l : NULL;
}

void jni_entry_set_object_field(JNIEnv* env, jobject object, jfieldID field, jobject input)
{
    jvalue* value = __field_value(env, object, field, JNI_FALSE, 'L', true);
    if(value) {
        value->l = (jobject)jni_obj_get(input);
    }
}

void jni_entry_set_static_object_field(JNIEnv* env, jclass clazz, jfieldID field, jobject input)
{
    jvalue* value = __field_value(env, clazz, field, JNI_TRUE, 'L', true);
    if(value) {
        value->l = (jobject)jni_obj_get(input);
    }
}
