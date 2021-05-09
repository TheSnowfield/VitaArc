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
  void* lpElfSectionBase;
  void* lpElfProgramBase;

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

/**
 * Loa kubridge.kuprx (Internal use)
 *
 * @return void
 */
void solibLoadKuBridge(void);

#endif /* _SOLIB_INTERNAL_H_ */
