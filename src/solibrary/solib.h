#ifndef _SOLIB_H_
#define _SOLIB_H_

#include <common/types.h>

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
 * Init so library
 *
 * @param hSoLibrary Library instance handle
 *
 */
void solibInitLibrary(HSOLIB hSoLibrary);

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
 * Install relocation address by symbol name
 *
 * @param hSoLibrary Library instance handle
 *
 * @param szSymbolName Symbol name to retrieve
 *
 * @param pfnDestProc Pointer to a proc address
 *
 * @return not 0 success
 */
void *solibInstallProc(HSOLIB hSoLibrary, const char *szSymbolName, void *pfnDestProc);

/**
 * Get library image base
 *
 * @param hSoLibrary Library instance handle
 *
 * @return library image base
 */
void *solibGetLibraryImageBase(HSOLIB hSoLibrary);

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
HSOLIB solibCloneHandle(HSOLIB hSoLibrary);

#endif /* _SOLIB_H_ */
