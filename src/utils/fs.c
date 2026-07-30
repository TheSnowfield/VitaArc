#include <stdlib.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include "fs.h"

bool util_file_exists(const char *file_path)
{
  SceIoStat status;
  return sceIoGetstat(file_path, &status) >= 0;
}

uint32_t util_get_file_size(const char *file_path)
{
  SceUID file_id;
  uint32_t file_size;

  file_id = sceIoOpen(file_path, SCE_O_RDONLY, 0777);
  file_size = sceIoLseek(file_id, 0, SCE_SEEK_END);

  sceIoClose(file_id);
  return file_size;
}

bool read_file_all(const char *file_path, void *memory,
                   uint32_t buffer_size)
{
  if (util_file_exists(file_path))
    return false;

  if (!memory)
    return false;

  SceUID file_id;

  file_id = sceIoOpen(file_path, SCE_O_RDONLY, 0777);
  {
    sceIoLseek(file_id, 0, SCE_SEEK_SET);
    sceIoRead(file_id, memory, buffer_size);
  }
  sceIoClose(file_id);

  return true;
}
