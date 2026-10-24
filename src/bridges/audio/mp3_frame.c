#include <psp2/audiodec.h>
#include <string.h>

#include "mp3_frame.h"

bool mp3_frame_parse(const uint8_t data[4], mp3_frame_t* frame)
{
    static const uint16_t mpeg1[16] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
    static const uint16_t mpeg2[16] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0};
    static const uint32_t rates[3] = {44100, 48000, 32000};

    uint32_t header = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
        ((uint32_t)data[2] << 8) | data[3];
    if((header & 0xFFE00000) != 0xFFE00000) {
        return false;
    }

    uint32_t version = (header >> 19) & 3;
    uint32_t layer = (header >> 17) & 3;
    uint32_t bitrate = (header >> 12) & 15;
    uint32_t rate = (header >> 10) & 3;
    uint32_t padding = (header >> 9) & 1;
    if(version == SCE_AUDIODEC_MP3_MPEG_VERSION_RESERVED || layer != 1) {
        return false;
    }

    if(bitrate == 0 || bitrate == 15 || rate == 3) {
        return false;
    }

    frame->rate = rates[rate];
    const uint16_t* table = mpeg1;
    switch(version) {
        case SCE_AUDIODEC_MP3_MPEG_VERSION_2:
            frame->rate /= 2;
            table = mpeg2;
            break;

        case SCE_AUDIODEC_MP3_MPEG_VERSION_2_5:
            frame->rate /= 4;
            table = mpeg2;
            break;
    }

    frame->samples = version == SCE_AUDIODEC_MP3_MPEG_VERSION_1 ? 1152 : 576;
    frame->size = (frame->samples / 8) * table[bitrate] * 1000 / frame->rate + padding;
    if(frame->size < 4 || frame->size > SCE_AUDIODEC_MP3_MAX_ES_SIZE) {
        return false;
    }

    frame->version = version;
    frame->channels = ((header >> 6) & 3) == 3 ? 1 : 2;
    uint32_t side = frame->channels == 1 ? 17 : 32;
    if(version != SCE_AUDIODEC_MP3_MPEG_VERSION_1) {
        side = frame->channels == 1 ? 9 : 17;
    }

    uint32_t overhead = 4 + side + ((header & 0x10000) ? 0 : 2);
    frame->payload = frame->size > overhead ? frame->size - overhead : 0;
    return true;
}

bool mp3_frame_read(FILE* file, uint8_t header[4], mp3_frame_t* frame)
{
    if(fread(header, 1, 4, file) != 4) {
        return false;
    }

    for(;;) {
        if(mp3_frame_parse(header, frame)) {
            return true;
        }

        int byte = fgetc(file);
        if(byte == EOF) {
            return false;
        }

        memmove(header, header + 1, 3);
        header[3] = (uint8_t)byte;
    }
}

bool mp3_frame_matches(const mp3_frame_t* frame, const mp3_frame_t* format)
{
    if(frame->version != format->version || frame->rate != format->rate) {
        return false;
    }

    return frame->channels == format->channels;
}
