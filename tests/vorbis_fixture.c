#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <vorbis/vorbisenc.h>

#include "vorbis_fixture.h"

static void __write_page(FILE* file, const ogg_page* page)
{
    assert(fwrite(page->header, 1, page->header_len, file) == (size_t)page->header_len);
    assert(fwrite(page->body, 1, page->body_len, file) == (size_t)page->body_len);
}

void make_vorbis_fixture(const char* path, unsigned rate, unsigned channels, unsigned frames)
{
    FILE* file = fopen(path, "wb");
    assert(file);
    vorbis_info info;
    vorbis_comment comment;
    vorbis_dsp_state encoder;
    vorbis_block block;
    ogg_stream_state packets;
    ogg_packet headers[3], packet;
    ogg_page page;
    vorbis_info_init(&info);
    assert(vorbis_encode_init_vbr(&info, channels, rate, 0.3f) == 0);
    vorbis_comment_init(&comment);
    vorbis_comment_add_tag(&comment, "ENCODER", "VitaArc synthetic test fixture");
    assert(vorbis_analysis_init(&encoder, &info) == 0);
    assert(vorbis_block_init(&encoder, &block) == 0);
    assert(ogg_stream_init(&packets, 12345 + rate + channels) == 0);
    assert(vorbis_analysis_headerout(&encoder, &comment, &headers[0], &headers[1], &headers[2]) == 0);
    for(unsigned index = 0; index < 3; ++index) {
        assert(ogg_stream_packetin(&packets, &headers[index]) == 0);
    }

    while(ogg_stream_flush(&packets, &page)) {
        __write_page(file, &page);
    }

    float** pcm = vorbis_analysis_buffer(&encoder, frames);
    assert(pcm);
    for(unsigned channel = 0; channel < channels; ++channel) {
        for(unsigned index = 0; index < frames; ++index) {
            pcm[channel][index] = (float)(0.25 * sin(6.283185307179586 * (440 + channel * 220) * index / rate));
        }
    }

    assert(vorbis_analysis_wrote(&encoder, frames) == 0);
    assert(vorbis_analysis_wrote(&encoder, 0) == 0);
    while(vorbis_analysis_blockout(&encoder, &block) == 1) {
        assert(vorbis_analysis(&block, NULL) == 0);
        assert(vorbis_bitrate_addblock(&block) == 0);
        while(vorbis_bitrate_flushpacket(&encoder, &packet)) {
            assert(ogg_stream_packetin(&packets, &packet) == 0);
            while(ogg_stream_pageout(&packets, &page)) {
                __write_page(file, &page);
            }
        }
    }

    while(ogg_stream_flush(&packets, &page)) {
        __write_page(file, &page);
    }

    assert(ogg_stream_clear(&packets) == 0);
    assert(vorbis_block_clear(&block) == 0);
    vorbis_dsp_clear(&encoder);
    vorbis_comment_clear(&comment);
    vorbis_info_clear(&info);
    assert(fclose(file) == 0);
}
