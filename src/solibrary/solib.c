#include <stdlib.h>
#include <utils/fs.h>
#include <utils/string.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/sysmem.h>

#include "solib.h"

#define _H(x) ((LPSOINTERNAL)(*x));

const uint32_t libraryLoaded = 0;
const LPSOINTERNAL libraryInstances[32] = {NULL};

HSOLIB solibLoadLibrary(const char *szLibrary)
{
  // Check file exists
  if (!utilFileExists(szLibrary))
    return NULL;

  // Check file size
  uint32_t nfileSize = utilGetFileSize(szLibrary);
  if (nfileSize <= 0)
    return NULL;

  // Open file
  SceUID nfile = sceIoOpen(szLibrary, SCE_O_RDONLY, 0777);
  if (nfile <= 0)
    return NULL;

  // Find library instance
  HSOLIB hSoLibrary = NULL;
  hSoLibrary = solibFindLibrary(utilGetFileName(szLibrary));

  // If this library has been loaded
  // Duplicate handle and return
  if (hSoLibrary)
    return solidCloneHandle(hSoLibrary);

  // Prepare new block
  // = malloc(sizeof(HSO));
  // {

  //   sceKernelAllocMemBlock("ELF", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, nfileSize, NULL);

  //   hSoLibrary->sceImageMemBlock;
  // }

  return hSoLibrary;
}

void *solibGetProcAddress(HSOLIB hSoLibrary, const char *szFunctionName)
{
}

void *solibInstallRelocation(HSOLIB hSoLibrary, const char *szSymbolName, void *pfnDestProc)
{
}

HSOLIB solibFindLibrary(const char *szLibraryName)
{
}

void solibFreeLibrary(HSOLIB hSoLibrary)
{
  LPSOINTERNAL lpInternal = _H(hSoLibrary);

  // Sub reference count
  if (--(lpInternal->nRefCount) == 0)
    solibFreeLibrary(*hSoLibrary);

  // Destroy pointer
  free(hSoLibrary);
}

HSOLIB solidCloneHandle(HSOLIB hSoLibrary)
{
  LPSOINTERNAL lpInternal = _H(hSoLibrary);

  // Clone a pointer
  HSOLIB hSoLibraryNew = (HSOLIB)malloc(sizeof(HSOLIB));
  (*hSoLibraryNew) = lpInternal;

  // Add reference count
  ++(lpInternal->nRefCount);
}

HSOLIB solidCloneHandleInternal(LPSOINTERNAL lpInternal)
{
  // Clone a pointer
  HSOLIB hSoLibrary = (HSOLIB)malloc(sizeof(HSOLIB));
  (*hSoLibrary) = lpInternal;

  // Add reference count
  ++(lpInternal->nRefCount);
}
