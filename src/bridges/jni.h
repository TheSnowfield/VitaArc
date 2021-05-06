#ifndef _PATCH_JNI_H_
#define _PATCH_JNI_H_

#include <solibrary/solib.h>

void bridgePatchJNI(LPHSO lpSoLibrary);
void bridgeCallJNIMain(LPHSO lpSoLibrary);

#endif /* _PATCH_JNI_H_ */
