#include <string.h>
#include <stddef.h>
#include <psp2/types.h>

#include "string.h"

const char *utilGetFileName(const char *szFilePath)
{
  uint32_t strLength = strlen(szFilePath);

  for (int i = strLength; i > 0; --i)
  {
    if (szFilePath[i] == '/' && i < strLength)
    {
      return &szFilePath[i + 1];
    }
  }

  return NULL;
}

size_t utilUcharLen(const uint16_t *lpcszString)
{
  if (!lpcszString)
    return -1;

  for (uint16_t *i = lpcszString;; ++i)
    if (*(i) == 0) return i - lpcszString;

  return -1;
}
