#include <common/define.h>
#include <utils/patcher.h>
#include <logcat/logcat.h>

#include "ndk.h"
#include "ndkimpl.h"

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
