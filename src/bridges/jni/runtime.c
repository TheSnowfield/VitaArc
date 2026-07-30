#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <logcat/logcat.h>

#include "cocos.h"
#include "runtime.h"

#define FAKE_JNI_MAGIC 0x4A4E4931u

typedef enum FakeJniKind
{
  FAKE_JNI_OBJECT,
  FAKE_JNI_CLASS,
  FAKE_JNI_STRING,
  FAKE_JNI_METHOD,
  FAKE_JNI_FIELD,
  FAKE_JNI_ARRAY,
  FAKE_JNI_DIRECT_BUFFER
} FakeJniKind;

typedef struct FakeJniObject
{
  uint32_t magic;
  FakeJniKind kind;
  struct FakeJniObject *next;
  struct FakeJniObject *clazz;
  char *name;
  char *signature;
  char *utf8;
  jchar *utf16;
  jsize length;
  size_t element_size;
  void *data;
  void *native_proc;
  uint32_t global_refs;
  jboolean is_static;
} FakeJniObject;

static FakeJniObject *__fake_objects;
static jthrowable __pending_exception;

static char *__runtime_strdup(const char *text)
{
  if (!text)
    return NULL;

  size_t length = strlen(text) + 1;
  char *copy = malloc(length);
  if (copy)
    memcpy(copy, text, length);
  return copy;
}

static FakeJniObject *__runtime_alloc(FakeJniKind kind)
{
  FakeJniObject *object = calloc(1, sizeof(*object));
  if (!object)
    return NULL;

  object->magic = FAKE_JNI_MAGIC;
  object->kind = kind;
  object->next = __fake_objects;
  __fake_objects = object;
  return object;
}

static FakeJniObject *__runtime_cast(const void *reference)
{
  FakeJniObject *object = (FakeJniObject *)reference;
  if (!object || object->magic != FAKE_JNI_MAGIC)
    return NULL;
  return object;
}

static uint32_t __runtime_decode_utf8(const unsigned char **cursor)
{
  const unsigned char *text = *cursor;
  uint32_t codepoint;

  if (text[0] < 0x80)
  {
    *cursor = text + 1;
    return text[0];
  }
  if ((text[0] & 0xE0) == 0xC0 && (text[1] & 0xC0) == 0x80)
  {
    codepoint = ((text[0] & 0x1F) << 6) | (text[1] & 0x3F);
    *cursor = text + 2;
    return codepoint;
  }
  if ((text[0] & 0xF0) == 0xE0 &&
      (text[1] & 0xC0) == 0x80 && (text[2] & 0xC0) == 0x80)
  {
    codepoint = ((text[0] & 0x0F) << 12) |
                ((text[1] & 0x3F) << 6) | (text[2] & 0x3F);
    *cursor = text + 3;
    return codepoint;
  }
  if ((text[0] & 0xF8) == 0xF0 &&
      (text[1] & 0xC0) == 0x80 && (text[2] & 0xC0) == 0x80 &&
      (text[3] & 0xC0) == 0x80)
  {
    codepoint = ((text[0] & 0x07) << 18) |
                ((text[1] & 0x3F) << 12) |
                ((text[2] & 0x3F) << 6) | (text[3] & 0x3F);
    *cursor = text + 4;
    return codepoint;
  }

  *cursor = text + 1;
  return 0xFFFD;
}

static jchar *__runtime_utf8_to_utf16(const char *text, jsize *out_length)
{
  const unsigned char *cursor = (const unsigned char *)(text ? text : "");
  const unsigned char *scan = cursor;
  jsize length = 0;

  while (*scan)
  {
    uint32_t codepoint = __runtime_decode_utf8(&scan);
    length += codepoint > 0xFFFF ? 2 : 1;
  }

  jchar *utf16 = calloc((size_t)length + 1, sizeof(*utf16));
  if (!utf16)
    return NULL;

  jsize index = 0;
  while (*cursor)
  {
    uint32_t codepoint = __runtime_decode_utf8(&cursor);
    if (codepoint > 0xFFFF)
    {
      codepoint -= 0x10000;
      utf16[index++] = (jchar)(0xD800 | (codepoint >> 10));
      utf16[index++] = (jchar)(0xDC00 | (codepoint & 0x3FF));
    }
    else
    {
      utf16[index++] = (jchar)codepoint;
    }
  }

  if (out_length)
    *out_length = length;
  return utf16;
}

