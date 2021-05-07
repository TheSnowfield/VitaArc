#ifndef _BRIDGE_ANDROIDNDK_IMPL_H_
#define _BRIDGE_ANDROIDNDK_IMPL_H_

#include <ctype.h>

void AAssetDir_close(void *assetDir) { return 0; }
void *AAssetDir_getNextFileName(void *assetDir) { return 0; }
void *AAssetManager_fromJava(void *env, void *assetManager) { return 0; }
void *AAssetManager_open(void *mgr, const char *filename, int mode) { return 0; }
void *AAssetManager_openDir(void *mgr, const char *dirName) { return 0; }
void AAsset_close(void *asset) { return 0; }
void AAsset_getLength(void *asset) { return 0; }
int AAsset_openFileDescriptor(void *asset, void *outStart, void *outLength) { return 0; }
int AAsset_read(void *asset, void *buf, size_t count) { return 0; }

#endif /* _BRIDGE_ANDROIDNDK_IMPL_H_ */
