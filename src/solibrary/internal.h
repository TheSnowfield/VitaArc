#ifndef _SOLIB_INTERNAL_H_
#define _SOLIB_INTERNAL_H_

#include <common/types.h>

typedef struct SOINTERNAL
{
  uint32_t nRefCount;
  uint32_t nSlotIndex;
  char szLibraryName[64];
  char szLibraryPath[128];
  void *lpLibraryImageBase;
  SceUID sceImageMemBlock;
  Elf32_Sym *lpElfSymSection;
  uint32_t nElfSymCount;
  void *lpElfDynStrSection;

} SOINTERNAL, *LPSOINTERNAL;

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
void solibLoadSections(LPSOINTERNAL lpInternal);

/**
 * Print ELF fotmat information (Internal use)
 *
 * @param lpInternal Library base pointer
 *
 * @return not 0 success
 */
void solibDebugPrintElfTable(LPSOINTERNAL lpInternal);

#endif /* _SOLIB_INTERNAL_H_ */