static char *__runtime_utf16_to_utf8(const jchar *text, jsize length)
{
  size_t capacity = (size_t)length * 4 + 1;
  char *utf8 = calloc(capacity, 1);
  if (!utf8)
    return NULL;

  size_t output = 0;
  for (jsize index = 0; index < length; ++index)
  {
    uint32_t codepoint = text[index];
    if (codepoint >= 0xD800 && codepoint <= 0xDBFF &&
        index + 1 < length &&
        text[index + 1] >= 0xDC00 && text[index + 1] <= 0xDFFF)
    {
      codepoint = 0x10000 +
                  ((codepoint - 0xD800) << 10) +
                  (text[++index] - 0xDC00);
    }

    if (codepoint < 0x80)
      utf8[output++] = (char)codepoint;
    else if (codepoint < 0x800)
    {
      utf8[output++] = (char)(0xC0 | (codepoint >> 6));
      utf8[output++] = (char)(0x80 | (codepoint & 0x3F));
    }
    else if (codepoint < 0x10000)
    {
      utf8[output++] = (char)(0xE0 | (codepoint >> 12));
      utf8[output++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
      utf8[output++] = (char)(0x80 | (codepoint & 0x3F));
    }
    else
    {
      utf8[output++] = (char)(0xF0 | (codepoint >> 18));
      utf8[output++] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
      utf8[output++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
      utf8[output++] = (char)(0x80 | (codepoint & 0x3F));
    }
  }

  return utf8;
}

static jstring __new_string_utf(JNIEnv *env, const char *text)
{
  (void)env;
  FakeJniObject *string = __runtime_alloc(FAKE_JNI_STRING);
  if (!string)
    return NULL;

  string->utf8 = __runtime_strdup(text ? text : "");
  string->utf16 = __runtime_utf8_to_utf16(text, &string->length);
  if (!string->utf8 || !string->utf16)
    return NULL;
  return (jstring)string;
}

static jstring __new_string(JNIEnv *env, const jchar *text, jsize length)
{
  (void)env;
  if (!text || length < 0)
    return NULL;

  FakeJniObject *string = __runtime_alloc(FAKE_JNI_STRING);
  if (!string)
    return NULL;

  string->utf16 = calloc((size_t)length + 1, sizeof(*string->utf16));
  if (!string->utf16)
    return NULL;

  memcpy(string->utf16, text, (size_t)length * sizeof(*text));
  string->length = length;
  string->utf8 = __runtime_utf16_to_utf8(text, length);
  return (jstring)string;
}

jstring jni_runtime_new_string_utf(const char *text)
{
  return __new_string_utf(jni_runtime_get_env(), text);
}

const char *jni_runtime_get_string_utf(jstring string)
{
  FakeJniObject *object = __runtime_cast(string);
  return object && object->kind == FAKE_JNI_STRING ? object->utf8 : NULL;
}

static jclass __find_class(JNIEnv *env, const char *name)
{
  (void)env;
  if (!name)
    return NULL;

  for (FakeJniObject *object = __fake_objects; object; object = object->next)
    if (object->kind == FAKE_JNI_CLASS && object->name &&
        strcmp(object->name, name) == 0)
      return (jclass)object;

  FakeJniObject *clazz = __runtime_alloc(FAKE_JNI_CLASS);
  if (!clazz)
    return NULL;
  clazz->name = __runtime_strdup(name);
  log_v(TAG, "JNI FindClass(\"%s\")", name);
  return (jclass)clazz;
}

static FakeJniObject *__runtime_find_member(jclass clazz, const char *name,
                                     const char *signature,
                                     FakeJniKind kind, jboolean is_static)
{
  for (FakeJniObject *object = __fake_objects; object; object = object->next)
    if (object->kind == kind && object->clazz == __runtime_cast(clazz) &&
        object->is_static == is_static && object->name && object->signature &&
        strcmp(object->name, name) == 0 &&
        strcmp(object->signature, signature) == 0)
      return object;

  FakeJniObject *member = __runtime_alloc(kind);
  if (!member)
    return NULL;
  member->clazz = __runtime_cast(clazz);
  member->name = __runtime_strdup(name);
  member->signature = __runtime_strdup(signature);
  member->is_static = is_static;
  return member;
}

static jmethodID __get_method_id(JNIEnv *env, jclass clazz,
                                 const char *name, const char *signature)
{
  (void)env;
  if (!clazz || !name || !signature)
    return NULL;
  log_v(TAG, "JNI GetMethodID(\"%s\", \"%s\")", name, signature);
  return (jmethodID)__runtime_find_member(clazz, name, signature,
                                  FAKE_JNI_METHOD, JNI_FALSE);
}

static jmethodID __get_static_method_id(JNIEnv *env, jclass clazz,
                                       const char *name,
                                       const char *signature)
{
  (void)env;
  if (!clazz || !name || !signature)
    return NULL;
  log_v(TAG, "JNI GetStaticMethodID(\"%s\", \"%s\")", name, signature);
  return (jmethodID)__runtime_find_member(clazz, name, signature,
                                  FAKE_JNI_METHOD, JNI_TRUE);
}

static jfieldID __get_field_id(JNIEnv *env, jclass clazz,
                               const char *name, const char *signature)
{
  (void)env;
  if (!clazz || !name || !signature)
    return NULL;
  return (jfieldID)__runtime_find_member(clazz, name, signature,
                                 FAKE_JNI_FIELD, JNI_FALSE);
}

static jfieldID __get_static_field_id(JNIEnv *env, jclass clazz,
                                     const char *name,
                                     const char *signature)
{
  (void)env;
  if (!clazz || !name || !signature)
    return NULL;
  return (jfieldID)__runtime_find_member(clazz, name, signature,
                                 FAKE_JNI_FIELD, JNI_TRUE);
}

static jobject __new_object_v(JNIEnv *env, jclass clazz,
                              jmethodID method, va_list args)
{
  (void)env;
  (void)method;
  (void)args;
  FakeJniObject *object = __runtime_alloc(FAKE_JNI_OBJECT);
  if (object)
    object->clazz = __runtime_cast(clazz);
  return (jobject)object;
}

static jobject __new_object(JNIEnv *env, jclass clazz,
                             jmethodID method, ...)
{
  va_list args;
  va_start(args, method);
  jobject result = __new_object_v(env, clazz, method, args);
  va_end(args);
  return result;
}

static jobject __new_object_a(JNIEnv *env, jclass clazz,
                              jmethodID method, const jvalue *args)
{
  (void)args;
  va_list empty;
  memset(&empty, 0, sizeof(empty));
  return __new_object_v(env, clazz, method, empty);
}

static jobject __call_object_method_v(JNIEnv *env, jobject receiver,
                                     jmethodID method_id, va_list args)
{
  FakeJniObject *method = __runtime_cast(method_id);
  if (!method || method->kind != FAKE_JNI_METHOD)
    return NULL;

  if (strcmp(method->name, "getClassLoader") == 0)
    return __new_object(env, __find_class(env, "java/lang/ClassLoader"),
                         NULL);

  if (strcmp(method->name, "loadClass") == 0)
  {
    jstring name = va_arg(args, jstring);
    return __find_class(env, jni_runtime_get_string_utf(name));
  }

  (void)receiver;
  return NULL;
}

static jobject __call_object_method(JNIEnv *env, jobject receiver,
                                    jmethodID method, ...)
{
  va_list args;
  va_start(args, method);
  jobject result = __call_object_method_v(env, receiver, method, args);
  va_end(args);
  return result;
}

static jobject __call_object_method_a(JNIEnv *env, jobject receiver,
                                     jmethodID method_id, const jvalue *args)
{
  FakeJniObject *method = __runtime_cast(method_id);
  if (method && method->name && strcmp(method->name, "loadClass") == 0)
    return __find_class(env, args ? jni_runtime_get_string_utf(args[0].l) : NULL);
  if (method && method->name && strcmp(method->name, "getClassLoader") == 0)
    return __new_object(env, __find_class(env, "java/lang/ClassLoader"),
                         NULL);
  (void)receiver;
  return NULL;
}

static jobject __call_static_object_method_v(JNIEnv *env, jclass clazz,
                                           jmethodID method_id, va_list args)
{
  FakeJniObject *method = __runtime_cast(method_id);
  (void)clazz;
  if (!method || !method->name)
    return NULL;

  if (strcmp(method->name, "getCocos2dxPackageName") == 0)
    return get_cocos2dx_package_name();
  if (strcmp(method->name, "getCocos2dxWritablePath") == 0)
    return get_cocos2dx_writable_path();
  if (strcmp(method->name, "getCurrentLanguage") == 0)
    return get_current_language();
  if (strcmp(method->name, "generateGuid") == 0)
    return generate_guid();
  if (strcmp(method->name, "getStringForKey") == 0)
  {
    jstring key = va_arg(args, jstring);
    jstring default_value = va_arg(args, jstring);
    (void)key;
    return default_value;
  }

  log_v(TAG, "JNI unhandled static object method: %s", method->name);
  return NULL;
}

static jobject __call_static_object_method(JNIEnv *env, jclass clazz,
                                          jmethodID method, ...)
{
  va_list args;
  va_start(args, method);
  jobject result = __call_static_object_method_v(env, clazz, method, args);
  va_end(args);
  return result;
}

static jobject __call_static_object_method_a(JNIEnv *env, jclass clazz,
                                           jmethodID method_id,
                                           const jvalue *args)
{
  FakeJniObject *method = __runtime_cast(method_id);
  (void)clazz;
  if (!method || !method->name)
    return NULL;
  if (strcmp(method->name, "getStringForKey") == 0)
    return args ? args[1].l : NULL;
  if (strcmp(method->name, "getCocos2dxPackageName") == 0)
    return get_cocos2dx_package_name();
  if (strcmp(method->name, "getCocos2dxWritablePath") == 0)
    return get_cocos2dx_writable_path();
  if (strcmp(method->name, "getCurrentLanguage") == 0)
    return get_current_language();
  if (strcmp(method->name, "generateGuid") == 0)
    return generate_guid();
  (void)env;
  return NULL;
}

static jint __call_static_int_method_v(JNIEnv *env, jclass clazz,
                                     jmethodID method_id, va_list args)
{
  FakeJniObject *method = __runtime_cast(method_id);
  (void)env;
  (void)clazz;
  if (method && method->name &&
      strcmp(method->name, "getIntegerForKey") == 0)
  {
    (void)va_arg(args, jstring);
    return va_arg(args, jint);
  }
  return 0;
}

static jint __call_static_int_method(JNIEnv *env, jclass clazz,
                                    jmethodID method, ...)
{
  va_list args;
  va_start(args, method);
  jint result = __call_static_int_method_v(env, clazz, method, args);
  va_end(args);
  return result;
}

static jint __call_static_int_method_a(JNIEnv *env, jclass clazz,
                                     jmethodID method_id, const jvalue *args)
{
  FakeJniObject *method = __runtime_cast(method_id);
  (void)env;
  (void)clazz;
  return method && method->name &&
                 strcmp(method->name, "getIntegerForKey") == 0 && args
             ? args[1].i
             : 0;
}

static jboolean __call_static_boolean_method_v(JNIEnv *env, jclass clazz,
                                             jmethodID method_id, va_list args)
{
  FakeJniObject *method = __runtime_cast(method_id);
  (void)env;
  (void)clazz;
  if (method && method->name && strcmp(method->name, "getBoolForKey") == 0)
  {
    (void)va_arg(args, jstring);
    return (jboolean)va_arg(args, jint);
  }
  return JNI_FALSE;
}

static jboolean __call_static_boolean_method(JNIEnv *env, jclass clazz,
                                            jmethodID method, ...)
{
  va_list args;
  va_start(args, method);
  jboolean result = __call_static_boolean_method_v(env, clazz, method, args);
  va_end(args);
  return result;
}

static jboolean __call_static_boolean_method_a(JNIEnv *env, jclass clazz,
                                             jmethodID method_id,
                                             const jvalue *args)
{
  FakeJniObject *method = __runtime_cast(method_id);
  (void)env;
  (void)clazz;
  return method && method->name &&
                 strcmp(method->name, "getBoolForKey") == 0 && args
             ? args[1].z
             : JNI_FALSE;
}

static void __call_static_void_method_v(JNIEnv *env, jclass clazz,
                                      jmethodID method_id, va_list args)
{
  FakeJniObject *method = __runtime_cast(method_id);
  (void)env;
  (void)clazz;
  if (method && method->name && strcmp(method->name, "setStringForKey") == 0)
  {
    jstring key = va_arg(args, jstring);
    jstring value = va_arg(args, jstring);
    set_string_for_key(key, value);
  }
}

static void __call_static_void_method(JNIEnv *env, jclass clazz,
                                     jmethodID method, ...)
{
  va_list args;
  va_start(args, method);
  __call_static_void_method_v(env, clazz, method, args);
  va_end(args);
}

static void __call_static_void_method_a(JNIEnv *env, jclass clazz,
                                      jmethodID method_id, const jvalue *args)
{
  FakeJniObject *method = __runtime_cast(method_id);
  (void)env;
  (void)clazz;
  if (method && method->name && strcmp(method->name, "setStringForKey") == 0 &&
      args)
    set_string_for_key(args[0].l, args[1].l);
}

#define DEFINE_INSTANCE_CALLS(name, type, zero)                            \
  static type __call_##name##_method(JNIEnv *env, jobject object,              \
                                      jmethodID method, ...)                \
  {                                                                         \
    (void)env; (void)object; (void)method; return (zero);                   \
  }                                                                         \
  static type __call_##name##_method_v(JNIEnv *env, jobject object,             \
                                       jmethodID method, va_list args)       \
  {                                                                         \
    (void)env; (void)object; (void)method; (void)args; return (zero);        \
  }                                                                         \
  static type __call_##name##_method_a(JNIEnv *env, jobject object,             \
                                       jmethodID method, const jvalue *args) \
  {                                                                         \
    (void)env; (void)object; (void)method; (void)args; return (zero);        \
  }

