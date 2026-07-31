#include <stdlib.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include "fs.h"

bool util_file_exists(const char* file_path)
{
    SceIoStat status;
    return sceIoGetstat(file_path, &status) >= 0;
}

uint32_t util_get_file_size(const char* file_path)
{
    SceUID file_id;
    file_id = sceIoOpen(file_path, SCE_O_RDONLY, 0777);
    if(file_id < 0) {
        return 0;
    }

    int64_t file_size = sceIoLseek(file_id, 0, SCE_SEEK_END);
    sceIoClose(file_id);
    return file_size > 0 && (uint64_t)file_size <= UINT32_MAX ? (uint32_t)file_size : 0;
}

bool read_file_all(const char* file_path, void* memory, uint32_t buffer_size)
{
    if(!util_file_exists(file_path)) {
        return false;
    }

    if(!memory) {
        return false;
    }

    SceUID file_id;

    file_id = sceIoOpen(file_path, SCE_O_RDONLY, 0777);
    if(file_id < 0) {
        return false;
    }

    sceIoLseek(file_id, 0, SCE_SEEK_SET);
    int32_t bytes_read = sceIoRead(file_id, memory, buffer_size);
    sceIoClose(file_id);
    return bytes_read == (int32_t)buffer_size;
}
