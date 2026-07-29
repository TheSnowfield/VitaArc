#ifndef _LOADER_H_
#define _LOADER_H_

#include <types.h>
#include <elf.h>

typedef struct dynalib_t
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

} dynalib_t;

/**
 * Load so library
 *
 * @param szLibrary Path to library
 *
 * @return not 0 success
 */
dynalib_t *solibLoadLibrary(const char *szLibrary);

/**
 * Init so library
 *
 * @param hSoLibrary Library instance handle
 *
 */
void solibInitLibrary(dynalib_t *library);

/**
 * Free so library
 *
 * @param hSoLibrary Library instance handle
 *
 * @return void
 */
void solibFreeLibrary(dynalib_t *library);

/**
 * Get proc address by symbol name
 *
 * @param hSoLibrary Library instance handle
 *
 * @param szSymbolName Symbol name to retrieve
 *
 * @return not 0 success
 */
void *solibGetProcAddress(dynalib_t *library, const char *szSymbolName);

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
void *solibInstallProc(dynalib_t *library, const char *szSymbolName, uintptr_t pfnDestProc);

/**
 * Get library image base
 *
 * @param hSoLibrary Library instance handle
 *
 * @return library image base
 */
void *solibGetLibraryImageBase(dynalib_t *library);

/**
 * Find loaded library by name
 *
 * @param szLibraryName Library instance handle
 *
 * @return not 0 success
 */
dynalib_t *solibFindLibrary(const char *szLibraryName);

/**
 * Clone a handle
 *
 * @param hSoLibrary Library instance handle
 *
 * @return not 0 success
 */
dynalib_t *solibCloneHandle(dynalib_t *library);

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
bool solibLoadSections(dynalib_t *library);

/**
 * Print ELF fotmat information (Internal use)
 *
 * @param lpInternal Library base pointer
 *
 * @return not 0 success
 */
void solibDebugPrintElfTable(dynalib_t *library);


#endif /* _LOADER_H_ */
