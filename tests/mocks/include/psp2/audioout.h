#ifndef __MOCK_PSP2_AUDIOOUT_H
#define __MOCK_PSP2_AUDIOOUT_H
#define SCE_AUDIO_OUT_PORT_TYPE_MAIN 0
#define SCE_AUDIO_OUT_MODE_STEREO 1
int sceAudioOutOpenPort(int type, int grain, int rate, int mode);
int sceAudioOutOutput(int port, const void* output);
int sceAudioOutReleasePort(int port);
#endif
