#ifndef _COMMON_DEFINES_H_
#define _COMMON_DEFINES_H_

#define LIBRARY_LIBFMOD "ux0:vitaarc/library/armeabi-v7a/libfmod.so"
#define LIBRARY_LIBFMODPROVIDER "ux0:vitaarc/library/armeabi-v7a/libfmodProvider.so"
#define LIBRARY_LIBCOCOS2DCPP "ux0:vitaarc/library/armeabi-v7a/libcocos2dcpp.so"

#define PATH_TO_LOGFILE "ux0:vitaarc/boot.log"

#define TAG ((__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__))

#endif /* _COMMON_DEFINES_H_ */
