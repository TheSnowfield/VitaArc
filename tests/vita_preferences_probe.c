/* Execute the real game's UserDefault implementation in separate processes.
 * This verifies Cocos/JNI preferences, not chart scores or the game's st3 save. */
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <config.h>
#include <loader.h>
#include <logcat/logcat.h>
#include <bridges/compat/compat.h>
#include <bridges/jni/runtime.h>
#include <bridges/jni/cocos.h>

int main(void)
{
    int result = 1;
    JNIEnv* env = jni_runtime_get_env();
    log_begin(PATH_TO_LOGFILE);
    bridge_compat_initialize();
    dynalib_t* game = loader_load_library(LIBRARY_LIBCOCOS2DCPP);
    if(!game || !loader_init_library(game)) {
        goto done;
    }

    void (*set_vm)(JavaVM*) = loader_get_proc_address(game, "_ZN7cocos2d9JniHelper9setJavaVMEP7_JavaVM");
    void (*set_context)(JNIEnv*, jobject, jobject, jobject) =
        loader_get_proc_address(game, "Java_org_cocos2dx_lib_Cocos2dxHelper_nativeSetContext");
    void* (*get_instance)(void) = loader_get_proc_address(game, "_ZN7cocos2d11UserDefault11getInstanceEv");
    void (*set_integer)(void*, const char*, int) =
        loader_get_proc_address(game, "_ZN7cocos2d11UserDefault16setIntegerForKeyEPKci");
    int (*get_integer)(void*, const char*, int) =
        loader_get_proc_address(game, "_ZN7cocos2d11UserDefault16getIntegerForKeyEPKci");
    void (*delete_key)(void*, const char*) =
        loader_get_proc_address(game, "_ZN7cocos2d11UserDefault17deleteValueForKeyEPKc");
    void (*flush)(void*) = loader_get_proc_address(game, "_ZN7cocos2d11UserDefault5flushEv");
    if(!set_vm || !set_context || !get_instance || !set_integer || !get_integer || !delete_key || !flush) {
        goto done;
    }

    set_vm(jni_runtime_get_java_vm());
    set_context(env, NULL, jni_cocos_get_context(), jni_cocos_get_asset_manager());
    if((*env)->ExceptionCheck(env)) {
        goto done;
    }

    void* preferences = get_instance();
    if(!preferences || (*env)->ExceptionCheck(env)) {
        goto done;
    }

    const char* key = "__vitaarc_native_preference_probe_20261024";
#ifdef VITAARC_PREF_WRITE
    set_integer(preferences, key, 0x51a3b4);
    flush(preferences);
#endif
    int value = get_integer(preferences, key, -7);
    sceClibPrintf("VitaArc preferences probe: value=%d\n", value);
    if(value != 0x51a3b4 || (*env)->ExceptionCheck(env)) {
        goto done;
    }

#ifndef VITAARC_PREF_WRITE
    delete_key(preferences, key);
    flush(preferences);
    if(get_integer(preferences, key, -7) != -7 || (*env)->ExceptionCheck(env)) {
        goto done;
    }

#endif
    sceClibPrintf("VitaArc preferences probe: native UserDefault roundtrip passed\n");
    result = 0;
done:
    if((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
    }

    sceClibPrintf("VitaArc preferences probe: exit=%d\n", result);
    log_end();
    /* Keep the game runtime and any exception/TLS code mapped through process
     * exit. The separate lifecycle probe exercises production unloading. */
    sceKernelExitProcess(result);
    return result;
}
