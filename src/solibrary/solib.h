#ifndef _SOLIB_H_
#define _SOLIB_H_

#include <psp2/types.h>

typedef struct HSO
{
  uint32_t nRefCount;
  void *lpLibrary;
} HSO, *LPHSO;

/**
 * Load so library
 *
 * @param szLibrary Path to library
 *
 * @return not 0 success
 */
LPHSO solibLoadLibrary(char *szLibrary);

/**
 * Free so library
 *
 * @param lpSoLibrary Pointer to library
 *
 * @return void
 */
void solibFreeLibrary(LPHSO lpSoLibrary);

/**
 * Get proc address by symbol name
 *
 * @param lpSoLibrary Pointer to library
 *
 * @param szSymbolName Symbol name to retrieve
 *
 * @return not 0 success
 */
void *solibGetProcAddress(LPHSO lpSoLibrary, char *szSymbolName);

/**
 * Set relocation address by symbol name
 *
 * @param lpSoLibrary Pointer to library
 *
 * @param szSymbolName Symbol name to retrieve
 *
 * @param pfnDestProc Pointer to a proc address
 *
 * @return not 0 success
 */
void *solibInstallRelocation(LPHSO lpSoLibrary, char *szSymbolName, void *pfnDestProc);

#endif /* _SOLIB_H_ */
