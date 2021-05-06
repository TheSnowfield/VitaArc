#include <stdlib.h>
#include <string.h>
#include <utils/fs.h>
#include <utils/string.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/sysmem.h>

#include "solib.h"
#include <common/elf.h>

#define _H(x) ((LPSOINTERNAL)(*x));
#define TOTAL_LIBRARY 32

uint32_t libraryLoaded = 0;
LPSOINTERNAL libraryInstances[TOTAL_LIBRARY] = {NULL};

HSOLIB solibLoadLibrary(const char *szLibrary)
{
  // Check file exists
  if (!utilFileExists(szLibrary))
    return NULL;

  // Check file size
  uint32_t nfileSize = utilGetFileSize(szLibrary);
  if (nfileSize <= 0)
    return NULL;

  // Find library instance
  HSOLIB hSoLibrary = NULL;
  hSoLibrary = solibFindLibrary(utilGetFileName(szLibrary));

  // If this library has been loaded
  // just duplicate the handle and return
  if (hSoLibrary)
    return solibCloneHandle(hSoLibrary);

  // Prepare new block
  LPSOINTERNAL lpInternal = malloc(sizeof(LPSOINTERNAL));
  {
    // Allocate a memory block to
    // storage library image
    lpInternal->sceImageMemBlock =
        sceKernelAllocMemBlock("elf_image", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, nfileSize, NULL);

    // Check success
    if (lpInternal->sceImageMemBlock <= 0)
      goto ReleaseInternal;

    // Save the head of this memory block
    sceKernelGetMemBlockBase(lpInternal->sceImageMemBlock, &lpInternal->lpLibraryBase);

    // Open file and read
    SceUID libFile = sceIoOpen(szLibrary, SCE_O_RDONLY, 0777);
    sceIoRead(libFile, lpInternal->lpLibraryBase, nfileSize);
    sceIoClose(libFile);

    // Allocate a slot to store instance
    int32_t nSlotIndex = solibFindEmptySlot();
    if (nSlotIndex < 0)
      goto ReleaseMemblock;

    // Setup pointers
    lpInternal->nSlotIndex = nSlotIndex;
    lpInternal->szLibraryPath = malloc(strlen(szLibrary) + 1);
    strcpy(lpInternal->szLibraryPath, szLibrary);
    lpInternal->szLibraryName = malloc(strlen(utilGetFileName(szLibrary)) + 1);
    strcpy(lpInternal->szLibraryName, szLibrary);

    ++lpInternal->nRefCount;
    libraryInstances[nSlotIndex] = lpInternal;

    // Clone an userend handle
    return solibCloneHandleInternal(lpInternal);

  ReleaseMemblock:
    sceKernelFreeMemBlock(lpInternal->sceImageMemBlock);
  }

ReleaseInternal:
  free(lpInternal);

  return NULL;
}

void *solibGetProcAddress(HSOLIB hSoLibrary, const char *szFunctionName)
{
  return NULL;
}

void *solibInstallRelocation(HSOLIB hSoLibrary, const char *szSymbolName, void *pfnDestProc)
{
  return NULL;
}

HSOLIB solibFindLibrary(const char *szLibraryName)
{
  for (int i = 0; i < TOTAL_LIBRARY; ++i)
  {
    if (libraryInstances[i] &&
        !strcmp(libraryInstances[i]->szLibraryName, szLibraryName))
    {
      return solibCloneHandleInternal(libraryInstances[i]);
    }
  }

  return NULL;
}

void solibFreeLibrary(HSOLIB hSoLibrary)
{
  LPSOINTERNAL lpInternal = _H(hSoLibrary);

  // Sub reference count
  if (--(lpInternal->nRefCount) == 0)
    solibFreeLibrary(*hSoLibrary);

  // Destroy pointers
  libraryInstances[lpInternal->nSlotIndex] = NULL;
  --libraryLoaded;

  sceKernelFreeMemBlock(lpInternal->sceImageMemBlock);
  free(lpInternal->szLibraryPath);
  free(lpInternal->szLibraryName);
  free(hSoLibrary);
}

HSOLIB solibCloneHandle(HSOLIB hSoLibrary)
{
  LPSOINTERNAL lpInternal = _H(hSoLibrary);

  // Clone a pointer
  HSOLIB hSoLibraryNew = (HSOLIB)malloc(sizeof(HSOLIB));
  (*hSoLibraryNew) = lpInternal;

  // Add reference count
  ++(lpInternal->nRefCount);

  return hSoLibraryNew;
}

HSOLIB solibCloneHandleInternal(LPSOINTERNAL lpInternal)
{
  // Clone a pointer
  HSOLIB hSoLibraryNew = (HSOLIB)malloc(sizeof(HSOLIB));
  (*hSoLibraryNew) = lpInternal;

  // Add reference count
  ++(lpInternal->nRefCount);

  return hSoLibraryNew;
}

int solibFindEmptySlot()
{
  if (libraryLoaded >= TOTAL_LIBRARY)
    return -1;

  for (int i = 0; i < TOTAL_LIBRARY; ++i)
    if (libraryInstances[i] == NULL)
    {
      return i;
    }

  return -2;
}
