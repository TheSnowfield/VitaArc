#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <logcat/logcat.h>

#include "compat.h"

typedef struct {
    const char* name;
    const char* value;
} property_t;

static const property_t __properties[] = {
    {"ro.product.model", "PlayStation Vita"},
    {"ro.product.manufacturer", "Sony"},
    {"ro.product.cpu.abi", "armeabi-v7a"},
    {"ro.build.version.release", "4.4"},
};

uint8_t bridge_bionic_stdio[1024] __attribute__((aligned(8)));
// Legacy Bionic imports are pointer variables, not the tables themselves.
// Entry zero represents EOF; byte values index the following 256 entries.
static uint8_t __ctype[257];
static int16_t __tolower[257];
static int16_t __toupper[257];
const uint8_t* bridge_bionic_ctype = __ctype;
const int16_t* bridge_bionic_tolower = __tolower;
const int16_t* bridge_bionic_toupper = __toupper;

static uint32_t bridge_sl_token_engine;
static uint32_t bridge_sl_token_play;
static uint32_t bridge_sl_token_prefetch_status;
static uint32_t bridge_sl_token_seek;
static uint32_t bridge_sl_token_volume;

const void* bridge_sl_iid_engine = &bridge_sl_token_engine;
const void* bridge_sl_iid_play = &bridge_sl_token_play;
const void* bridge_sl_iid_prefetch_status = &bridge_sl_token_prefetch_status;
const void* bridge_sl_iid_seek = &bridge_sl_token_seek;
const void* bridge_sl_iid_volume = &bridge_sl_token_volume;

static bool bridge_compat_initialized;

void bridge_compat_initialize(void)
{
    if(bridge_compat_initialized) {
        return;
    }

    __tolower[0] = __toupper[0] = -1;
    for(uint32_t value = 0; value < 256; ++value) {
        uint32_t index = value + 1;
        uint8_t flags = 0;
        if(value >= 'A' && value <= 'Z') {
            flags |= 0x01;
        }

        if(value >= 'a' && value <= 'z') {
            flags |= 0x02;
        }

        if(value >= '0' && value <= '9') {
            flags |= 0x04;
        }

        if(value == ' ' || (value >= '\t' && value <= '\r')) {
            flags |= 0x08;
        }

        if((value >= '!' && value <= '/') || (value >= ':' && value <= '@') || (value >= '[' && value <= '`') ||
            (value >= '{' && value <= '~') || value >= 0xa0) {
            flags |= 0x10;
        }

        if(value < ' ' || (value >= 0x7f && value < 0xa0)) {
            flags |= 0x20;
        }

        if((value >= 'A' && value <= 'F') || (value >= 'a' && value <= 'f')) {
            flags |= 0x40;
        }

        if(value == ' ') {
            flags |= 0x80;
        }

        __ctype[index] = flags;
        __tolower[index] = value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
        __toupper[index] = value >= 'a' && value <= 'z' ? value - ('a' - 'A') : value;
    }

    bridge_compat_initialized = true;
}

void bridge_google_blocking_region_begin(void)
{}

void bridge_google_blocking_region_end(void)
{}

int bridge_system_property_get(const char* name, char* value)
{
    if(!value) {
        return 0;
    }

    const char* result = "";
    if(name) {
        for(size_t index = 0; index < sizeof(__properties) / sizeof(__properties[0]); ++index) {
            if(strcmp(name, __properties[index].name)) {
                continue;
            }

            result = __properties[index].value;
            break;
        }
    }

    strcpy(value, result);
    return (int)strlen(result);
}

int bridge_sl_create_engine(void* engine, uint32_t option_count, const void* options, uint32_t interface_count,
    const void* interface_ids, const void* interface_required)
{
    (void)engine;
    (void)option_count;
    (void)options;
    (void)interface_count;
    (void)interface_ids;
    (void)interface_required;
    log_w(TAG, "OpenSL ES initialization was rejected by the Vita backend");
    return -1;
}
