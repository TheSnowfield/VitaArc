#include <loader.h>

#include <logcat/logcat.h>
#include <utils/debug.h>

#include "runtime.h"

void bridge_call_jni_main(dynalib_t *library)
{
  JNI_OnLoad on_load = loader_get_proc_address(library, "JNI_OnLoad");
  if (!on_load)
  {
    log_w(TAG, "JNI_OnLoad was not exported.");
    return;
  }

  debug_memory_dump("ux0:vitaarc/memdump.bin", (void *)0x98000000, 0xA60AA0);

  jint version = on_load(jni_runtime_get_java_vm(), NULL);
  log_i(TAG, "JNI_OnLoad returned 0x%08X", version);
  if (version != JNI_VERSION_1_1 &&
      version != JNI_VERSION_1_2 &&
      version != JNI_VERSION_1_4 &&
      version != JNI_VERSION_1_6)
    log_w(TAG, "JNI_OnLoad returned an unsupported JNI version.");
}

typedef void (*JNICocosNativeInit)(JNIEnv *, jobject, jint, jint);
typedef void (*JNICocosInitApp)(JNIEnv *, jobject);
typedef void (*JNICocosSetAppVersion)(JNIEnv *, jobject, jstring);
typedef void (*JNICocosSetDeviceId)(JNIEnv *, jobject, jstring);

void bridge_jni_cocos_native_init(dynalib_t *library,
                                  uint32_t screen_width,
                                  uint32_t screen_height)
{
  JNICocosNativeInit native_init = loader_get_proc_address(
      library, "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit");
  if (native_init)
    native_init(jni_runtime_get_env(), NULL, screen_width, screen_height);
}

void bridge_jni_cocos_init_app(dynalib_t *library)
{
  JNICocosInitApp init_app = loader_get_proc_address(
      library, "Java_low_moe_AppActivity_initJVMPlatformUtils");
  if (init_app)
    init_app(jni_runtime_get_env(), NULL);
}

void bridge_jni_cocos_set_app_version(dynalib_t *library)
{
  JNICocosSetAppVersion set_app_version = loader_get_proc_address(
      library, "Java_low_moe_AppActivity_setAppVersion");
  if (set_app_version)
    set_app_version(jni_runtime_get_env(), NULL,
                    jni_runtime_new_string_utf("v6.1.6"));
}

void bridge_jni_cocos_set_device_id(dynalib_t *library)
{
  JNICocosSetDeviceId set_device_id = loader_get_proc_address(
      library, "Java_low_moe_AppActivity_setDeviceId");
  if (set_device_id)
    set_device_id(jni_runtime_get_env(), NULL,
                  jni_runtime_new_string_utf("61616161"));
}