#define DEFINE_STATIC_CALLS(name, type, zero)                              \
  static type __call_static_##name##_method(JNIEnv *env, jclass clazz,          \
                                            jmethodID method, ...)          \
  {                                                                         \
    (void)env; (void)clazz; (void)method; return (zero);                    \
  }                                                                         \
  static type __call_static_##name##_method_v(JNIEnv *env, jclass clazz,         \
                                             jmethodID method, va_list args)\
  {                                                                         \
    (void)env; (void)clazz; (void)method; (void)args; return (zero);         \
  }                                                                         \
  static type __call_static_##name##_method_a(JNIEnv *env, jclass clazz,         \
                                             jmethodID method,              \
                                             const jvalue *args)            \
  {                                                                         \
    (void)env; (void)clazz; (void)method; (void)args; return (zero);         \
  }

DEFINE_INSTANCE_CALLS(boolean, jboolean, JNI_FALSE)
DEFINE_INSTANCE_CALLS(byte, jbyte, 0)
DEFINE_INSTANCE_CALLS(char, jchar, 0)
DEFINE_INSTANCE_CALLS(short, jshort, 0)
DEFINE_INSTANCE_CALLS(int, jint, 0)
DEFINE_INSTANCE_CALLS(long, jlong, 0)
DEFINE_INSTANCE_CALLS(float, jfloat, 0.0f)
DEFINE_INSTANCE_CALLS(double, jdouble, 0.0)

DEFINE_STATIC_CALLS(byte, jbyte, 0)
DEFINE_STATIC_CALLS(char, jchar, 0)
DEFINE_STATIC_CALLS(short, jshort, 0)
DEFINE_STATIC_CALLS(long, jlong, 0)
DEFINE_STATIC_CALLS(float, jfloat, 0.0f)
DEFINE_STATIC_CALLS(double, jdouble, 0.0)

static void __call_void_method(JNIEnv *env, jobject object,
                               jmethodID method, ...)
{
  (void)env; (void)object; (void)method;
}

static void __call_void_method_v(JNIEnv *env, jobject object,
                                jmethodID method, va_list args)
{
  (void)env; (void)object; (void)method; (void)args;
}

