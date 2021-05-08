#include <stdlib.h>
#include <string.h>
#include <utils/fs.h>
#include <utils/string.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/sysmem.h>

#include <common/elf.h>
#include <common/define.h>
#include <logcat/logcat.h>
#include <utils/debug.h>
#include "solib.h"

#define _H(x) ((LPSOINTERNAL)(*x));
#define MAX_LIBRARY 32

#define FAILED(x, ...)           \
  {                              \
    logF(TAG, x, ##__VA_ARGS__); \
    return NULL;                 \
  }

#define FAILED_INTERNAL(x, ...)  \
  {                              \
    logF(TAG, x, ##__VA_ARGS__); \
    goto ReleaseInternal;        \
  }

#define FAILED_MEMBLOCK(x, ...)  \
  {                              \
    logF(TAG, x, ##__VA_ARGS__); \
    goto ReleaseMemblock;        \
  }

static uint32_t libraryLoaded = 0;
static LPSOINTERNAL libraryInstances[MAX_LIBRARY] = {NULL};

HSOLIB solibLoadLibrary(const char *szLibrary)
{
  logV(TAG, "Loading library '%s'", szLibrary);

  // Check file exists
  if (!utilFileExists(szLibrary))
    FAILED("No such file or directory.");

  // Check file size
  uint32_t nfileSize = utilGetFileSize(szLibrary);
  if (nfileSize <= 0)
    FAILED("File is empty.");

  // Find library instance
  HSOLIB hSoLibrary = NULL;
  const char *szLibraryName = utilGetFileName(szLibrary);
  hSoLibrary = solibFindLibrary(szLibraryName);

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
        sceKernelAllocMemBlock("elf_image",
                               SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
                               (nfileSize + 0xFFF) & ~0xFFF,
                               NULL);

    // Check success
    if (lpInternal->sceImageMemBlock <= 0)
      FAILED_INTERNAL("Allocate memory block failed.");

    // Save the head of this memory block
    sceKernelGetMemBlockBase(lpInternal->sceImageMemBlock,
                             &lpInternal->lpLibraryImageBase);

    // Open file and read
    SceUID libFile = sceIoOpen(szLibrary, SCE_O_RDONLY, 0);
    sceIoRead(libFile, lpInternal->lpLibraryImageBase, nfileSize);
    sceIoClose(libFile);

    // Allocate a slot to store instance
    int32_t nSlotIndex = solibFindEmptySlot();
    if (nSlotIndex < 0)
      FAILED_MEMBLOCK("Library loaded too much!");

    // Setup pointers
    lpInternal->nSlotIndex = nSlotIndex;
    strcpy(lpInternal->szLibraryPath, szLibrary);
    strcpy(lpInternal->szLibraryName, szLibraryName);

    ++libraryLoaded;
    libraryInstances[nSlotIndex] = lpInternal;

    // Print slots information
    logI(TAG, "Library '%s' loaded.", szLibraryName);
    logI(TAG, "  Current slot: %d", nSlotIndex);
    logI(TAG, "  Available slots: %d", MAX_LIBRARY - libraryLoaded);

    // Process relocation
    solibRelocateVirtualAddress(lpInternal);
    logI(TAG, "All virtual addresses have been relocated.");

    // Print debug information
    solibDebugPrintElfTable(lpInternal);

    // Clone an userend handle
    return solibCloneHandleInternal(lpInternal);

  ReleaseMemblock:
    sceKernelFreeMemBlock(lpInternal->sceImageMemBlock);
  }

ReleaseInternal:
  free(lpInternal);

  return NULL;
}

void solibRelocateVirtualAddress(LPSOINTERNAL lpInternal)
{
}

void solibDebugPrintElfTable(LPSOINTERNAL lpInternal)
{
  logV(TAG, "Image Base: 0x%08X", lpInternal->lpLibraryImageBase);
  debugPrintMemoryBlock(lpInternal->lpLibraryImageBase, 16);

  uintptr_t lpImageBase = (uintptr_t)lpInternal->lpLibraryImageBase;
  Elf32_Ehdr *lpElfHeader = (Elf32_Ehdr *)lpImageBase;
  {
    logV(TAG, "ELF Magic: %s", lpElfHeader->e_ident);
    logV(TAG, "ELF Header Size: %d", lpElfHeader->e_ehsize);
    logV(TAG, "ELF Virtual Address Entry: 0x%08X", lpElfHeader->e_entry);
    logV(TAG, "ELF Flags: 0x%08X", lpElfHeader->e_flags);
    logV(TAG, "ELF Architecture: 0x%08X", lpElfHeader->e_machine);
    logV(TAG, "ELF Program Entry Size: %d", lpElfHeader->e_phentsize);
    logV(TAG, "ELF Program Entry Count: %d", lpElfHeader->e_phnum);
    logV(TAG, "ELF Program Entry Offset: 0x%08X", lpElfHeader->e_phoff);
    logV(TAG, "ELF Section Entry Size: %d", lpElfHeader->e_shentsize);
    logV(TAG, "ELF Section Entry Count: %d", lpElfHeader->e_shnum);
    logV(TAG, "ELF Section Entry Offset: 0x%08X", lpElfHeader->e_shoff);
    logV(TAG, "ELF Section String Index: %d", lpElfHeader->e_shstrndx);
    logV(TAG, "ELF Object File Type: 0x%08X", lpElfHeader->e_type);
  }

  Elf32_Phdr *lpElfProgramHeader =
      (Elf32_Phdr *)(lpImageBase + lpElfHeader->e_phoff);
  {
    logV(TAG, "ELF Program Header Base: 0x%08X", lpElfProgramHeader);
    logV(TAG, "    Alignment: %d", lpElfProgramHeader->p_align);
    logV(TAG, "    Size (File): %d", lpElfProgramHeader->p_filesz);
    logV(TAG, "    Flags: 0x%08X", lpElfProgramHeader->p_flags);
    logV(TAG, "    Size (Memory): %d", lpElfProgramHeader->p_memsz);
    logV(TAG, "    File Offset: 0x%08X", lpElfProgramHeader->p_offset);
    logV(TAG, "    Physical Address: 0x%08X", lpElfProgramHeader->p_paddr);
    logV(TAG, "    Type: 0x%08X", lpElfProgramHeader->p_type);
    logV(TAG, "    Virtual Address: 0x%08X", lpElfProgramHeader->p_vaddr);
  }

  Elf32_Shdr *lpElfSectionBase = lpImageBase + lpElfHeader->e_shoff;
  char *lpSectionStrTab = lpImageBase + lpElfSectionBase[lpElfHeader->e_shstrndx].sh_offset;

  // Print all section informations
  for (int i = 0; i < lpElfHeader->e_shnum; ++i)
  {
    logV(TAG, "Section %s [0x%08X => 0x%08X]",
         lpSectionStrTab + lpElfSectionBase[i].sh_name,
         lpElfSectionBase[i].sh_addr,
         lpImageBase + lpElfSectionBase[i].sh_addr);

    logV(TAG, "    Virtual Address Exec: 0x%08X", lpElfSectionBase[i].sh_addr);
    logV(TAG, "    Alignment: %d", lpElfSectionBase[i].sh_addralign);
    logV(TAG, "    Entry Size: %d", lpElfSectionBase[i].sh_entsize);
    logV(TAG, "    Flags: 0x%08X", lpElfSectionBase[i].sh_flags);
    logV(TAG, "    Section Info: 0x%08X", lpElfSectionBase[i].sh_info);
    logV(TAG, "    Another Section Link: 0x%08X", lpElfSectionBase[i].sh_link);
    logV(TAG, "    File Offset : 0x%08X", lpElfSectionBase[i].sh_offset);
    logV(TAG, "    Size In Bytes: %d", lpElfSectionBase[i].sh_size);
    logV(TAG, "    Type: 0x%08X", lpElfSectionBase[i].sh_type);
  }
}

void *solibGetProcAddress(HSOLIB hSoLibrary, const char *szFunctionName)
{
  return NULL;
}

void *solibGetSymbolStub(HSOLIB hSoLibrary, const char *szSymbolName, void *pfnDestProc)
{
  return NULL;
}

HSOLIB solibFindLibrary(const char *szLibraryName)
{
  for (int i = 0; i < MAX_LIBRARY; ++i)
  {
    if (libraryInstances[i] &&
        !strcmp(libraryInstances[i]->szLibraryName, szLibraryName))
    {
      logV(TAG, "Library found. Index: %d", i);
      return solibCloneHandleInternal(libraryInstances[i]);
    }
  }

  return NULL;
}

void solibFreeLibrary(HSOLIB hSoLibrary)
{
  LPSOINTERNAL lpInternal = _H(hSoLibrary);
  logV(TAG, "Free library %s", lpInternal->szLibraryName);

  // Sub reference count
  if (--(lpInternal->nRefCount) == 0)
    solibFreeLibrary(*hSoLibrary);

  // Destroy pointers
  libraryInstances[lpInternal->nSlotIndex] = NULL;
  --libraryLoaded;

  sceKernelFreeMemBlock(lpInternal->sceImageMemBlock);
  // free(lpInternal->szLibraryPath);
  // free(lpInternal->szLibraryName);
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

int32_t solibFindEmptySlot()
{
  if (libraryLoaded >= MAX_LIBRARY)
    return -1;

  for (int i = 0; i < MAX_LIBRARY; ++i)
    if (libraryInstances[i] == NULL)
    {
      return i;
    }

  return -2;
}
