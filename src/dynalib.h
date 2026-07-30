#ifndef __DYNALIB_H
#define __DYNALIB_H

#include <types.h>
#include <elf.h>

typedef struct dynalib_t
{
  uint32_t ref_count;
  uint32_t slot_index;
  char library_name[64];
  char library_path[128];
  void *library_image_base;
  SceUID image_mem_block;

  Elf32_Ehdr *elf_header;
  Elf32_Phdr *elf_segment_base;
  Elf32_Shdr *elf_section_base;
  Elf32_Shdr *elf_symbol_section;
  Elf32_Shdr *elf_init_array_section;

  void *elf_section_string_table;
  Elf32_Sym *elf_dynamic_symbols;
  void *elf_dynamic_string_table;
  uint32_t elf_dynamic_symbol_count;

  void *text_base;
} dynalib_t;

#endif /* __DYNALIB_H */
