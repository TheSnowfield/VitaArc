#ifndef _SOLIB_H_
#define _SOLIB_H_

#include <types.h>
#include <elf.h>

typedef void **HSOLIB;

typedef struct SOINTERNAL
{
  uint32_t nRefCount;
  uint32_t nSlotIndex;
  char szLibraryName[64];
  char szLibraryPath[128];
  void *lpLibraryImageBase;
  SceUID sceImageMemBlock;

  Elf32_Ehdr *lpElfHeader;
  Elf32_Phdr *lpElfSegmentBase;
  Elf32_Shdr *lpElfSectionBase;
  Elf32_Shdr *lpElfSectionSymbol;
  Elf32_Shdr *lpElfSectionInitArray;

  void *lpElfSecStrTab;
  Elf32_Sym *lpElfDynSymbols;
  void *lpElfDynStrTab;
  uint32_t nElfDynSymbolCount;

  void *lpTextBase;

} SOINTERNAL, *LPSOINTERNAL;

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
void *solibInstallProc(HSOLIB hSoLibrary, const char *szSymbolName, uintptr_t pfnDestProc);

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


/**
 * Clone a handle (Internal use)
 *
 * @param lpInternal Library base pointer
 *
 * @return not 0 success
 */
HSOLIB solibCloneHandleInternal(LPSOINTERNAL lpInternal);

/**
 * Find an empty library slot (Internal use)
 *
 * @return >0 success
 */
int32_t solibFindEmptySlot();

/**
 * Relocate virtual address (Internal use)
 *
 * @param lpInternal Library base pointer
 *
 * @return not 0 success
 */
bool solibLoadSections(LPSOINTERNAL lpInternal);

/**
 * Print ELF fotmat information (Internal use)
 *
 * @param lpInternal Library base pointer
 *
 * @return not 0 success
 */
void solibDebugPrintElfTable(LPSOINTERNAL lpInternal);


#endif /* _SOLIB_H_ */
