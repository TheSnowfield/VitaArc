#include <common/define.h>
#include <utils/patcher.h>
#include <logcat/logcat.h>

#include "androidndk.h"

void AAssetDir_close(void *assetDir) { return 0; }
void *AAssetDir_getNextFileName(void *assetDir) { return 0; }
void *AAssetManager_fromJava(void *env, void *assetManager) { return 0; }
void *AAssetManager_open(void *mgr, const char *filename, int mode) { return 0; }
void *AAssetManager_openDir(void *mgr, const char *dirName) { return 0; }
void AAsset_close(void *asset) { return 0; }
void AAsset_getLength(void *asset) { return 0; }
int AAsset_openFileDescriptor(void *asset, void *outStart, void *outLength) { return 0; }
int AAsset_read(void *asset, void *buf, size_t count) { return 0; }

static const BRIDGEFUNC BRIDGE_ANDROIDNDK[] =
{
  {"AAssetDir_close", (uintptr_t)&AAssetDir_close},
  {"AAssetDir_getNextFileName", (uintptr_t)&AAssetDir_getNextFileName},
  {"AAssetManager_fromJava", (uintptr_t)&AAssetManager_fromJava},
  {"AAssetManager_open", (uintptr_t)&AAssetManager_open},
  {"AAssetDir_close", (uintptr_t)&AAssetDir_close},
  {"AAssetManager_openDir", (uintptr_t)&AAssetManager_openDir},
  {"AAsset_close", (uintptr_t)&AAsset_close},
  {"AAsset_getLength", (uintptr_t)&AAsset_getLength},
  {"AAsset_openFileDescriptor", (uintptr_t)&AAsset_openFileDescriptor},
  {"AAsset_read", (uintptr_t)&AAsset_read}
};

void bridgePatchAndroidNDK(HSOLIB hSoLibrary)
{
  patchSymbols(hSoLibrary, BRIDGE_ANDROIDNDK,
               sizeof(BRIDGE_ANDROIDNDK) / sizeof(BRIDGEFUNC));
}