static void __call_void_method_a(JNIEnv *env, jobject object,
                                jmethodID method, const jvalue *args)
{
  (void)env; (void)object; (void)method; (void)args;
}

#define DEFINE_NONVIRTUAL_CALLS(name, type, zero)                         \
  static type __call_nonvirtual_##name##_method(                               \
      JNIEnv *env, jobject object, jclass clazz, jmethodID method, ...)    \
  {                                                                         \
    (void)env; (void)object; (void)clazz; (void)method; return (zero);      \
  }                                                                         \
  static type __call_nonvirtual_##name##_method_v(                              \
      JNIEnv *env, jobject object, jclass clazz, jmethodID method,          \
      va_list args)                                                         \
  {                                                                         \
    (void)env; (void)object; (void)clazz; (void)method; (void)args;          \
    return (zero);                                                          \
  }                                                                         \
  static type __call_nonvirtual_##name##_method_a(                              \
      JNIEnv *env, jobject object, jclass clazz, jmethodID method,          \
      const jvalue *args)                                                   \
  {                                                                         \
    (void)env; (void)object; (void)clazz; (void)method; (void)args;          \
    return (zero);                                                          \
  }

DEFINE_NONVIRTUAL_CALLS(object, jobject, NULL)
DEFINE_NONVIRTUAL_CALLS(boolean, jboolean, JNI_FALSE)
DEFINE_NONVIRTUAL_CALLS(byte, jbyte, 0)
DEFINE_NONVIRTUAL_CALLS(char, jchar, 0)
DEFINE_NONVIRTUAL_CALLS(short, jshort, 0)
DEFINE_NONVIRTUAL_CALLS(int, jint, 0)
DEFINE_NONVIRTUAL_CALLS(long, jlong, 0)
DEFINE_NONVIRTUAL_CALLS(float, jfloat, 0.0f)
DEFINE_NONVIRTUAL_CALLS(double, jdouble, 0.0)

static void __call_nonvirtual_void_method(JNIEnv *env, jobject object,
                                         jclass clazz, jmethodID method, ...)
{
  (void)env; (void)object; (void)clazz; (void)method;
}

static void __call_nonvirtual_void_method_v(JNIEnv *env, jobject object,
                                          jclass clazz, jmethodID method,
                                          va_list args)
{
  (void)env; (void)object; (void)clazz; (void)method; (void)args;
}

static void __call_nonvirtual_void_method_a(JNIEnv *env, jobject object,
                                          jclass clazz, jmethodID method,
                                          const jvalue *args)
{
  (void)env; (void)object; (void)clazz; (void)method; (void)args;
}

#define DEFINE_FIELD_ACCESSORS(name, type, zero, value_type)                \
  static type __get_##name##_field(JNIEnv *env, jobject object,                 \
                                    jfieldID field)                          \
  {                                                                          \
    (void)env; (void)object; (void)field; return (zero);                     \
  }                                                                          \
  static void __set_##name##_field(JNIEnv *env, jobject object,                 \
                                    jfieldID field, value_type value)          \
  {                                                                          \
    (void)env; (void)object; (void)field; (void)value;                        \
  }                                                                          \
  static type __get_static_##name##_field(JNIEnv *env, jclass clazz,             \
                                          jfieldID field)                    \
  {                                                                          \
    (void)env; (void)clazz; (void)field; return (zero);                      \
  }                                                                          \
  static void __set_static_##name##_field(JNIEnv *env, jclass clazz,             \
                                          jfieldID field, value_type value)    \
  {                                                                          \
    (void)env; (void)clazz; (void)field; (void)value;                         \
  }

DEFINE_FIELD_ACCESSORS(object, jobject, NULL, jobject)
DEFINE_FIELD_ACCESSORS(boolean, jboolean, JNI_FALSE, jboolean)
DEFINE_FIELD_ACCESSORS(byte, jbyte, 0, jbyte)
DEFINE_FIELD_ACCESSORS(char, jchar, 0, jchar)
DEFINE_FIELD_ACCESSORS(short, jshort, 0, jshort)
DEFINE_FIELD_ACCESSORS(int, jint, 0, jint)
DEFINE_FIELD_ACCESSORS(long, jlong, 0, jlong)
DEFINE_FIELD_ACCESSORS(float, jfloat, 0.0f, jfloat)
DEFINE_FIELD_ACCESSORS(double, jdouble, 0.0, jdouble)

static jsize __get_string_length(JNIEnv *env, jstring string)
{
  (void)env;
  FakeJniObject *object = __runtime_cast(string);
  return object && object->kind == FAKE_JNI_STRING ? object->length : 0;
}

static jsize __get_string_utf_length(JNIEnv *env, jstring string)
{
  (void)env;
  const char *text = jni_runtime_get_string_utf(string);
  return text ? (jsize)strlen(text) : 0;
}

static const jchar *__get_string_chars(JNIEnv *env, jstring string,
                                       jboolean *is_copy)
{
  (void)env;
  FakeJniObject *object = __runtime_cast(string);
  if (is_copy)
    *is_copy = JNI_FALSE;
  return object && object->kind == FAKE_JNI_STRING ? object->utf16 : NULL;
}

static const char *__get_string_utf_chars(JNIEnv *env, jstring string,
                                         jboolean *is_copy)
{
  (void)env;
  if (is_copy)
    *is_copy = JNI_FALSE;
  return jni_runtime_get_string_utf(string);
}

static void __release_string_chars(JNIEnv *env, jstring string,
                                   const jchar *chars)
{
  (void)env; (void)string; (void)chars;
}

static void __release_string_utf_chars(JNIEnv *env, jstring string,
                                      const char *chars)
{
  (void)env; (void)string; (void)chars;
}

static FakeJniObject *__runtime_new_array(jsize length, size_t element_size)
{
  if (length < 0)
    return NULL;

  FakeJniObject *array = __runtime_alloc(FAKE_JNI_ARRAY);
  if (!array)
    return NULL;
  array->length = length;
  array->element_size = element_size;
  array->data = calloc((size_t)length, element_size);
  return array;
}

static jsize __get_array_length(JNIEnv *env, jarray array)
{
  (void)env;
  FakeJniObject *object = __runtime_cast(array);
  return object && object->kind == FAKE_JNI_ARRAY ? object->length : 0;
}

static jobjectArray __new_object_array(JNIEnv *env, jsize length,
                                       jclass clazz, jobject initial)
{
  (void)env;
  FakeJniObject *array = __runtime_new_array(length, sizeof(jobject));
  if (!array)
    return NULL;
  array->clazz = __runtime_cast(clazz);
  for (jsize index = 0; index < length; ++index)
    ((jobject *)array->data)[index] = initial;
  return (jobjectArray)array;
}

static jobject __get_object_array_element(JNIEnv *env, jobjectArray array,
                                         jsize index)
{
  (void)env;
  FakeJniObject *object = __runtime_cast(array);
  if (!object || object->kind != FAKE_JNI_ARRAY ||
      index < 0 || index >= object->length)
    return NULL;
  return ((jobject *)object->data)[index];
}

