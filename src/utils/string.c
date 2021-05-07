#include <string.h>
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
