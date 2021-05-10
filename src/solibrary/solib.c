#include <stdlib.h>
#include <string.h>
#include <utils/fs.h>
#include <utils/string.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/modulemgr.h>

#include <common/elf.h>
#include <common/define.h>
#include <kubridge/kubridge.h>
#include <logcat/logcat.h>
#include <utils/kubridge.h>
#include <utils/debug.h>

#include "solib.h"
#include "internal.h"

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

#define MEMALIGN(x, align) (((x) + ((align)-1)) & ~((align)-1))

static uint32_t libraryLoaded = 0;
static LPSOINTERNAL librarySlots[MAX_LIBRARY] = {NULL};

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
    librarySlots[nSlotIndex] = lpInternal;

    // Print slots information
    logI(TAG, "Library '%s' loaded.", szLibraryName);
    logI(TAG, "  Current slot: %d", nSlotIndex);
    logI(TAG, "  Available slots: %d", MAX_LIBRARY - libraryLoaded);

    // Print debug information
    solibDebugPrintElfTable(lpInternal);

    // Process section
    solibLoadSections(lpInternal);
    logI(TAG, "Load and relocated all sections.");

    // Clone an userend handle
    return solibCloneHandleInternal(lpInternal);

  ReleaseMemblock:
    sceKernelFreeMemBlock(lpInternal->sceImageMemBlock);
  }

ReleaseInternal:
  free(lpInternal);

  return NULL;
}

void solibLoadSections(LPSOINTERNAL lpInternal)
{
  // Image base address
  uintptr_t lpImageBase = (uintptr_t)lpInternal->lpLibraryImageBase;

  // ELF format header
  Elf32_Ehdr *lpElfHeader = (Elf32_Ehdr *)lpImageBase;

  // ELF program base
  Elf32_Phdr *lpElfProgramBase = (Elf32_Phdr *)(lpImageBase + lpElfHeader->e_phoff);

  // ELF section base
  Elf32_Shdr *lpElfSectionBase = lpImageBase + lpElfHeader->e_shoff;

  uintptr_t lpLinearAddressBase = 0x98000000;
  uintptr_t lpLinearAddress = lpLinearAddressBase;

  // Load program segments
  for (int i = 0; i < lpElfHeader->e_phnum; ++i)
  {
    // If this section is loadable
    if (lpElfProgramBase[i].p_type == PT_LOAD)
    {
      // Calculate aligned block size
      uint32_t nBlockSize =
          MEMALIGN(lpElfProgramBase[i].p_memsz, lpElfProgramBase[i].p_align);

      SceUID nBlockID = NULL;
      SceKernelAllocMemBlockKernelOpt sAllocOption = {0};
      {
        sAllocOption.size = sizeof(SceKernelAllocMemBlockKernelOpt);
        sAllocOption.field_C = lpLinearAddress;
        sAllocOption.attr = 0x01;
      }

      logV(TAG, "Address 0x%08X, Size %d", lpLinearAddress, nBlockSize);

      // If this segment exectuable
      if (lpElfProgramBase[i].p_flags & PF_X)
      {
        nBlockID = kuKernelAllocMemBlock("elf_rx_block",
                                         SCE_KERNEL_MEMBLOCK_TYPE_USER_RX,
                                         nBlockSize, &sAllocOption);
      }
      else
      {
        nBlockID = kuKernelAllocMemBlock("elf_rw_block",
                                         SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
                                         nBlockSize, &sAllocOption);
      }

      logV(TAG, "nBlockID %d", nBlockID);

      if (nBlockID <= 0)
        return false;

      logV(TAG, "Allocated %d bytes at [0x%08X] for segment.",
           nBlockSize, lpElfProgramBase[i].p_vaddr);

      // Update section data
      void *lpBlockData = NULL;
      sceKernelGetMemBlockBase(nBlockID, &lpBlockData);
      lpElfProgramBase[i].p_vaddr += lpLinearAddressBase;

      // Copy data
      kuKernelCpuUnrestrictedMemset(lpBlockData, 0x00, nBlockSize);
      kuKernelCpuUnrestrictedMemcpy(lpElfProgramBase[i].p_vaddr,
                                    lpImageBase + lpElfProgramBase[i].p_offset,
                                    lpElfProgramBase[i].p_filesz);

      // Next segment
      lpLinearAddress += nBlockSize;

      logV(TAG, "Load segment: [0x%08X]. Length %d.",
           lpElfProgramBase[i].p_vaddr, lpElfProgramBase[i].p_filesz);

      debugPrintMemoryBlock(lpElfProgramBase[i].p_vaddr, 16, 16);
    }
  }

  logV(TAG, "Segments load finished.");

  // Find dynsym and dynstr
  Elf32_Sym *lpSectionSyms = NULL;
  uint32_t nSymsCount = 0;
  char *lpSectionDynStrTab = NULL;
  char *lpSectionStrTab = lpImageBase + lpElfSectionBase[lpElfHeader->e_shstrndx].sh_offset;

  for (int i = 0; i < lpElfHeader->e_shnum; ++i)
  {
    char *lpszSectionName = lpSectionStrTab + lpElfSectionBase[i].sh_name;

    // Find .dynsym section
    if (!lpSectionSyms && strcmp(lpszSectionName, ".dynsym") == 0)
    {
      nSymsCount = lpElfSectionBase[i].sh_size / sizeof(Elf32_Sym);
      lpSectionSyms = lpImageBase + lpElfSectionBase[i].sh_offset;
    }

    // Find .dynstr section
    if (!lpSectionDynStrTab && strcmp(lpszSectionName, ".dynstr") == 0)
      lpSectionDynStrTab = lpImageBase + lpElfSectionBase[i].sh_offset;
  }

  logV(TAG, ".dynsym: [0x%08X], .dynstr: [0x%08X]", lpSectionSyms, lpSectionDynStrTab);

  // If one of section not found
  if (!(lpSectionSyms && lpSectionDynStrTab))
    return false;

  for (int i = 0; i < lpElfHeader->e_shnum; ++i)
  {
    char *lpszSectionName =
        lpSectionStrTab + lpElfSectionBase[i].sh_name;

    if ((strcmp(lpszSectionName, ".rel.dyn") == 0) ||
        (strcmp(lpszSectionName, ".rel.plt") == 0))
    {
      // Process section relocation
      for (int j = 0; j < lpElfSectionBase[i].sh_size / sizeof(Elf32_Rel); ++j)
      {
        Elf32_Rel *lpSectionBase = lpImageBase + lpElfSectionBase[i].sh_offset;
        Elf32_Sym *lpRelocateInfo = &lpSectionSyms[ELF32_R_SYM(lpSectionBase[j].r_info)];
        uintptr_t *lpRelocateAddress = lpLinearAddressBase + lpSectionBase[j].r_offset;
        uint32_t nRelocateType = ELF32_R_TYPE(lpSectionBase[j].r_info);

        logV(TAG, "Relocating symbol: %s => [0x%08X], 0x%08X, %d, %d",
             lpSectionDynStrTab + lpRelocateInfo->st_name,
             *lpRelocateAddress, lpRelocateInfo->st_value,
             nRelocateType, lpRelocateInfo->st_shndx);

        switch (nRelocateType)
        {
        case R_ARM_ABS32:
          (*lpRelocateAddress) += lpRelocateInfo->st_value;
          break;

        case R_ARM_RELATIVE:
          (*lpRelocateAddress) += lpLinearAddressBase;
          break;

        case R_ARM_GLOB_DAT:
        case R_ARM_JUMP_SLOT:
          (*lpRelocateAddress) = lpLinearAddressBase + lpRelocateInfo->st_value;
          break;

        default:
          logE(TAG, "Unknown relocate type reached. %d", nRelocateType);
          // return false;
        }

        logV(TAG, "Relocated to [0x%08X]\n", (*lpRelocateAddress));
      }
    }
  }

  // Save cache
  lpInternal->nElfSymCount = nSymsCount;
  lpInternal->lpElfSymSection = lpSectionSyms;
  lpInternal->lpElfDynStrSection = lpSectionDynStrTab;

  logI(TAG, "Relocated all symbols.");

  return true;
}

