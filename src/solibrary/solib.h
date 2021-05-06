#ifndef _SOLIB_H_
#define _SOLIB_H_

#include <common/types.h>

typedef struct SOINTERNAL
{
  uint32_t nRefCount;
  char *szLibraryPath;
  void *lpLibraryBase;
  SceUID sceImageMemBlock;

} SOINTERNAL, *LPSOINTERNAL;

typedef void **HSOLIB;

/**
 * Load so library
 *
 * @param szLibrary Path to library
 *
 * @return not 0 success
 */
HSOLIB solibLoadLibrary(const char *szLibrary);

/**
 * Free so library
 *
 * @param hSoLibrary Library instance handle
 *
 * @return void
 */
void solibFreeLibrary(HSOLIB hSoLibrary);

/**
 * Get proc address by symbol name
 *
 * @param hSoLibrary Library instance handle
 *
 * @param szSymbolName Symbol name to retrieve
 *
 * @return not 0 success
 */
void *solibGetProcAddress(HSOLIB hSoLibrary, const char *szSymbolName);

/**
 * Set relocation address by symbol name
 *
 * @param hSoLibrary Library instance handle
 *
 * @param szSymbolName Symbol name to retrieve
 *
 * @param pfnDestProc Pointer to a proc address
 *
 * @return not 0 success
 */
void *solibInstallRelocation(HSOLIB hSoLibrary, const char *szSymbolName, void *pfnDestProc);

/**
 * Find loaded library by name
 *
 * @param szLibraryName Library instance handle
 *
 * @return not 0 success
 */
HSOLIB solibFindLibrary(const char *szLibraryName);

/**
 * Clone a handle
 *
 * @param hSoLibrary Library instance handle
 *
 * @return not 0 success
 */
HSOLIB solidCloneHandle(HSOLIB hSoLibrary);

/**
 * Clone a handle (Internal use)
 *
 * @param lpInternal Library base pointer
 *
 * @return not 0 success
 */
HSOLIB solidCloneHandleInternal(LPSOINTERNAL lpInternal);

#endif /* _SOLIB_H_ */
