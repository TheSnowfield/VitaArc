#include <stdlib.h>
#include <string.h>
#include <kubridge/kubridge.h>
#include "kubridge.h"

int kuKernelCpuUnrestrictedMemset(void *destination, int value,
                                 size_t length)
{
  void *fill_buffer = malloc(length);
  memset(fill_buffer, value, length);
  int result =
      kuKernelCpuUnrestrictedMemcpy(destination, fill_buffer, length);
  free(fill_buffer);
  return result;
}
