#include <string.h>
#include <stddef.h>
#include <psp2/types.h>

#include "string.h"

const char *util_get_file_name(const char *file_path)
{
  uint32_t string_length = strlen(file_path);

  for (int i = string_length; i > 0; --i)
  {
    if (file_path[i] == '/' && i < string_length)
    {
      return &file_path[i + 1];
    }
  }

  return NULL;
}

size_t util_uchar_len(const uint16_t *string)
{
  if (!string)
    return -1;

  for (const uint16_t *i = string;; ++i)
    if (*i == 0)
      return i - string;

  return -1;
}
