#include <solibrary/solib.h>
#include <utils/patcher.h>

#include <logcat/logcat.h>
#include <common/define.h>
#include <utils/debug.h>

#include "impl/jvm.h"
#include "impl/jni.h"
#include "impl/cocos.h"

jint GetEnv(JavaVM *javaVM, void **a1, jint i);

static struct JNINativeInterface jniNativeEnv =
{
  .reserved0 = &jniNativeEnv,
  .GetVersion = GetVersion,
  .FromReflectedField = FromReflectedField,
  .FromReflectedMethod = FromReflectedMethod,
  .FindClass = FindClass,
  .ExceptionClear = ExceptionClear,
  .NewGlobalRef = NewGlobalRef,
  .DeleteLocalRef = DeleteLocalRef,
  .GetMethodID = GetMethodID,
  .GetStaticMethodID = GetStaticMethodID,
  .GetStringChars = GetStringChars,
  .GetStringLength = GetStringLength,
  .CallObjectMethodV = CallObjectMethodV,
  .CallStaticObjectMethodV = CallStaticObjectMethodV,
  .CallBooleanMethodV = CallBooleanMethodV,
  .CallIntMethodV = CallIntMethodV,
  .CallFloatMethodV = CallFloatMethodV,
  .CallVoidMethodV = CallVoidMethodV,
  .NewString = NewString,
  .NewStringUTF = NewStringUTF,
  .GetStringUTFChars = GetStringUTFChars,
  .ReleaseStringChars = ReleaseStringChars,
  .ReleaseStringUTFChars = ReleaseStringUTFChars,
  .RegisterNatives = RegisterNatives
};

static struct JNIInvokeInterface jniInvokeEnv =
{
  .reserved0 = &jniInvokeEnv,
  .GetEnv = GetEnv,
  .DestroyJavaVM = DestroyJavaVM,
  .AttachCurrentThread = AttachCurrentThread,
  .DetachCurrentThread = DetachCurrentThread,
  .AttachCurrentThreadAsDaemon = AttachCurrentThreadAsDaemon
};

jint GetEnv(JavaVM *javaVM, void **a1, jint i)
{
  logV(TAG, "Called GetEnv");

  *a1 = &jniNativeEnv;
  return 0;
}

void bridgeCallJNIMain(HSOLIB hSoLibrary)
{
  JNI_OnLoad pfnJNIOnload = solibGetProcAddress(hSoLibrary, "JNI_OnLoad");
  debugMemoryDump("ux0:vitaarc/memdump.bin", (void *)0x98000000, 0xA60AA0);
  // debugMemoryDump("ux0:vitaarc/vitaarc.bin", 0x81000000, 0x000100);

  pfnJNIOnload(&jniInvokeEnv, NULL);
}
