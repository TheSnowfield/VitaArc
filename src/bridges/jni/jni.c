#include <solibrary/solib.h>
#include <utils/patcher.h>

#include <logcat/logcat.h>
#include <common/define.h>
#include <utils/debug.h>

#include "impl/jvm.h"
#include "impl/jni.h"
#include "impl/cocos.h"

jint GetEnv(JavaVM *javaVM, void **a1, jint i);
jint GetJavaVM(JNIEnv* javaEnv, JavaVM** javaVM);

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
  .GetJavaVM = GetJavaVM,
  .GetMethodID = GetMethodID,
  .GetStaticMethodID = GetStaticMethodID,
  .GetStringChars = GetStringChars,
  .GetStringLength = GetStringLength,
  .CallObjectMethodV = CallObjectMethodV,
  .CallStaticObjectMethodV = CallStaticObjectMethodV,
  .CallStaticIntMethodV = CallStaticIntMethodV,
  .CallStaticBooleanMethodV = CallStaticBooleanMethodV,
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
  *a1 = &jniNativeEnv;

  logV(TAG, "Called GetEnv");
  return 0;
}

jint GetJavaVM(JNIEnv* javaEnv, JavaVM** javaVM)
{
  *javaVM = &jniInvokeEnv;

  logV(TAG, "Called GetJavaVM");
  return 0;
}

void bridgeCallJNIMain(HSOLIB hSoLibrary)
{
  JNI_OnLoad pfnJNIOnload = solibGetProcAddress(hSoLibrary, "JNI_OnLoad");
  debugMemoryDump("ux0:vitaarc/memdump.bin", (void *)0x98000000, 0xA60AA0);
  // debugMemoryDump("ux0:vitaarc/vitaarc.bin", 0x81000000, 0x000100);

  pfnJNIOnload(&jniInvokeEnv, NULL);
}

typedef int (*JNICocosNativeInit)(uint32_t, uint32_t);
typedef int (*JNICocosInitApp)(struct JNINativeInterface *);
typedef int (*JNICocosSetAppVersion)(struct JNINativeInterface *, jstring);
typedef int (*JNICocosSetDeviceId)(struct JNINativeInterface *, jstring);

void bridgeJNICocosNativeInit(HSOLIB hSoLibrary,
                                      uint32_t nScreenWidth, uint32_t nScreenHeight)
{
  JNICocosNativeInit lpfnNativeInit =
      solibGetProcAddress(hSoLibrary, "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit");

  lpfnNativeInit(nScreenWidth, nScreenHeight);
}

void bridgeJNICocosInitApp(HSOLIB hSoLibrary)
{
  JNICocosInitApp lpfnJNICocosInitApp =
      solibGetProcAddress(hSoLibrary, "Java_low_moe_AppActivity_initJVMPlatformUtils");

  lpfnJNICocosInitApp(&jniNativeEnv);
}

void bridgeJNICocosSetAppVersion(HSOLIB hSoLibrary)
{
  JNICocosSetAppVersion lpfnJNICocosSetAppVersion =
      solibGetProcAddress(hSoLibrary, "Java_low_moe_AppActivity_setAppVersion");

  lpfnJNICocosSetAppVersion(&jniNativeEnv, JSTRING("v6.1.6"));
}

void bridgeJNICocosSetDeviceId(HSOLIB hSoLibrary)
{
  JNICocosSetDeviceId lpfnJNICocosSetDeviceId =
      solibGetProcAddress(hSoLibrary, "Java_low_moe_AppActivity_setDeviceId");

  lpfnJNICocosSetDeviceId(&jniNativeEnv, JSTRING("61616161"));
}

