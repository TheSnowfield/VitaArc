#include <config.h>
#include <logcat/logcat.h>
#include <kubridge/kubridge.h>
#include <string.h>
#include "kubridge.h"
#include "patcher.h"

static const patch_func_t *__find_bridge_symbol(
    const patch_func_t bridge_functions[], uint32_t bridge_count,
    const char *symbol_name)
{
  uint32_t left = 0;
  uint32_t right = bridge_count;

  while (left < right)
  {
    uint32_t middle = left + (right - left) / 2;
    int result = strcmp(symbol_name, bridge_functions[middle].symbol_name);
    if (result == 0)
      return &bridge_functions[middle];
    if (result < 0)
      right = middle;
    else
      left = middle + 1;
  }

  return NULL;
}

void patch_symbols(dynalib_t *library,
                   const patch_func_t bridge_functions[],
                   uint32_t bridge_count)
{
  if (!library || !bridge_functions || !bridge_count)
    return;

  uint32_t patched_relocations = 0;
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
      const char *symbol_name =
          (const char *)library->elf_dynamic_string_table + symbol->st_name;
      const patch_func_t *bridge =
          __find_bridge_symbol(bridge_functions, bridge_count, symbol_name);
      if (!bridge)
        continue;

      uintptr_t *relocation_address =
          (uintptr_t *)(uintptr_t)(0x98000000 + relocations[j].r_offset);
      *relocation_address = bridge->bridge_proc;
      ++patched_relocations;
    }
  }

  log_i(TAG, "Patched %u relocations from %u bridge symbols.",
        patched_relocations, bridge_count);
}

void patch_address(dynalib_t *library,
                   const patch_address_t patches[], uint32_t patch_count)
{
  (void)library;

  // Get image base
  uintptr_t image_base = 0x98000000;
  {
    if (!image_base)
    {
      log_e(TAG, "Patch image base is NULL");
      return;
    }
  }

  // Apply patches
  for (int i = 0; i < patch_count; ++i)
  {
    // Calculate address
    uintptr_t target_address = image_base + patches[i].patch_offset;

    // Apply patch
    if (patches[i].patch_length < 8)
      kuKernelCpuUnrestrictedMemcpy((void *)target_address,
                                    &patches[i].patch_value,
                                    patches[i].patch_length);
    else
      log_w(TAG, "Patch length over 8 bytes.");
  }
}

void patch_uint32(dynalib_t *library, uint32_t patch_offset,
                  uint32_t patch_value)
{
  (void)library;

  // Get image base
  uintptr_t image_base = 0x98000000;
  {
    if (!image_base)
    {
      log_e(TAG, "Patch image base is NULL");
      return;
    }
  }

  // Calculate address
  uintptr_t target_address = image_base + patch_offset;

  // Apply patch
  kuKernelCpuUnrestrictedMemcpy((void *)target_address,
                                &patch_value, 4);
}

void patch_uint16(dynalib_t *library, uint32_t patch_offset,
                  uint16_t patch_value)
{
  (void)library;

  // Get image base
  uintptr_t image_base = 0x98000000;
  {
    if (!image_base)
    {
      log_e(TAG, "Patch image base is NULL");
      return;
    }
  }

  // Calculate address
  uintptr_t target_address = image_base + patch_offset;

  // Apply patch
  kuKernelCpuUnrestrictedMemcpy((void *)target_address,
                                &patch_value, 2);
}

void patch_thumb(dynalib_t *library, uint32_t patch_offset,
                 uint16_t patch_value)
{
  return patch_uint16(library, patch_offset, patch_value);
}

void patch_arm(dynalib_t *library, uint32_t patch_offset,
               uint32_t patch_value)
{
  return patch_uint32(library, patch_offset, patch_value);
}

void hook_stub_proc(dynalib_t *library, uint32_t hook_offset,
                    void *hook_callback)
{
  if (!library ||
      !hook_offset ||
      !hook_callback)
    return;

  // Get image base
  uintptr_t image_base = 0x98000000;
  {
    if (!image_base)
    {
      log_e(TAG, "Patch image base is NULL");
      return;
    }
  }

  // Calculate address
  uintptr_t hook_address = image_base + hook_offset;

  // hook
  uint32_t hook[2];
  hook[0] = 0xE51FF004; // LDR PC, [PC, #-4]
  hook[1] = (uint32_t)(uintptr_t)hook_callback;
  kuKernelCpuUnrestrictedMemcpy((void *)hook_address, hook, sizeof(hook));
}
