#ifndef _BRIDGE_JNI_H_
#define _BRIDGE_JNI_H_

#include <stddef.h>
#include <loader.h>

void bridgeCallJNIMain(dynalib_t *hSoLibrary);

void bridgeJNICocosNativeInit(dynalib_t *hSoLibrary,
                                      uint32_t nScreenWidth, uint32_t nScreenHeight);

void bridgeJNICocosInitApp(dynalib_t *hSoLibrary);

void bridgeJNICocosSetDeviceId(dynalib_t *hSoLibrary);

#endif /* _BRIDGE_JNI_H_ */
