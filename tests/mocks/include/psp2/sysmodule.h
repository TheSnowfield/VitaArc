#ifndef __MOCK_PSP2_SYSMODULE_H
#define __MOCK_PSP2_SYSMODULE_H
#define SCE_SYSMODULE_AUDIOCODEC 0x37
#define SCE_SYSMODULE_LOADED 0
int sceSysmoduleIsLoaded(int module);
int sceSysmoduleLoadModule(int module);
int sceSysmoduleUnloadModule(int module);
#endif
