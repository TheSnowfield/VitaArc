#include <stdlib.h>
#include <string.h>
#include <kubridge/kubridge.h>
#include "kubridge.h"

int kuKernelCpuUnrestrictedMemset(void *dst, int val, size_t len)
{
  void *lpZeroMemory = malloc(len);
  memset(lpZeroMemory, 0x00, len);
  kuKernelCpuUnrestrictedMemcpy(dst, lpZeroMemory, len);
  free(lpZeroMemory);
}
