#ifndef __MOCK_PSP2_AUDIODEC_H
#define __MOCK_PSP2_AUDIODEC_H
#include <stdint.h>
#define SCE_AUDIODEC_ALIGNMENT_SIZE 256
#define SCE_AUDIODEC_MP3_MAX_STREAMS 8
#define SCE_AUDIODEC_MP3_MAX_ES_SIZE 1441
#define SCE_AUDIODEC_MP3_MAX_SAMPLES 1152
#define SCE_AUDIODEC_WORD_LENGTH_16BITS 16
#define SCE_AUDIODEC_TYPE_MP3 0x1004
#define SCE_AUDIODEC_MP3_MPEG_VERSION_2_5 0
#define SCE_AUDIODEC_MP3_MPEG_VERSION_RESERVED 1
#define SCE_AUDIODEC_MP3_MPEG_VERSION_2 2
#define SCE_AUDIODEC_MP3_MPEG_VERSION_1 3
typedef union SceAudiodecInitParam {
    struct {
        uint32_t size, totalStreams;
    } mp3;
} SceAudiodecInitParam;

typedef union SceAudiodecInfo {
    struct {
        uint32_t size, ch, version;
    } mp3;
} SceAudiodecInfo;

typedef struct SceAudiodecCtrl {
    uint32_t size;
    int handle;
    uint8_t* pEs;
    uint32_t inputEsSize, maxEsSize;
    void* pPcm;
    uint32_t outputPcmSize, maxPcmSize, wordLength;
    SceAudiodecInfo* pInfo;
} SceAudiodecCtrl;

int sceAudiodecInitLibrary(uint32_t type, SceAudiodecInitParam* parameter);
int sceAudiodecTermLibrary(uint32_t type);
int sceAudiodecCreateDecoder(SceAudiodecCtrl* control, uint32_t type);
int sceAudiodecDeleteDecoder(SceAudiodecCtrl* control);
int sceAudiodecDecode(SceAudiodecCtrl* control);
int sceAudiodecClearContext(SceAudiodecCtrl* control);
#endif
