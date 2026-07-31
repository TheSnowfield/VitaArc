#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "entry.h"
#include "exception.h"

static uint32_t __decode(const unsigned char** cursor)
{
    const unsigned char* text = *cursor;
    uint32_t codepoint;

    if(text[0] < 0x80) {
        *cursor = text + 1;
        return text[0];
    }

    if((text[0] & 0xE0) == 0xC0 && (text[1] & 0xC0) == 0x80) {
        codepoint = ((text[0] & 0x1F) << 6) | (text[1] & 0x3F);
        *cursor = text + 2;
        return codepoint;
    }

    if((text[0] & 0xF0) == 0xE0 && (text[1] & 0xC0) == 0x80 && (text[2] & 0xC0) == 0x80) {
        codepoint = ((text[0] & 0x0F) << 12) | ((text[1] & 0x3F) << 6) | (text[2] & 0x3F);
        *cursor = text + 3;
        return codepoint;
    }

    if((text[0] & 0xF8) == 0xF0 && (text[1] & 0xC0) == 0x80 && (text[2] & 0xC0) == 0x80 && (text[3] & 0xC0) == 0x80) {
        codepoint = ((text[0] & 0x07) << 18) | ((text[1] & 0x3F) << 12) | ((text[2] & 0x3F) << 6) | (text[3] & 0x3F);
        *cursor = text + 4;
        return codepoint;
    }

    *cursor = text + 1;
    return 0xFFFD;
}

static jchar* __utf16(const char* text, jsize* out_length)
{
    const unsigned char* cursor = (const unsigned char*)(text ? text : "");
    const unsigned char* scan = cursor;
    jsize length = 0;

    while(*scan) {
        uint32_t codepoint = __decode(&scan);
        length += codepoint > 0xFFFF ? 2 : 1;
    }

    jchar* utf16 = calloc((size_t)length + 1, sizeof(*utf16));
    if(!utf16) {
        return NULL;
    }

    jsize index = 0;
    while(*cursor) {
        uint32_t codepoint = __decode(&cursor);
        if(codepoint > 0xFFFF) {
            codepoint -= 0x10000;
            utf16[index++] = (jchar)(0xD800 | (codepoint >> 10));
            utf16[index++] = (jchar)(0xDC00 | (codepoint & 0x3FF));
        } else {
            utf16[index++] = (jchar)codepoint;
        }
    }

    if(out_length) {
        *out_length = length;
    }

    return utf16;
}

