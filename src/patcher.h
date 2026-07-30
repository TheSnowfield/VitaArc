#ifndef __PATCHER_H
#define __PATCHER_H

#include "types.h"
#include "dynalib.h"

typedef struct {
  const char *symbol_name;
  uintptr_t bridge_proc;
} patch_func_t;

typedef struct {
  uintptr_t patch_offset;
  uint64_t patch_value;
  uint8_t patch_length;
} patch_address_t;

/**
 * Patch all matching ELF relocations in one pass.
 * bridge_functions must be sorted by symbol_name.
 */
void patch_symbols(dynalib_t *library,
                   const patch_func_t bridge_functions[],
                   uint32_t bridge_count);

void patch_address(dynalib_t *library, const patch_address_t patches[],
                   uint32_t patch_count);

void patch_uint32(dynalib_t *library, uint32_t patch_offset,
                  uint32_t patch_value);

void patch_uint16(dynalib_t *library, uint32_t patch_offset,
                  uint16_t patch_value);

void patch_thumb(dynalib_t *library, uint32_t patch_offset,
                 uint16_t patch_value);

void patch_arm(dynalib_t *library, uint32_t patch_offset,
               uint32_t patch_value);

void hook_stub_proc(dynalib_t *library, uint32_t hook_offset,
                    void *hook_callback);

#endif /* __PATCHER_H */