static void __set_object_array_element(JNIEnv *env, jobjectArray array,
                                      jsize index, jobject value)
{
  (void)env;
  FakeJniObject *object = __runtime_cast(array);
  if (object && object->kind == FAKE_JNI_ARRAY &&
      index >= 0 && index < object->length)
    ((jobject *)object->data)[index] = value;
}

#define DEFINE_PRIMITIVE_ARRAY(name, type, array_type)                      \
  static array_type __new_##name##_array(JNIEnv *env, jsize length)             \
  {                                                                         \
    (void)env; return (array_type)__runtime_new_array(length, sizeof(type));      \
  }                                                                         \
  static type *__get_##name##_array_elements(JNIEnv *env, array_type array,      \
                                             jboolean *is_copy)              \
  {                                                                         \
    (void)env;                                                               \
    FakeJniObject *object = __runtime_cast(array);                              \
    if (is_copy) *is_copy = JNI_FALSE;                                         \
    return object && object->kind == FAKE_JNI_ARRAY ? object->data : NULL;   \
  }                                                                         \
  static void __release_##name##_array_elements(                                \
      JNIEnv *env, array_type array, type *elements, jint mode)              \
  {                                                                         \
    (void)env; (void)array; (void)elements; (void)mode;                      \
  }                                                                         \
  static void __get_##name##_array_region(                                     \
      JNIEnv *env, array_type array, jsize start, jsize length, type *out)    \
  {                                                                         \
    (void)env; FakeJniObject *object = __runtime_cast(array);                   \
    if (!object || !out || start < 0 || length < 0 ||                        \
        start + length > object->length) return;                             \
    memcpy(out, (type *)object->data + start, (size_t)length * sizeof(type));\
  }                                                                         \
  static void __set_##name##_array_region(                                     \
      JNIEnv *env, array_type array, jsize start, jsize length,               \
      const type *input)                                                     \
  {                                                                         \
    (void)env; FakeJniObject *object = __runtime_cast(array);                   \
    if (!object || !input || start < 0 || length < 0 ||                      \
        start + length > object->length) return;                             \
    memcpy((type *)object->data + start, input,                              \
           (size_t)length * sizeof(type));                                   \
  }

DEFINE_PRIMITIVE_ARRAY(boolean, jboolean, jbooleanArray)
DEFINE_PRIMITIVE_ARRAY(byte, jbyte, jbyteArray)
DEFINE_PRIMITIVE_ARRAY(char, jchar, jcharArray)
DEFINE_PRIMITIVE_ARRAY(short, jshort, jshortArray)
DEFINE_PRIMITIVE_ARRAY(int, jint, jintArray)
DEFINE_PRIMITIVE_ARRAY(long, jlong, jlongArray)
DEFINE_PRIMITIVE_ARRAY(float, jfloat, jfloatArray)
DEFINE_PRIMITIVE_ARRAY(double, jdouble, jdoubleArray)

static jint __register_natives(JNIEnv *env, jclass clazz,
                                const JNINativeMethod *methods,
                                jint method_count)
{
  (void)env;
  if (!clazz || !methods || method_count < 0)
    return JNI_EINVAL;

  for (jint index = 0; index < method_count; ++index)
  {
    FakeJniObject *method = __runtime_find_member(
        clazz, methods[index].name, methods[index].signature,
        FAKE_JNI_METHOD, JNI_FALSE);
    if (!method)
      return JNI_ENOMEM;
    method->native_proc = methods[index].fnPtr;
    log_v(TAG, "JNI RegisterNatives: %s %s -> 0x%08X",
         methods[index].name, methods[index].signature,
         (uintptr_t)methods[index].fnPtr);
  }
  return JNI_OK;
}

static jint __unregister_natives(JNIEnv *env, jclass clazz)
{
  (void)env;
  FakeJniObject *class_object = __runtime_cast(clazz);
  for (FakeJniObject *object = __fake_objects; object; object = object->next)
    if (object->kind == FAKE_JNI_METHOD && object->clazz == class_object)
      object->native_proc = NULL;
  return JNI_OK;
}

static jint __get_java_vm(JNIEnv *env, JavaVM **vm)
{
  (void)env;
  if (!vm)
    return JNI_EINVAL;
  *vm = jni_runtime_get_java_vm();
  return JNI_OK;
}

static jint __get_version(JNIEnv *env)
{
  (void)env;
  return JNI_VERSION_1_6;
}

static jclass __define_class(JNIEnv *env, const char *name, jobject loader,
                              const jbyte *data, jsize length)
{
  (void)loader; (void)data; (void)length;
  return __find_class(env, name);
}

static jmethodID __from_reflected_method(JNIEnv *env, jobject method)
{
  (void)env;
  return (jmethodID)method;
}

static jfieldID __from_reflected_field(JNIEnv *env, jobject field)
{
  (void)env;
  return (jfieldID)field;
}

static jobject __to_reflected_method(JNIEnv *env, jclass clazz,
                                     jmethodID method, jboolean is_static)
{
  (void)env; (void)clazz; (void)is_static;
  return (jobject)method;
}

static jobject __to_reflected_field(JNIEnv *env, jclass clazz,
                                    jfieldID field, jboolean is_static)
{
  (void)env; (void)clazz; (void)is_static;
  return (jobject)field;
}

static jclass __get_superclass(JNIEnv *env, jclass clazz)
{
  (void)clazz;
  return __find_class(env, "java/lang/Object");
}

static jboolean __is_assignable_from(JNIEnv *env, jclass first, jclass second)
{
  (void)env;
  return first == second ? JNI_TRUE : JNI_FALSE;
}

static jint __throw(JNIEnv *env, jthrowable throwable)
{
  (void)env;
  __pending_exception = throwable;
  return JNI_OK;
}

static jint __throw_new(JNIEnv *env, jclass clazz, const char *message)
{
  (void)clazz;
  __pending_exception = (jthrowable)__new_string_utf(env, message);
  return __pending_exception ? JNI_OK : JNI_ENOMEM;
}

static jthrowable __exception_occurred(JNIEnv *env)
{
  (void)env;
  return __pending_exception;
}

static void __exception_describe(JNIEnv *env)
{
  (void)env;
  if (__pending_exception)
    log_w(TAG, "JNI fake exception is pending.");
}

static void __exception_clear(JNIEnv *env)
{
  (void)env;
  __pending_exception = NULL;
}

static jboolean __exception_check(JNIEnv *env)
{
  (void)env;
  return __pending_exception ? JNI_TRUE : JNI_FALSE;
}

static void __fatal_error(JNIEnv *env, const char *message)
{
  (void)env;
  log_f(TAG, "JNI FatalError: %s", message ? message : "(null)");
}

static jint __push_local_frame(JNIEnv *env, jint capacity)
{
  (void)env; (void)capacity;
  return JNI_OK;
}

