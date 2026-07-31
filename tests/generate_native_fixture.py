"""Generate a full Java parameter-slot-limit native callback for host/ARM tests."""

from pathlib import Path
import sys

output = Path(sys.argv[1])
output.parent.mkdir(parents=True, exist_ok=True)
parameters = ", ".join(f"jint a{index}" for index in range(255))
checks = "\n".join(f"  if(a{index} != {index * 7 - 300}) return -{index + 1};" for index in range(255))
arguments = "\n".join(f"  args[{index}].i = {index * 7 - 300};" for index in range(255))
wide_parameters = ", ".join(f"jlong a{index}" for index in range(127))
wide_checks = "\n".join(f"  if(a{index} != INT64_C(0x1234567800000000) + {index * 37}) return -{index + 1};" for index in range(127))
wide_arguments = "\n".join(f"  args[{index}].j = INT64_C(0x1234567800000000) + {index * 37};" for index in range(127))
plain_arguments = ", ".join(str(index * 7 - 300) for index in range(255))
signature = "(" + "I" * 255 + ")I"
output.with_suffix(".h").write_text(
    '#include <jni/aosp/jni.h>\n'
    f'jint VITAARC_SOFTFP test_native_255(JNIEnv *env, jclass clazz, {parameters});\n'
    f'jlong VITAARC_SOFTFP test_native_127_wide(JNIEnv *env, jclass clazz, {wide_parameters}, jint tail);\n',
    encoding="utf-8", newline="\n")
output.write_text(f'''#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <jni/descriptor.h>
#include <jni/runtime.h>
#include <jni/native_call.h>
#include "{output.with_suffix('.h').name}"

jint VITAARC_SOFTFP test_native_255(JNIEnv *env, jclass clazz, {parameters})
{{
  (void)env; (void)clazz;
{checks}
  return 123456789;
}}

jlong VITAARC_SOFTFP test_native_127_wide(JNIEnv *env, jclass clazz, {wide_parameters}, jint tail)
{{
  (void)env; (void)clazz;
{wide_checks}
  if(tail != -123) return -999;
  return INT64_C(0x123456789ab);
}}

#ifndef VITAARC_ARM_FIXTURE
void test_native_many(JNIEnv *env)
{{
  jclass clazz = (*env)->FindClass(env, "test/ManyArguments");
  char signature[] = "{signature}";
  jni_sig_t descriptor;
  assert(jni_sig_parse(signature, &descriptor));
  assert(descriptor.argc == 255 && descriptor.slots == 255);
  assert(!(*env)->GetMethodID(env, clazz, "many", signature));
  assert((*env)->ExceptionCheck(env));
  (*env)->ExceptionClear(env);
  JNINativeMethod method = {{"many", signature, test_native_255}};
  assert((*env)->RegisterNatives(env, clazz, &method, 1) == JNI_OK);
  jmethodID id = (*env)->GetStaticMethodID(env, clazz, "many", signature);
  jvalue args[255];
{arguments}
  assert((*env)->CallStaticIntMethodA(env, clazz, id, args) == 123456789);
  assert((*env)->CallStaticIntMethod(env, clazz, id, {plain_arguments}) == 123456789);
  char invalid[260];
  invalid[0] = '(';
  memset(invalid + 1, 'I', 256);
  strcpy(invalid + 257, ")I");
  assert(!jni_sig_parse(invalid, &descriptor));
  memset(invalid + 1, 'J', 128);
  strcpy(invalid + 129, ")I");
  assert(!jni_sig_parse(invalid, &descriptor));
  memset(invalid + 1, 'J', 127);
  strcpy(invalid + 128, "I)I");
  assert(jni_sig_parse(invalid, &descriptor));
  assert(descriptor.argc == 128 && descriptor.slots == 255);
  invalid[130] = 'J';
  method = (JNINativeMethod){{"wide", invalid, test_native_127_wide}};
  assert((*env)->RegisterNatives(env, clazz, &method, 1) == JNI_OK);
  id = (*env)->GetStaticMethodID(env, clazz, "wide", invalid);
{wide_arguments}
  args[127].i = -123;
  assert((*env)->CallStaticLongMethodA(env, clazz, id, args) == INT64_C(0x123456789ab));
}}
#endif
''', encoding="utf-8", newline="\n")
