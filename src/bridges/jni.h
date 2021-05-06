#ifndef _BRIDGE_JNI_H_
#define _BRIDGE_JNI_H_

#include <solibrary/solib.h>

void bridgePatchJNI(HSOLIB lpSoLibrary);
void bridgeCallJNIMain(HSOLIB lpSoLibrary);

#endif /* _BRIDGE_JNI_H_ */
