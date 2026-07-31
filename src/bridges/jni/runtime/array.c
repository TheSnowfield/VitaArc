#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "entry.h"
#include "class.h"
#include "exception.h"

static jni_obj_t* __array_new(JNIEnv* env, jsize length, size_t stride, char type, jni_obj_t* elem)
{
    if(length < 0) {
        jni_raise(env, JEX_ARRAY_SIZE, "negative array length");
        return NULL;
    }

    if((size_t)length > SIZE_MAX / stride) {
        jni_raise(env, JEX_MEMORY, "array size overflow");
        return NULL;
    }

    char name[1024];
    int name_length;
    if(type != 'L') {
        name_length = snprintf(name, sizeof(name), "[%c", type);
    } else if(elem->name[0] == '[') {
        name_length = snprintf(name, sizeof(name), "[%s", elem->name);
    } else {
        name_length = snprintf(name, sizeof(name), "[L%s;", elem->name);
    }

    if(name_length < 0 || name_length >= (int)sizeof(name)) {
        jni_raise(env, JEX_ARG, "array class name too long");
        return NULL;
    }

    jni_obj_t* clazz = jni_obj_get(jni_entry_find_class(env, name));
    if(!clazz) {
        return NULL;
    }

    jni_obj_t* array = jni_heap_new(JNI_KIND_ARRAY);
    if(!array) {
        return NULL;
    }

    array->clazz = clazz;
    array->length = length;
    array->stride = stride;
    array->type = type;
    array->elem = elem;
    array->data = calloc(length ? (size_t)length : 1, stride);
    if(!array->data) {
        jni_raise(env, JEX_MEMORY, "array allocation failed");
        return NULL;
    }

    return array;
}

static jni_obj_t* __array_get(JNIEnv* env, jarray reference, char type)
{
    if(!reference) {
        jni_raise(env, JEX_NULL, "array is null");
        return NULL;
    }

    jni_obj_t* array = jni_obj_get(reference);
    if(!array || array->kind != JNI_KIND_ARRAY) {
        jni_raise(env, JEX_ARG, "expected JNI array");
        return NULL;
    }

    if(type && array->type != type) {
        jni_raise(env, JEX_ARG, "array element type mismatch");
        return NULL;
    }

    return array;
}

static bool __region_ok(JNIEnv* env, jni_obj_t* array, jsize start, jsize length)
{
    if(start >= 0 && length >= 0 && start <= array->length) {
        if(length <= array->length - start) {
            return true;
        }
    }

    jni_raise(env, JEX_ARRAY_BOUNDS, "array region");
    return false;
}

jsize jni_entry_get_array_length(JNIEnv* env, jarray reference)
{
    jni_obj_t* array = __array_get(env, reference, 0);
    return array ? array->length : 0;
}

jobjectArray jni_entry_new_object_array(JNIEnv* env, jsize length, jclass clazz, jobject initial)
{
    jni_obj_t* elem = jni_obj_get(clazz);
    if(!elem || elem->kind != JNI_KIND_CLASS) {
        jni_raise(env, JEX_ARG, "invalid array element class");
        return NULL;
    }

    if(!jni_instanceof(env, initial, elem)) {
        jni_raise(env, JEX_STORE, "initial value has incompatible type");
        return NULL;
    }

    jni_obj_t* array = __array_new(env, length, sizeof(jobject), 'L', elem);
    if(!array) {
        return NULL;
    }

    for(jsize index = 0; index < length; ++index) {
        ((jobject*)array->data)[index] = (jobject)jni_obj_get(initial);
    }

    return (jobjectArray)array;
}

jobject jni_entry_get_object_array_element(JNIEnv* env, jobjectArray reference, jsize index)
{
    jni_obj_t* array = __array_get(env, reference, 'L');
    if(!array || !__region_ok(env, array, index, 1)) {
        return NULL;
    }

    return ((jobject*)array->data)[index];
}

void jni_entry_set_object_array_element(JNIEnv* env, jobjectArray reference, jsize index, jobject value)
{
    jni_obj_t* array = __array_get(env, reference, 'L');
    if(!array || !__region_ok(env, array, index, 1)) {
        return;
    }

    if(!jni_instanceof(env, value, array->elem)) {
        jni_raise(env, JEX_STORE, "incompatible array element");
        return;
    }

    ((jobject*)array->data)[index] = (jobject)jni_obj_get(value);
}

static void* __pin(JNIEnv* env, jarray reference, char type, jboolean* is_copy)
{
    if(is_copy) {
        *is_copy = JNI_FALSE;
    }

    jni_obj_t* array = __array_get(env, reference, type);
    if(!array) {
        return NULL;
    }

    if(!jni_refs_pin(array)) {
        jni_raise(env, JEX_MEMORY, "cannot retain array elements");
        return NULL;
    }

    return array->data;
}

