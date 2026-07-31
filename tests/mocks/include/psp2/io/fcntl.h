#ifndef __MOCK_PSP2_IO_FCNTL_H
#define __MOCK_PSP2_IO_FCNTL_H
#include <stdint.h>
int64_t sceIoLseek(int fd, int64_t offset, int whence);
int sceIoOpen(const char* path, int flags, unsigned int mode);
int sceIoClose(int fd);
int sceIoRead(int fd, void* buffer, unsigned int size);
int sceIoWrite(int fd, const void* buffer, unsigned int size);
int sceIoSyncByFd(int fd, unsigned int flags);
int sceIoRename(const char* old_path, const char* new_path);
int sceIoRemove(const char* path);
#endif