static char* __utf8(const jchar* text, jsize length)
{
    if((size_t)length > (SIZE_MAX - 1) / 3) {
        return NULL;
    }

    size_t capacity = (size_t)length * 3 + 1;
    char* utf8 = calloc(capacity, 1);
    if(!utf8) {
        return NULL;
    }

    size_t output = 0;
    for(jsize index = 0; index < length; ++index) {
        uint32_t codepoint = text[index];
        /* JNI modified UTF-8 encodes NUL as C0 80 and encodes each surrogate
         * separately. It never emits a four-byte UTF-8 sequence. */
        if(codepoint > 0 && codepoint < 0x80) {
            utf8[output++] = (char)codepoint;
        } else if(codepoint < 0x800) {
            utf8[output++] = (char)(0xC0 | (codepoint >> 6));
            utf8[output++] = (char)(0x80 | (codepoint & 0x3F));
        } else {
            utf8[output++] = (char)(0xE0 | (codepoint >> 12));
            utf8[output++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
            utf8[output++] = (char)(0x80 | (codepoint & 0x3F));
        }
    }

    return utf8;
}

jstring jni_entry_new_string_utf(JNIEnv* env, const char* text)
{
    (void)env;
    jni_obj_t* string = jni_heap_new(JNI_KIND_STRING);
    if(!string) {
        return NULL;
    }

    string->utf16 = __utf16(text, &string->length);
    string->utf8 = string->utf16 ? __utf8(string->utf16, string->length) : NULL;
    if(!string->utf8 || !string->utf16) {
        return NULL;
    }

    return (jstring)string;
}

jstring jni_entry_new_string(JNIEnv* env, const jchar* text, jsize length)
{
    (void)env;
    if((!text && length != 0) || length < 0) {
        return NULL;
    }

    jni_obj_t* string = jni_heap_new(JNI_KIND_STRING);
    if(!string) {
        return NULL;
    }

    string->utf16 = calloc((size_t)length + 1, sizeof(*string->utf16));
    if(!string->utf16) {
        return NULL;
    }

    if(length) {
        memcpy(string->utf16, text, (size_t)length * sizeof(*text));
    }

    string->length = length;
    string->utf8 = __utf8(text, length);
    return (jstring)string;
}

jstring jni_runtime_new_string_utf(const char* text)
{
    JNIEnv* env = jni_runtime_get_env();
    return env ? (*env)->NewStringUTF(env, text) : NULL;
}

const char* jni_runtime_get_string_utf(jstring string)
{
    jni_thread_lock();
    jni_obj_t* object = jni_obj_get(string);
    const char* text = object && object->kind == JNI_KIND_STRING ? object->utf8 : NULL;
    jni_thread_unlock();
    return text;
}

jsize jni_entry_get_string_length(JNIEnv* env, jstring string)
{
    (void)env;
    jni_obj_t* object = jni_obj_get(string);
    return object && object->kind == JNI_KIND_STRING ? object->length : 0;
}

jsize jni_entry_get_string_utf_length(JNIEnv* env, jstring string)
{
    (void)env;
    const char* text = jni_runtime_get_string_utf(string);
    return text ? (jsize)strlen(text) : 0;
}

const jchar* jni_entry_get_string_chars(JNIEnv* env, jstring string, jboolean* is_copy)
{
    (void)env;
    jni_obj_t* object = jni_obj_get(string);
    if(is_copy) {
        *is_copy = JNI_FALSE;
    }

    if(!object || object->kind != JNI_KIND_STRING) {
        return NULL;
    }

    if(!jni_refs_pin(object)) {
        jni_raise(env, JEX_MEMORY, "cannot retain string characters");
        return NULL;
    }

    return object->utf16;
}

const char* jni_entry_get_string_utf_chars(JNIEnv* env, jstring string, jboolean* is_copy)
{
    (void)env;
    if(is_copy) {
        *is_copy = JNI_FALSE;
    }

    jni_obj_t* object = jni_obj_get(string);
    if(!object || object->kind != JNI_KIND_STRING) {
        return NULL;
    }

    if(!jni_refs_pin(object)) {
        jni_raise(env, JEX_MEMORY, "cannot retain string bytes");
        return NULL;
    }

    return object->utf8;
}

void jni_entry_release_string_chars(JNIEnv* env, jstring string, const jchar* chars)
{
    jni_obj_t* object = jni_obj_get(string);
    if(!object || chars != object->utf16 || !jni_refs_unpin(object)) {
        jni_raise(env, JEX_ARG, "string character release mismatch");
    }
}

void jni_entry_release_string_utf_chars(JNIEnv* env, jstring string, const char* chars)
{
    jni_obj_t* object = jni_obj_get(string);
    if(!object || chars != object->utf8 || !jni_refs_unpin(object)) {
        jni_raise(env, JEX_ARG, "string byte release mismatch");
    }
}

static const jchar* __region(jni_obj_t* object, jsize start, jsize length, const void* out)
{
    if(!object || object->kind != JNI_KIND_STRING) {
        return NULL;
    }

    if(!out && length) {
        return NULL;
    }

    if(start < 0 || length < 0) {
        return NULL;
    }

    if(start > object->length) {
        return NULL;
    }

    if(length > object->length - start) {
        return NULL;
    }

    return object->utf16;
}

void jni_entry_get_string_region(JNIEnv* env, jstring string, jsize start, jsize length, jchar* out)
{
    const jchar* chars = __region(jni_obj_get(string), start, length, out);
    if(!chars) {
        jni_entry_throw_new(env, jni_entry_find_class(env, JEX_STRING_BOUNDS), "string region");
        return;
    }

    if(length) {
        memcpy(out, chars + start, (size_t)length * sizeof(*out));
    }
}

void jni_entry_get_string_utf_region(JNIEnv* env, jstring string, jsize start, jsize length, char* out)
{
    const jchar* chars = __region(jni_obj_get(string), start, length, out);
    if(!chars) {
        jni_entry_throw_new(env, jni_entry_find_class(env, JEX_STRING_BOUNDS), "UTF string region");
        return;
    }

    if(!length) {
        return;
    }

    char* encoded = __utf8(chars + start, length);
    if(!encoded) {
        return;
    }

    memcpy(out, encoded, strlen(encoded));
    free(encoded);
}

const jchar* jni_entry_get_string_critical(JNIEnv* env, jstring string, jboolean* is_copy)
{
    return jni_entry_get_string_chars(env, string, is_copy);
}

void jni_entry_release_string_critical(JNIEnv* env, jstring string, const jchar* chars)
{
    jni_entry_release_string_chars(env, string, chars);
}
