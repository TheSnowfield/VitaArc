#include <stdlib.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include "fs.h"

bool utilFileExists(const char *szFilePath)
{
  SceIoStat status;
  return sceIoGetstat(szFilePath, &status) >= 0;
}

uint32_t utilGetFileSize(const char *szFilePath)
{
  SceUID fileId;
  uint32_t fileSize;

  fileId = sceIoOpen(szFilePath, SCE_O_RDONLY, 0777);
  fileSize = sceIoLseek(fileId, 0, SCE_SEEK_END);

  sceIoClose(fileId);
  return fileSize;
}

bool utilsReadFileAll(const char *szFilePath, void *lpMemory, uint32_t nBufferSize)
{
  if (utilFileExists(szFilePath))
    return false;

  if (!lpMemory)
    return false;

  SceUID fileId;

  fileId = sceIoOpen(szFilePath, SCE_O_RDONLY, 0777);
  {
    sceIoLseek(fileId, 0, SCE_SEEK_SET);
    sceIoRead(fileId, lpMemory, nBufferSize);
  }
  sceIoClose(fileId);

  return true;
}
