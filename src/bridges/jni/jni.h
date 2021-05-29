#ifndef _BRIDGE_JNI_H_
#define _BRIDGE_JNI_H_

#include <stddef.h>
#include "../../solibrary/solib.h"

void bridgeCallJNIMain(HSOLIB hSoLibrary);

void bridgeJNICocosNativeInit(HSOLIB hSoLibrary,
                                      uint32_t nScreenWidth, uint32_t nScreenHeight);

void bridgeJNICocosInitApp(HSOLIB hSoLibrary);

void bridgeJNICocosSetDeviceId(HSOLIB hSoLibrary);

#endif /* _BRIDGE_JNI_H_ */