static jobject __pop_local_frame(JNIEnv *env, jobject result)
{
  (void)env;
  return result;
}

static jobject __new_global_ref(JNIEnv *env, jobject object)
{
  (void)env;
  FakeJniObject *fake = __runtime_cast(object);
  if (fake)
    ++fake->global_refs;
  return object;
}

static void __delete_global_ref(JNIEnv *env, jobject object)
{
  (void)env;
  FakeJniObject *fake = __runtime_cast(object);
  if (fake && fake->global_refs)
    --fake->global_refs;
}

static void __delete_local_ref(JNIEnv *env, jobject object)
{
  (void)env; (void)object;
}

static jboolean __is_same_object(JNIEnv *env, jobject first, jobject second)
{
  (void)env;
  return first == second ? JNI_TRUE : JNI_FALSE;
}

static jobject __new_local_ref(JNIEnv *env, jobject object)
{
  (void)env;
  return object;
}

static jint __ensure_local_capacity(JNIEnv *env, jint capacity)
{
  (void)env; (void)capacity;
  return JNI_OK;
}

static jobject __alloc_object(JNIEnv *env, jclass clazz)
{
  return __new_object(env, clazz, NULL);
}

static jclass __get_object_class(JNIEnv *env, jobject object)
{
  FakeJniObject *fake = __runtime_cast(object);
  if (fake && fake->clazz)
    return (jclass)fake->clazz;
  if (fake && fake->kind == FAKE_JNI_CLASS)
    return __find_class(env, "java/lang/Class");
  return __find_class(env, "java/lang/Object");
}

static jboolean __is_instance_of(JNIEnv *env, jobject object, jclass clazz)
{
  return __get_object_class(env, object) == clazz ? JNI_TRUE : JNI_FALSE;
}

static void __get_string_region(JNIEnv *env, jstring string,
                                jsize start, jsize length, jchar *out)
{
  const jchar *chars = __get_string_chars(env, string, NULL);
  jsize total = __get_string_length(env, string);
  if (chars && out && start >= 0 && length >= 0 && start + length <= total)
    memcpy(out, chars + start, (size_t)length * sizeof(*out));
}

static void __get_string_utf_region(JNIEnv *env, jstring string,
                                   jsize start, jsize length, char *out)
{
  const char *chars = __get_string_utf_chars(env, string, NULL);
  jsize total = __get_string_utf_length(env, string);
  if (chars && out && start >= 0 && length >= 0 && start + length <= total)
    memcpy(out, chars + start, (size_t)length);
}

static void *__get_primitive_array_critical(JNIEnv *env, jarray array,
                                            jboolean *is_copy)
{
  (void)env;
  FakeJniObject *object = __runtime_cast(array);
  if (is_copy)
    *is_copy = JNI_FALSE;
  return object && object->kind == FAKE_JNI_ARRAY ? object->data : NULL;
}

static void __release_primitive_array_critical(JNIEnv *env, jarray array,
                                               void *data, jint mode)
{
  (void)env; (void)array; (void)data; (void)mode;
}

static const jchar *__get_string_critical(JNIEnv *env, jstring string,
                                          jboolean *is_copy)
{
  return __get_string_chars(env, string, is_copy);
}

static void __release_string_critical(JNIEnv *env, jstring string,
                                      const jchar *chars)
{
  __release_string_chars(env, string, chars);
}

static jweak __new_weak_global_ref(JNIEnv *env, jobject object)
{
  (void)env;
  return (jweak)object;
}

static void __delete_weak_global_ref(JNIEnv *env, jweak object)
{
  (void)env; (void)object;
}

static jobject __new_direct_byte_buffer(JNIEnv *env, void *address,
                                       jlong capacity)
{
  (void)env;
  if (capacity < 0)
    return NULL;
  FakeJniObject *buffer = __runtime_alloc(FAKE_JNI_DIRECT_BUFFER);
  if (!buffer)
    return NULL;
  buffer->data = address;
  buffer->length = (jsize)capacity;
  return (jobject)buffer;
}

static void *__get_direct_buffer_address(JNIEnv *env, jobject buffer)
{
  (void)env;
  FakeJniObject *object = __runtime_cast(buffer);
  return object && object->kind == FAKE_JNI_DIRECT_BUFFER
             ? object->data : NULL;
}

static jlong __get_direct_buffer_capacity(JNIEnv *env, jobject buffer)
{
  (void)env;
  FakeJniObject *object = __runtime_cast(buffer);
  return object && object->kind == FAKE_JNI_DIRECT_BUFFER
             ? object->length : -1;
}

static jobjectRefType __get_object_ref_type(JNIEnv *env, jobject object)
{
  (void)env;
  FakeJniObject *fake = __runtime_cast(object);
  if (!fake)
    return JNIInvalidRefType;
  return fake->global_refs ? JNIGlobalRefType : JNILocalRefType;
}

static jint __monitor_enter(JNIEnv *env, jobject object)
{
  (void)env; (void)object;
  return JNI_OK;
}

static jint __monitor_exit(JNIEnv *env, jobject object)
{
  (void)env; (void)object;
  return JNI_OK;
}

jint destroy_java_vm(JavaVM *java_vm)
{
  (void)java_vm;
  log_v(TAG, "Called DestroyJavaVM");
  return JNI_OK;
}

jint attach_current_thread(JavaVM *java_vm, JNIEnv **jni_env, void *args)
{
  (void)java_vm;
  (void)args;
  if (!jni_env)
    return JNI_EINVAL;

  *jni_env = jni_runtime_get_env();
  log_v(TAG, "Called AttachCurrentThread");
  return JNI_OK;
}

jint detach_current_thread(JavaVM *java_vm)
{
  (void)java_vm;
  log_v(TAG, "Called DetachCurrentThread");
  return JNI_OK;
}

jint attach_current_thread_as_daemon(JavaVM *java_vm, JNIEnv **jni_env, void *args)
{
  (void)java_vm;
  (void)args;
  if (!jni_env)
    return JNI_EINVAL;

  *jni_env = jni_runtime_get_env();
  log_v(TAG, "Called AttachCurrentThreadAsDaemon");
  return JNI_OK;
}

jint get_env(JavaVM *java_vm, void **jni_env, jint version)
{
  (void)java_vm;
  if (!jni_env)
    return JNI_EINVAL;

  if (version != JNI_VERSION_1_1 &&
      version != JNI_VERSION_1_2 &&
      version != JNI_VERSION_1_4 &&
      version != JNI_VERSION_1_6)
    return JNI_EVERSION;

  *jni_env = jni_runtime_get_env();
  log_v(TAG, "Called GetEnv(version=0x%08X)", version);
  return JNI_OK;
}