void solibDebugPrintElfTable(LPSOINTERNAL lpInternal)
{
  logV(TAG, "Image Base: 0x%08X", lpInternal->lpLibraryImageBase);
  debugPrintMemoryBlock(lpInternal->lpLibraryImageBase, 16, 16);

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

  Elf32_Phdr *lpElfProgramBase = lpImageBase + lpElfHeader->e_phoff;

  for (int i = 0; i < lpElfHeader->e_phnum; ++i)
  {
    logV(TAG, "Segment [0x%08X => 0x%08X]",
         lpElfProgramBase[i].p_vaddr,
         lpImageBase + lpElfProgramBase[i].p_vaddr);

    logV(TAG, "    Type: 0x%08X", lpElfProgramBase[i].p_type);
    logV(TAG, "    Alignment: %d", lpElfProgramBase[i].p_align);
    logV(TAG, "    Size (File): %d", lpElfProgramBase[i].p_filesz);
    logV(TAG, "    Size (Memory): %d", lpElfProgramBase[i].p_memsz);
    logV(TAG, "    Flags: 0x%08X", lpElfProgramBase[i].p_flags);
    logV(TAG, "    Data Offset: 0x%08X", lpElfProgramBase[i].p_offset);
    logV(TAG, "    Virtual Address: 0x%08X", lpElfProgramBase[i].p_vaddr);
    logV(TAG, "    Physical Address: 0x%08X", lpElfProgramBase[i].p_paddr);
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
  if (!hSoLibrary)
    return NULL;

  LPSOINTERNAL lpInternal = _H(hSoLibrary);

  // Enumerate symbol table
  for (int i = 0; i < lpInternal->nElfSymCount; ++i)
  {
    char *lpszSymbolName =
        lpInternal->lpElfDynStrSection +
        lpInternal->lpElfSymSection[i].st_name;

    // Compare symbol name
    if (strcmp(lpszSymbolName, szFunctionName) == 0)
    {
      logV(TAG, "ProcAddress found: %s [0x%08X]", szFunctionName,
           0x98000000 + lpInternal->lpElfSymSection[i].st_value - 1);

      return 0x98000000 + lpInternal->lpElfSymSection[i].st_value - 1;
    }
  }

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
    if (librarySlots[i] &&
        !strcmp(librarySlots[i]->szLibraryName, szLibraryName))
    {
      logV(TAG, "Library found. Index: %d", i);
      return solibCloneHandleInternal(librarySlots[i]);
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
  librarySlots[lpInternal->nSlotIndex] = NULL;
  --libraryLoaded;

  sceKernelFreeMemBlock(lpInternal->sceImageMemBlock);
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
    if (librarySlots[i] == NULL)
    {
      return i;
    }

  return -2;
}
