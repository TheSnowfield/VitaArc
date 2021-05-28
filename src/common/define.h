#ifndef _COMMON_DEFINE_H_
#define _COMMON_DEFINE_H_

#define LIBRARY_LIBFMOD "ux0:vitaarc/library/armeabi-v7a/libfmod.so"
#define LIBRARY_LIBFMODPROVIDER "ux0:vitaarc/library/armeabi-v7a/libfmodProvider.so"
#define LIBRARY_LIBCOCOS2DCPP "ux0:vitaarc/library/armeabi-v7a/libcocos2dcpp.so"
#define LIBRARY_LIBCRASHLYTICS "ux0:vitaarc/library/armeabi-v7a/libcrashlytics.so"
#define LIBRARY_LIBCRASHLYTICS_COMMON "ux0:vitaarc/library/armeabi-v7a/libcrashlytics-common.so"
#define LIBRARY_LIBCRASHLYTICS_HANDLER "ux0:vitaarc/library/armeabi-v7a/libcrashlytics-handler.so"
#define LIBRARY_LIBCRASHLYTICS_TRAMPOLINE "ux0:vitaarc/library/armeabi-v7a/libcrashlytics-trampoline.so"
#define LIBRARY_LIBFMOD "ux0:vitaarc/library/armeabi-v7a/libfmod.so"
#define LIBRARY_LIBFMOD_PROVIDER "ux0:vitaarc/library/armeabi-v7a/libfmodProvider.so"

#define PATH_TO_LOGFILE "ux0:vitaarc/boot.log"
#define PATH_TO_DATA "ux0:vitaarc/persistent/data/"
#define PATH_TO_OBB "ux0:vitaarc/persistent/obb/"

#define TAG ((__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__))

#define BREAKPOINT(x) \
  static int _trigger_count = 0;   \
  if (++_trigger_count == x)       \
  asm volatile("mov r0, #0; ldr r0, [r0];")

#endif /* _COMMON_DEFINE_H_ */
