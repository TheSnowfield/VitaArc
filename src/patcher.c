#include <config.h>
#include <logcat/logcat.h>
#include <kubridge/kubridge.h>
#include "kubridge.h"
#include "patcher.h"

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