static struct JNINativeInterface __fake_native_interface =
{
  .GetVersion = __get_version,
  .DefineClass = __define_class,
  .FindClass = __find_class,
  .FromReflectedMethod = __from_reflected_method,
  .FromReflectedField = __from_reflected_field,
  .ToReflectedMethod = __to_reflected_method,
  .GetSuperclass = __get_superclass,
  .IsAssignableFrom = __is_assignable_from,
  .ToReflectedField = __to_reflected_field,
  .Throw = __throw,
  .ThrowNew = __throw_new,
  .ExceptionOccurred = __exception_occurred,
  .ExceptionDescribe = __exception_describe,
  .ExceptionClear = __exception_clear,
  .FatalError = __fatal_error,
  .PushLocalFrame = __push_local_frame,
  .PopLocalFrame = __pop_local_frame,
  .NewGlobalRef = __new_global_ref,
  .DeleteGlobalRef = __delete_global_ref,
  .DeleteLocalRef = __delete_local_ref,
  .IsSameObject = __is_same_object,
  .NewLocalRef = __new_local_ref,
  .EnsureLocalCapacity = __ensure_local_capacity,
  .AllocObject = __alloc_object,
  .NewObject = __new_object,
  .NewObjectV = __new_object_v,
  .NewObjectA = __new_object_a,
  .GetObjectClass = __get_object_class,
  .IsInstanceOf = __is_instance_of,
  .GetMethodID = __get_method_id,
  .CallObjectMethod = __call_object_method,
  .CallObjectMethodV = __call_object_method_v,
  .CallObjectMethodA = __call_object_method_a,
  .CallBooleanMethod = __call_boolean_method,
  .CallBooleanMethodV = __call_boolean_method_v,
  .CallBooleanMethodA = __call_boolean_method_a,
  .CallByteMethod = __call_byte_method,
  .CallByteMethodV = __call_byte_method_v,
  .CallByteMethodA = __call_byte_method_a,
  .CallCharMethod = __call_char_method,
  .CallCharMethodV = __call_char_method_v,
  .CallCharMethodA = __call_char_method_a,
  .CallShortMethod = __call_short_method,
  .CallShortMethodV = __call_short_method_v,
  .CallShortMethodA = __call_short_method_a,
  .CallIntMethod = __call_int_method,
  .CallIntMethodV = __call_int_method_v,
  .CallIntMethodA = __call_int_method_a,
  .CallLongMethod = __call_long_method,
  .CallLongMethodV = __call_long_method_v,
  .CallLongMethodA = __call_long_method_a,
  .CallFloatMethod = __call_float_method,
  .CallFloatMethodV = __call_float_method_v,
  .CallFloatMethodA = __call_float_method_a,
  .CallDoubleMethod = __call_double_method,
  .CallDoubleMethodV = __call_double_method_v,
  .CallDoubleMethodA = __call_double_method_a,
  .CallVoidMethod = __call_void_method,
  .CallVoidMethodV = __call_void_method_v,
  .CallVoidMethodA = __call_void_method_a,
  .CallNonvirtualObjectMethod = __call_nonvirtual_object_method,
  .CallNonvirtualObjectMethodV = __call_nonvirtual_object_method_v,
  .CallNonvirtualObjectMethodA = __call_nonvirtual_object_method_a,
  .CallNonvirtualBooleanMethod = __call_nonvirtual_boolean_method,
  .CallNonvirtualBooleanMethodV = __call_nonvirtual_boolean_method_v,
  .CallNonvirtualBooleanMethodA = __call_nonvirtual_boolean_method_a,
  .CallNonvirtualByteMethod = __call_nonvirtual_byte_method,
  .CallNonvirtualByteMethodV = __call_nonvirtual_byte_method_v,
  .CallNonvirtualByteMethodA = __call_nonvirtual_byte_method_a,
  .CallNonvirtualCharMethod = __call_nonvirtual_char_method,
  .CallNonvirtualCharMethodV = __call_nonvirtual_char_method_v,
  .CallNonvirtualCharMethodA = __call_nonvirtual_char_method_a,
  .CallNonvirtualShortMethod = __call_nonvirtual_short_method,
  .CallNonvirtualShortMethodV = __call_nonvirtual_short_method_v,
  .CallNonvirtualShortMethodA = __call_nonvirtual_short_method_a,
  .CallNonvirtualIntMethod = __call_nonvirtual_int_method,
  .CallNonvirtualIntMethodV = __call_nonvirtual_int_method_v,
  .CallNonvirtualIntMethodA = __call_nonvirtual_int_method_a,
  .CallNonvirtualLongMethod = __call_nonvirtual_long_method,
  .CallNonvirtualLongMethodV = __call_nonvirtual_long_method_v,
  .CallNonvirtualLongMethodA = __call_nonvirtual_long_method_a,
  .CallNonvirtualFloatMethod = __call_nonvirtual_float_method,
  .CallNonvirtualFloatMethodV = __call_nonvirtual_float_method_v,
  .CallNonvirtualFloatMethodA = __call_nonvirtual_float_method_a,
  .CallNonvirtualDoubleMethod = __call_nonvirtual_double_method,
  .CallNonvirtualDoubleMethodV = __call_nonvirtual_double_method_v,
  .CallNonvirtualDoubleMethodA = __call_nonvirtual_double_method_a,
  .CallNonvirtualVoidMethod = __call_nonvirtual_void_method,
  .CallNonvirtualVoidMethodV = __call_nonvirtual_void_method_v,
  .CallNonvirtualVoidMethodA = __call_nonvirtual_void_method_a,
  .GetFieldID = __get_field_id,
  .GetObjectField = __get_object_field,
  .GetBooleanField = __get_boolean_field,
  .GetByteField = __get_byte_field,
  .GetCharField = __get_char_field,
  .GetShortField = __get_short_field,
  .GetIntField = __get_int_field,
  .GetLongField = __get_long_field,
  .GetFloatField = __get_float_field,
  .GetDoubleField = __get_double_field,
  .SetObjectField = __set_object_field,
  .SetBooleanField = __set_boolean_field,
  .SetByteField = __set_byte_field,
  .SetCharField = __set_char_field,
  .SetShortField = __set_short_field,
  .SetIntField = __set_int_field,
  .SetLongField = __set_long_field,
  .SetFloatField = __set_float_field,
  .SetDoubleField = __set_double_field,
  .GetStaticMethodID = __get_static_method_id,
  .CallStaticObjectMethod = __call_static_object_method,
  .CallStaticObjectMethodV = __call_static_object_method_v,
  .CallStaticObjectMethodA = __call_static_object_method_a,
  .CallStaticBooleanMethod = __call_static_boolean_method,
  .CallStaticBooleanMethodV = __call_static_boolean_method_v,
  .CallStaticBooleanMethodA = __call_static_boolean_method_a,
  .CallStaticByteMethod = __call_static_byte_method,
  .CallStaticByteMethodV = __call_static_byte_method_v,
  .CallStaticByteMethodA = __call_static_byte_method_a,
  .CallStaticCharMethod = __call_static_char_method,
  .CallStaticCharMethodV = __call_static_char_method_v,
  .CallStaticCharMethodA = __call_static_char_method_a,
  .CallStaticShortMethod = __call_static_short_method,
  .CallStaticShortMethodV = __call_static_short_method_v,
  .CallStaticShortMethodA = __call_static_short_method_a,
  .CallStaticIntMethod = __call_static_int_method,
  .CallStaticIntMethodV = __call_static_int_method_v,
  .CallStaticIntMethodA = __call_static_int_method_a,
  .CallStaticLongMethod = __call_static_long_method,
  .CallStaticLongMethodV = __call_static_long_method_v,
  .CallStaticLongMethodA = __call_static_long_method_a,
  .CallStaticFloatMethod = __call_static_float_method,
  .CallStaticFloatMethodV = __call_static_float_method_v,
  .CallStaticFloatMethodA = __call_static_float_method_a,
  .CallStaticDoubleMethod = __call_static_double_method,
  .CallStaticDoubleMethodV = __call_static_double_method_v,
  .CallStaticDoubleMethodA = __call_static_double_method_a,
  .CallStaticVoidMethod = __call_static_void_method,
  .CallStaticVoidMethodV = __call_static_void_method_v,
  .CallStaticVoidMethodA = __call_static_void_method_a,
  .GetStaticFieldID = __get_static_field_id,
  .GetStaticObjectField = __get_static_object_field,
  .GetStaticBooleanField = __get_static_boolean_field,
  .GetStaticByteField = __get_static_byte_field,
  .GetStaticCharField = __get_static_char_field,
  .GetStaticShortField = __get_static_short_field,
  .GetStaticIntField = __get_static_int_field,
  .GetStaticLongField = __get_static_long_field,
  .GetStaticFloatField = __get_static_float_field,
  .GetStaticDoubleField = __get_static_double_field,
  .SetStaticObjectField = __set_static_object_field,
  .SetStaticBooleanField = __set_static_boolean_field,
  .SetStaticByteField = __set_static_byte_field,
  .SetStaticCharField = __set_static_char_field,
  .SetStaticShortField = __set_static_short_field,
  .SetStaticIntField = __set_static_int_field,
  .SetStaticLongField = __set_static_long_field,
  .SetStaticFloatField = __set_static_float_field,
  .SetStaticDoubleField = __set_static_double_field,
  .NewString = __new_string,
  .GetStringLength = __get_string_length,
  .GetStringChars = __get_string_chars,
  .ReleaseStringChars = __release_string_chars,
  .NewStringUTF = __new_string_utf,
  .GetStringUTFLength = __get_string_utf_length,
  .GetStringUTFChars = __get_string_utf_chars,
  .ReleaseStringUTFChars = __release_string_utf_chars,
  .GetArrayLength = __get_array_length,
  .NewObjectArray = __new_object_array,
  .GetObjectArrayElement = __get_object_array_element,
  .SetObjectArrayElement = __set_object_array_element,
  .NewBooleanArray = __new_boolean_array,
  .NewByteArray = __new_byte_array,
  .NewCharArray = __new_char_array,
  .NewShortArray = __new_short_array,
  .NewIntArray = __new_int_array,
  .NewLongArray = __new_long_array,
  .NewFloatArray = __new_float_array,
  .NewDoubleArray = __new_double_array,
  .GetBooleanArrayElements = __get_boolean_array_elements,
  .GetByteArrayElements = __get_byte_array_elements,
  .GetCharArrayElements = __get_char_array_elements,
  .GetShortArrayElements = __get_short_array_elements,
  .GetIntArrayElements = __get_int_array_elements,
  .GetLongArrayElements = __get_long_array_elements,
  .GetFloatArrayElements = __get_float_array_elements,
  .GetDoubleArrayElements = __get_double_array_elements,
  .ReleaseBooleanArrayElements = __release_boolean_array_elements,
  .ReleaseByteArrayElements = __release_byte_array_elements,
  .ReleaseCharArrayElements = __release_char_array_elements,
  .ReleaseShortArrayElements = __release_short_array_elements,
  .ReleaseIntArrayElements = __release_int_array_elements,
  .ReleaseLongArrayElements = __release_long_array_elements,
  .ReleaseFloatArrayElements = __release_float_array_elements,
  .ReleaseDoubleArrayElements = __release_double_array_elements,
  .GetBooleanArrayRegion = __get_boolean_array_region,
  .GetByteArrayRegion = __get_byte_array_region,
  .GetCharArrayRegion = __get_char_array_region,
  .GetShortArrayRegion = __get_short_array_region,
  .GetIntArrayRegion = __get_int_array_region,
  .GetLongArrayRegion = __get_long_array_region,
  .GetFloatArrayRegion = __get_float_array_region,
  .GetDoubleArrayRegion = __get_double_array_region,
  .SetBooleanArrayRegion = __set_boolean_array_region,
  .SetByteArrayRegion = __set_byte_array_region,
  .SetCharArrayRegion = __set_char_array_region,
  .SetShortArrayRegion = __set_short_array_region,
  .SetIntArrayRegion = __set_int_array_region,
  .SetLongArrayRegion = __set_long_array_region,
  .SetFloatArrayRegion = __set_float_array_region,
  .SetDoubleArrayRegion = __set_double_array_region,
  .RegisterNatives = __register_natives,
  .UnregisterNatives = __unregister_natives,
  .MonitorEnter = __monitor_enter,
  .MonitorExit = __monitor_exit,
  .GetJavaVM = __get_java_vm,
  .GetStringRegion = __get_string_region,
  .GetStringUTFRegion = __get_string_utf_region,
  .GetPrimitiveArrayCritical = __get_primitive_array_critical,
  .ReleasePrimitiveArrayCritical = __release_primitive_array_critical,
  .GetStringCritical = __get_string_critical,
  .ReleaseStringCritical = __release_string_critical,
  .NewWeakGlobalRef = __new_weak_global_ref,
  .DeleteWeakGlobalRef = __delete_weak_global_ref,
  .ExceptionCheck = __exception_check,
  .NewDirectByteBuffer = __new_direct_byte_buffer,
  .GetDirectBufferAddress = __get_direct_buffer_address,
  .GetDirectBufferCapacity = __get_direct_buffer_capacity,
  .GetObjectRefType = __get_object_ref_type
};

static struct JNIInvokeInterface __fake_invoke_interface =
{
  .DestroyJavaVM = destroy_java_vm,
  .AttachCurrentThread = attach_current_thread,
  .DetachCurrentThread = detach_current_thread,
  .GetEnv = get_env,
  .AttachCurrentThreadAsDaemon = attach_current_thread_as_daemon
};

static JNIEnv __fake_env = &__fake_native_interface;
static JavaVM __fake_java_vm = &__fake_invoke_interface;

JNIEnv *jni_runtime_get_env(void)
{
  return &__fake_env;
}

JavaVM *jni_runtime_get_java_vm(void)
{
  return &__fake_java_vm;
}
