#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <utils/fs.h>
#include <utils/string.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/modulemgr.h>

#include <elf.h>
#include <config.h>
#include <kubridge/kubridge.h>
#include <logcat/logcat.h>
#include <patcher.h>
#include <utils/debug.h>

#include <generated/bridge_symbols.h>

#include "loader.h"
#include "kubridge.h"

#define MAX_LIBRARY 32

#ifndef SCE_KERNEL_MEMBLOCK_TYPE_USER_RX
#define SCE_KERNEL_MEMBLOCK_TYPE_USER_RX 0x0C20D050
#endif

// Symbol Values
// In addition to the normal rules for symbol values
// the following rules shall also apply to symbols of type STT_FUNC:

// If the symbol addresses an Arm instruction,
// its value is the address of the instruction
// (in a relocatable object, the offset of the instruction
// from the start of the section containing it).

// If the symbol addresses a Thumb instruction,
// its value is the address of the instruction with bit zero set
// (in a relocatable object, the section offset with bit zero set).

// For the purposes of relocation the value used shall be the address
// of the instruction (st_value & ~1).
// https://developer.arm.com/documentation/ihi0044/latest/

// #define OFFRST(x) (x & ~1)
#define OFFRST(x) (x)

#define FAILED(x, ...)           \
  {                              \
    log_f(TAG, x, ##__VA_ARGS__); \
    return NULL;                 \
  }

#define FAILED_INTERNAL(x, ...)  \
  {                              \
    log_f(TAG, x, ##__VA_ARGS__); \
    goto release_internal;       \
  }

#define FAILED_MEMBLOCK(x, ...)  \
  {                              \
    log_f(TAG, x, ##__VA_ARGS__); \
    goto release_memblock;       \
  }

#define MEMALIGN(x, align) (((x) + ((align)-1)) & ~((align)-1))

static uint32_t __library_loaded = 0;
static dynalib_t *__library_slots[MAX_LIBRARY] = {NULL};

static const patch_func_t *__find_bridge_symbol(
    const patch_func_t bridge_functions[], uint32_t bridge_count,
    const char *symbol_name)
{
  uint32_t left = 0;
  uint32_t right = bridge_count;

  while (left < right)
  {
    uint32_t middle = left + (right - left) / 2;
    int result =
        strcmp(symbol_name, bridge_functions[middle].symbol_name);

    if (result == 0)
      return &bridge_functions[middle];
    if (result < 0)
      right = middle;
    else
      left = middle + 1;
  }

  return NULL;
}

static void __resolve_symbols(dynalib_t *library,
                              const patch_func_t bridge_functions[],
                              uint32_t bridge_count)
{
  if (!library || !bridge_functions || !bridge_count)
    return;

  uint32_t resolved_relocations = 0;
  for (uint32_t i = 0; i < library->elf_header->e_shnum; ++i)
  {
    const char *section_name =
        (const char *)library->elf_section_string_table +
        library->elf_section_base[i].sh_name;

    if (strcmp(section_name, ".rel.dyn") != 0 &&
        strcmp(section_name, ".rel.plt") != 0)
      continue;

    Elf32_Rel *relocations =
        (Elf32_Rel *)((uintptr_t)library->library_image_base +
                      library->elf_section_base[i].sh_addr);
    uint32_t relocation_count =
        library->elf_section_base[i].sh_size / sizeof(*relocations);

    for (uint32_t j = 0; j < relocation_count; ++j)
    {
      uint32_t symbol_index = ELF32_R_SYM(relocations[j].r_info);
      if (symbol_index >= library->elf_dynamic_symbol_count)
        continue;

      Elf32_Sym *symbol = &library->elf_dynamic_symbols[symbol_index];
      if (symbol->st_shndx != SHN_UNDEF)
        continue;

      const char *symbol_name =
          (const char *)library->elf_dynamic_string_table + symbol->st_name;
      const patch_func_t *bridge =
          __find_bridge_symbol(bridge_functions, bridge_count, symbol_name);
      if (!bridge)
        continue;

      uintptr_t *relocation_address =
          (uintptr_t *)(uintptr_t)(0x98000000 + relocations[j].r_offset);
      *relocation_address = bridge->bridge_proc;
      ++resolved_relocations;
    }
  }

  log_i(TAG, "Resolved %u relocations from %u local symbols.",
        resolved_relocations, bridge_count);
}

void loader_symbol_ref(dynalib_t *library)
{
  __resolve_symbols(
      library, __bridge_symbols,
      sizeof(__bridge_symbols) / sizeof(__bridge_symbols[0]));
}

dynalib_t *loader_load_library(const char *library_path)
{
  log_v(TAG, "Loading library '%s'", library_path);

  // Check file exists
  if (!util_file_exists(library_path))
    FAILED("No such file or directory.");

  // Check file size
  uint32_t file_size = util_get_file_size(library_path);
  if (file_size <= 0)
    FAILED("File is empty.");

  // Find library instance
  dynalib_t *library = NULL;
  const char *library_name = util_get_file_name(library_path);
  library = loader_find_library(library_name);

  // If this library has been loaded
  // just duplicate the handle and return
  if (library)
  {
    loader_symbol_ref(library);
    return loader_clone_handle(library);
  }

  // Prepare new block
  dynalib_t *new_library = calloc(1, sizeof(*new_library));
  if (!new_library)
    FAILED("Allocate library state failed.");
  {
    // Allocate a memory block to
    // storage library image
    new_library->image_mem_block =
        sceKernelAllocMemBlock("elf_image",
                               SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
                               (file_size + 0xFFF) & ~0xFFF,
                               NULL);

    // Check success
    if (new_library->image_mem_block <= 0)
      FAILED_INTERNAL("Allocate memory block failed.");

    // Save the head of this memory block
    sceKernelGetMemBlockBase(new_library->image_mem_block,
                             &new_library->library_image_base);

    // Open file and read
    SceUID library_file = sceIoOpen(library_path, SCE_O_RDONLY, 0);
    sceIoRead(library_file, new_library->library_image_base, file_size);
    sceIoClose(library_file);

    // Allocate a slot to store instance
    int32_t slot_index = loader_find_empty_slot();
    if (slot_index < 0)
      FAILED_MEMBLOCK("Library loaded too much!");

    // Setup pointers
    new_library->slot_index = slot_index;
    strcpy(new_library->library_path, library_path);
    strcpy(new_library->library_name, library_name);

    ++__library_loaded;
    __library_slots[slot_index] = new_library;

    // Print slots information
    log_i(TAG, "Library '%s' loaded.", library_name);
    log_i(TAG, "  Current slot: %d", slot_index);
    log_i(TAG, "  Available slots: %d", MAX_LIBRARY - __library_loaded);

    // Print debug information
    loader_debug_print_elf_table(new_library);

    // Process section
    if (!loader_load_sections(new_library))
    {
      __library_slots[slot_index] = NULL;
      --__library_loaded;
      FAILED_MEMBLOCK("Load or relocation failed.");
    }
    log_i(TAG, "Load and relocated all sections.");

    // Resolve imported dependencies against the generated local symbol table.
    loader_symbol_ref(new_library);

    // Clone an userend handle
    return loader_clone_handle(new_library);

  release_memblock:
    sceKernelFreeMemBlock(new_library->image_mem_block);
  }

release_internal:
  free(new_library);

  return NULL;
}

bool loader_load_sections(dynalib_t *library)
{
  dynalib_t *internal = library;
  // Image base address
  uintptr_t image_base = (uintptr_t)internal->library_image_base;

  // ELF format header
  Elf32_Ehdr *elf_header = (Elf32_Ehdr *)image_base;

  // ELF program base
  Elf32_Phdr *elf_program_base =
      (Elf32_Phdr *)(image_base + elf_header->e_phoff);

  // ELF section base
  Elf32_Shdr *elf_section_base =
      (Elf32_Shdr *)(image_base + elf_header->e_shoff);

  uintptr_t linear_address_base = 0x98000000;
  uintptr_t linear_address = linear_address_base;
  uintptr_t last_block_end = 0;

  // Load program segments
  for (int i = 0; i < elf_header->e_phnum; ++i)
  {
    // If this section is loadable
    if (elf_program_base[i].p_type == PT_LOAD)
    {

      uint32_t block_gap_size = 0;

      // Becareful the gap
      if (last_block_end)
      {
        uintptr_t current_block_start =
            linear_address_base + elf_program_base[i].p_vaddr;

        block_gap_size = current_block_start - last_block_end;

        log_v(TAG, "Block gap size %d", block_gap_size);
      }

      // Calculate aligned block size
      uint32_t block_size =
          MEMALIGN(elf_program_base[i].p_memsz + block_gap_size,
                   elf_program_base[i].p_align);

      SceUID block_id = -1;
      SceKernelAllocMemBlockKernelOpt alloc_options = {0};
      {
        alloc_options.size = sizeof(SceKernelAllocMemBlockKernelOpt);
        alloc_options.field_C = linear_address;
        alloc_options.attr = 0x01;
      }

      log_v(TAG, "Address 0x%08X, Size %d", linear_address, block_size);

      // If this segment exectuable
      if (elf_program_base[i].p_flags & PF_X)
      {
        block_id = kuKernelAllocMemBlock("elf_rx_block",
                                         SCE_KERNEL_MEMBLOCK_TYPE_USER_RX,
                                         block_size, &alloc_options);
      }
      else
      {
        block_id = kuKernelAllocMemBlock("elf_rw_block",
                                         SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
                                         block_size, &alloc_options);
      }

      log_v(TAG, "block_id %d", block_id);

      if (block_id <= 0)
        return false;

      log_v(TAG, "Allocated %d bytes at [0x%08X] for segment.",
           block_size, elf_program_base[i].p_vaddr);

      // Update section data
      void *block_data = NULL;
      sceKernelGetMemBlockBase(block_id, &block_data);
      elf_program_base[i].p_vaddr += linear_address_base;

      // Save text base
      if (elf_program_base[i].p_flags & PF_X)
      {
        internal->text_base =
            (void *)(uintptr_t)elf_program_base[i].p_vaddr;
        kuKernelFlushCaches(internal->text_base,
                            elf_program_base[i].p_memsz);
      }

      // Copy data
      kuKernelCpuUnrestrictedMemset(block_data, 0x00, block_size);
      kuKernelCpuUnrestrictedMemcpy(
          (void *)(uintptr_t)elf_program_base[i].p_vaddr,
          (const void *)(image_base + elf_program_base[i].p_offset),
          elf_program_base[i].p_filesz);

      // Next segment
      linear_address += block_size;
      last_block_end = elf_program_base[i].p_vaddr + block_size;

      log_v(TAG, "Load segment: [0x%08X]. Length %d.",
           elf_program_base[i].p_vaddr, elf_program_base[i].p_filesz);

      debug_print_memory_block(
          (void *)(uintptr_t)elf_program_base[i].p_vaddr, 16, 16);
    }
  }

  log_v(TAG, "Segments load finished.");

  // Find dynsym and dynstr
  Elf32_Sym *dynamic_symbols = NULL;
  uint32_t dynamic_symbol_count = 0;
  char *dynamic_string_table = NULL;
  char *section_string_table =
      (char *)(image_base +
               elf_section_base[elf_header->e_shstrndx].sh_offset);
  Elf32_Shdr *init_array_section = NULL;

  for (int i = 0; i < elf_header->e_shnum; ++i)
  {
    char *section_name =
        section_string_table + elf_section_base[i].sh_name;

    // Find .dynsym section
    if (!dynamic_symbols && strcmp(section_name, ".dynsym") == 0)
    {
      dynamic_symbol_count =
          elf_section_base[i].sh_size / sizeof(Elf32_Sym);
      dynamic_symbols =
          (Elf32_Sym *)(image_base + elf_section_base[i].sh_offset);
    }

    // Find .dynstr section
    if (!dynamic_string_table && strcmp(section_name, ".dynstr") == 0)
      dynamic_string_table =
          (char *)(image_base + elf_section_base[i].sh_offset);

    // Find .init_array section
    if (!init_array_section && strcmp(section_name, ".init_array") == 0)
      init_array_section = &elf_section_base[i];
  }

  log_v(TAG, ".dynsym: [0x%08X], .dynstr: [0x%08X]",
       dynamic_symbols, dynamic_string_table);

  // If one of section not found
  if (!(dynamic_symbols && dynamic_string_table))
    return false;

  for (int i = 0; i < elf_header->e_shnum; ++i)
  {
    char *section_name =
        section_string_table + elf_section_base[i].sh_name;

    if ((strcmp(section_name, ".rel.dyn") == 0) ||
        (strcmp(section_name, ".rel.plt") == 0))
    {
      Elf32_Rel *relocation_section =
          (Elf32_Rel *)(image_base + elf_section_base[i].sh_addr);

      // Process section relocation
      for (int j = 0;
           j < elf_section_base[i].sh_size / sizeof(Elf32_Rel); ++j)
      {
        Elf32_Sym *relocation_symbol =
            &dynamic_symbols[ELF32_R_SYM(relocation_section[j].r_info)];
        uintptr_t *relocation_address =
            (uintptr_t *)(linear_address_base +
                          relocation_section[j].r_offset);
        uint32_t relocation_type =
            ELF32_R_TYPE(relocation_section[j].r_info);
        uintptr_t symbol_address = 0;

        if (relocation_type == R_ARM_ABS32 ||
            relocation_type == R_ARM_GLOB_DAT ||
            relocation_type == R_ARM_JUMP_SLOT)
        {
          if (relocation_symbol->st_shndx != SHN_UNDEF)
            symbol_address =
                linear_address_base +
                OFFRST(relocation_symbol->st_value);
        }

        // log_v(TAG, "Relocating symbol: %s => [0x%08X], 0x%08X, %d, %d",
        //      dynamic_string_table + relocation_symbol->st_name,
        //      *relocation_address, OFFRST(relocation_symbol->st_value),
        //      relocation_type, relocation_symbol->st_shndx);

        switch (relocation_type)
        {
        case R_ARM_ABS32:
          (*relocation_address) += symbol_address;
          break;

        case R_ARM_RELATIVE:
          (*relocation_address) =
              OFFRST(*relocation_address) + linear_address_base;
          break;

        case R_ARM_GLOB_DAT:
        case R_ARM_JUMP_SLOT:
          (*relocation_address) = symbol_address;
          break;

        default:
          log_e(TAG, "Unknown relocate type reached. %d", relocation_type);
          // return false;
        }

        // log_v(TAG, "Relocated to [0x%08X]\n", (*relocation_address));
      }
    }
  }

  // Flush caches
  // I don't know what it is used for
  // kuKernelFlushCaches();

  // Save cache
  internal->elf_header = elf_header;
  internal->elf_section_base = elf_section_base;
  internal->elf_segment_base = elf_program_base;
  internal->elf_section_string_table = section_string_table;
  internal->elf_dynamic_string_table = dynamic_string_table;
  internal->elf_dynamic_symbols = dynamic_symbols;
  internal->elf_dynamic_symbol_count = dynamic_symbol_count;
  internal->elf_init_array_section = init_array_section;
  log_i(TAG, "Relocated all symbols.");

  return true;
}

void loader_init_library(dynalib_t *library)
{
  dynalib_t *internal = library;

  // Init array
  int (**init_functions)() =
      (void *)(internal->text_base +
               internal->elf_init_array_section->sh_addr);

  for (int i = 0; i < internal->elf_init_array_section->sh_size / 4; ++i)
  {
    log_v(TAG, "InitArray 0x%08X", init_functions[i]);
    if (init_functions[i] != NULL)
      init_functions[i]();
  }
}

void loader_debug_print_elf_table(dynalib_t *library)
{
  dynalib_t *internal = library;
  log_v(TAG, "Image Base: 0x%08X", internal->library_image_base);
  debug_print_memory_block(internal->library_image_base, 16, 16);

  uintptr_t image_base = (uintptr_t)internal->library_image_base;
  Elf32_Ehdr *elf_header = (Elf32_Ehdr *)image_base;
  {
    log_v(TAG, "ELF Magic: %s", elf_header->e_ident);
    log_v(TAG, "ELF Header Size: %d", elf_header->e_ehsize);
    log_v(TAG, "ELF Virtual Address Entry: 0x%08X", elf_header->e_entry);
    log_v(TAG, "ELF Flags: 0x%08X", elf_header->e_flags);
    log_v(TAG, "ELF Architecture: 0x%08X", elf_header->e_machine);
    log_v(TAG, "ELF Program Entry Size: %d", elf_header->e_phentsize);
    log_v(TAG, "ELF Program Entry Count: %d", elf_header->e_phnum);
    log_v(TAG, "ELF Program Entry Offset: 0x%08X", elf_header->e_phoff);
    log_v(TAG, "ELF Section Entry Size: %d", elf_header->e_shentsize);
    log_v(TAG, "ELF Section Entry Count: %d", elf_header->e_shnum);
    log_v(TAG, "ELF Section Entry Offset: 0x%08X", elf_header->e_shoff);
    log_v(TAG, "ELF Section String Index: %d", elf_header->e_shstrndx);
    log_v(TAG, "ELF Object File Type: 0x%08X", elf_header->e_type);
  }

  Elf32_Phdr *elf_program_base =
      (Elf32_Phdr *)(image_base + elf_header->e_phoff);

  for (int i = 0; i < elf_header->e_phnum; ++i)
  {
    log_v(TAG, "Segment [0x%08X => 0x%08X]",
         elf_program_base[i].p_vaddr,
         image_base + elf_program_base[i].p_vaddr);

    log_v(TAG, "    Type: 0x%08X", elf_program_base[i].p_type);
    log_v(TAG, "    Alignment: %d", elf_program_base[i].p_align);
    log_v(TAG, "    Size (File): %d", elf_program_base[i].p_filesz);
    log_v(TAG, "    Size (Memory): %d", elf_program_base[i].p_memsz);
    log_v(TAG, "    Flags: 0x%08X", elf_program_base[i].p_flags);
    log_v(TAG, "    Data Offset: 0x%08X", elf_program_base[i].p_offset);
    log_v(TAG, "    Virtual Address: 0x%08X", elf_program_base[i].p_vaddr);
    log_v(TAG, "    Physical Address: 0x%08X", elf_program_base[i].p_paddr);
  }

  Elf32_Shdr *elf_section_base =
      (Elf32_Shdr *)(image_base + elf_header->e_shoff);
  char *section_string_table =
      (char *)(image_base +
               elf_section_base[elf_header->e_shstrndx].sh_offset);
  Elf32_Sym *dynamic_symbol_table = NULL;
  char *dynamic_symbol_names = NULL;
  uint32_t dynamic_symbol_count = 0;

  // Print all section informations
  for (int i = 0; i < elf_header->e_shnum; ++i)
  {
    char *section_name =
        section_string_table + elf_section_base[i].sh_name;

    log_v(TAG, "Section %s [0x%08X => 0x%08X]",
         section_name, elf_section_base[i].sh_addr,
         image_base + elf_section_base[i].sh_addr);

    log_v(TAG, "    Virtual Address Exec: 0x%08X",
         elf_section_base[i].sh_addr);
    log_v(TAG, "    Alignment: %d", elf_section_base[i].sh_addralign);
    log_v(TAG, "    Entry Size: %d", elf_section_base[i].sh_entsize);
    log_v(TAG, "    Flags: 0x%08X", elf_section_base[i].sh_flags);
    log_v(TAG, "    Section Info: 0x%08X", elf_section_base[i].sh_info);
    log_v(TAG, "    Another Section Link: 0x%08X",
         elf_section_base[i].sh_link);
    log_v(TAG, "    File Offset : 0x%08X", elf_section_base[i].sh_offset);
    log_v(TAG, "    Size In Bytes: %d", elf_section_base[i].sh_size);
    log_v(TAG, "    Type: 0x%08X", elf_section_base[i].sh_type);

    // Find .dynsym section
    if (strcmp(section_name, ".dynsym") == 0)
    {
      dynamic_symbol_count =
          elf_section_base[i].sh_size / sizeof(Elf32_Sym);
      dynamic_symbol_table =
          (Elf32_Sym *)(image_base + elf_section_base[i].sh_offset);
    }

    // Find .dynstr section
    if (strcmp(section_name, ".dynstr") == 0)
      dynamic_symbol_names =
          (char *)(image_base + elf_section_base[i].sh_offset);
  }

  log_v(TAG, "Import Symbols");
  (void)dynamic_symbol_table;
  (void)dynamic_symbol_names;
  (void)dynamic_symbol_count;

  // Print all symbols of this library
  // for (int i = 0; i < dynamic_symbol_count; ++i) {
  //   log_v(TAG, "    [0x%08X] => [0x%08X] %s",
  //        ((uintptr_t)(dynamic_symbol_table + i)) + 4,
  //        dynamic_symbol_table[i].st_value,
  //        dynamic_symbol_names + dynamic_symbol_table[i].st_name);
  // }
}

void *loader_get_proc_address(dynalib_t *library, const char *symbol_name)
{
  if (!library)
    return NULL;

  dynalib_t *internal = library;

  // Enumerate symbol table
  for (int i = 0; i < internal->elf_dynamic_symbol_count; ++i)
  {
    char *candidate_name =
        internal->elf_dynamic_string_table +
        internal->elf_dynamic_symbols[i].st_name;

    // Compare symbol name
    if (strcmp(candidate_name, symbol_name) == 0)
    {
      log_v(TAG, "ProcAddress found: %s [0x%08X] + [0x%08X]",
           symbol_name, 0x98000000,
           OFFRST(internal->elf_dynamic_symbols[i].st_value));

      return (void *)(uintptr_t)(0x98000000 +
                                 OFFRST(internal->elf_dynamic_symbols[i].st_value));
    }
  }

  return NULL;
}

void *loader_get_library_image_base(dynalib_t *library)
{
  if (!library)
    return NULL;

  return library->library_image_base;
}

dynalib_t *loader_find_library(const char *library_name)
{
  for (int i = 0; i < MAX_LIBRARY; ++i)
  {
    if (__library_slots[i] &&
        !strcmp(__library_slots[i]->library_name, library_name))
    {
      log_v(TAG, "Library found. Index: %d", i);
      return __library_slots[i];
    }
  }

  return NULL;
}

void loader_free_library(dynalib_t *library)
{
  if (!library)
    return;

  log_v(TAG, "Free library %s", library->library_name);

  // Sub reference count
  if (library->ref_count > 0 && --library->ref_count > 0)
    return;

  // Destroy library
  __library_slots[library->slot_index] = NULL;
  --__library_loaded;

  sceKernelFreeMemBlock(library->image_mem_block);
  free(library);
}

dynalib_t *loader_clone_handle(dynalib_t *library)
{
  if (!library)
    return NULL;

  // Add reference count
  ++library->ref_count;

  return library;
}

int32_t loader_find_empty_slot(void)
{
  if (__library_loaded >= MAX_LIBRARY)
    return -1;

  for (int i = 0; i < MAX_LIBRARY; ++i)
    if (__library_slots[i] == NULL)
    {
      return i;
    }

  return -2;
}
