#ifndef _BRIDGE_ANDROID_ASSETMGRIMPL_H_
#define _BRIDGE_ANDROID_ASSETMGRIMPL_H_

#include <stddef.h>

void AAssetDir_close(void *assetDir);

void *AAssetDir_getNextFileName(void *assetDir);

void *AAssetManager_fromJava(void *env, void *assetManager);

void *AAssetManager_open(void *mgr, const char *filename, int mode);

void *AAssetManager_openDir(void *mgr, const char *dirName);

void AAsset_close(void *asset);

void AAsset_getLength(void *asset);

int AAsset_openFileDescriptor(void *asset, void *outStart, void *outLength);

int AAsset_read(void *asset, void *buf, size_t count);

#endif /* _BRIDGE_ANDROID_ASSETMGRIMPL_H_ */