static void __unpin(JNIEnv* env, jarray reference, char type, void* elements, jint mode)
{
    jni_obj_t* array = __array_get(env, reference, type);
    if(!array) {
        return;
    }

    if(elements != array->data) {
        jni_raise(env, JEX_ARG, "array elements pointer mismatch");
        return;
    }

    if(mode != 0 && mode != JNI_ABORT && mode != JNI_COMMIT) {
        jni_raise(env, JEX_ARG, "invalid array release mode");
        return;
    }

    if(mode != JNI_COMMIT && !jni_refs_unpin(array)) {
        jni_raise(env, JEX_ARG, "array elements were not retained");
    }

    /* Elements are direct storage, so release never copies or frees the buffer.
     * JNI_ABORT cannot undo writes through a direct pointer. */
}

static void __region_copy(JNIEnv* env, jarray reference, char type, jsize start, jsize length, void* buffer, bool write)
{
    jni_obj_t* array = __array_get(env, reference, type);
    if(!array || !__region_ok(env, array, start, length)) {
        return;
    }

    if(!length) {
        return;
    }

    if(!buffer) {
        jni_raise(env, JEX_NULL, "array region buffer is null");
        return;
    }

    unsigned char* position = (unsigned char*)array->data + (size_t)start * array->stride;
    size_t size = (size_t)length * array->stride;
    if(write) {
        memcpy(position, buffer, size);
    } else {
        memcpy(buffer, position, size);
    }
}

#define DEFINE_PRIMITIVE_ARRAY(name, type, array_type, sig)                                                            \
    array_type jni_entry_new_##name##_array(JNIEnv* env, jsize length)                                                 \
    {                                                                                                                  \
        return (array_type)__array_new(env, length, sizeof(type), sig, NULL);                                          \
    }                                                                                                                  \
    type* jni_entry_get_##name##_array_elements(JNIEnv* env, array_type array, jboolean* is_copy)                      \
    {                                                                                                                  \
        return __pin(env, array, sig, is_copy);                                                                        \
    }                                                                                                                  \
    void jni_entry_release_##name##_array_elements(JNIEnv* env, array_type array, type* elements, jint mode)           \
    {                                                                                                                  \
        __unpin(env, array, sig, elements, mode);                                                                      \
    }                                                                                                                  \
    void jni_entry_get_##name##_array_region(JNIEnv* env, array_type array, jsize start, jsize length, type* out)      \
    {                                                                                                                  \
        __region_copy(env, array, sig, start, length, out, false);                                                     \
    }                                                                                                                  \
    void jni_entry_set_##name##_array_region(                                                                          \
        JNIEnv* env, array_type array, jsize start, jsize length, const type* input)                                   \
    {                                                                                                                  \
        __region_copy(env, array, sig, start, length, (void*)input, true);                                             \
    }

DEFINE_PRIMITIVE_ARRAY(boolean, jboolean, jbooleanArray, 'Z')
DEFINE_PRIMITIVE_ARRAY(byte, jbyte, jbyteArray, 'B')
DEFINE_PRIMITIVE_ARRAY(char, jchar, jcharArray, 'C')
DEFINE_PRIMITIVE_ARRAY(short, jshort, jshortArray, 'S')
DEFINE_PRIMITIVE_ARRAY(int, jint, jintArray, 'I')
DEFINE_PRIMITIVE_ARRAY(long, jlong, jlongArray, 'J')
DEFINE_PRIMITIVE_ARRAY(float, jfloat, jfloatArray, 'F')
DEFINE_PRIMITIVE_ARRAY(double, jdouble, jdoubleArray, 'D')

void* jni_entry_get_primitive_array_critical(JNIEnv* env, jarray reference, jboolean* is_copy)
{
    jni_obj_t* array = __array_get(env, reference, 0);
    if(!array) {
        return NULL;
    }

    if(array->type == 'L') {
        jni_raise(env, JEX_ARG, "expected primitive array");
        return NULL;
    }

    return __pin(env, reference, array->type, is_copy);
}

void jni_entry_release_primitive_array_critical(JNIEnv* env, jarray reference, void* data, jint mode)
{
    jni_obj_t* array = __array_get(env, reference, 0);
    if(!array) {
        return;
    }

    if(array->type == 'L') {
        jni_raise(env, JEX_ARG, "expected primitive array");
        return;
    }

    __unpin(env, reference, array->type, data, mode);
}

jobject jni_entry_new_direct_byte_buffer(JNIEnv* env, void* address, jlong capacity)
{
    (void)env;
    if(capacity < 0 || capacity > INT32_MAX || (!address && capacity)) {
        return NULL;
    }

    jni_obj_t* buffer = jni_heap_new(JNI_KIND_DIRECT_BUFFER);
    if(!buffer) {
        return NULL;
    }

    buffer->data = address;
    buffer->length = (jsize)capacity;
    return (jobject)buffer;
}

void* jni_entry_get_direct_buffer_address(JNIEnv* env, jobject buffer)
{
    (void)env;
    jni_obj_t* object = jni_obj_get(buffer);
    return object && object->kind == JNI_KIND_DIRECT_BUFFER ? object->data : NULL;
}

jlong jni_entry_get_direct_buffer_capacity(JNIEnv* env, jobject buffer)
{
    (void)env;
    jni_obj_t* object = jni_obj_get(buffer);
    return object && object->kind == JNI_KIND_DIRECT_BUFFER ? object->length : -1;
}
