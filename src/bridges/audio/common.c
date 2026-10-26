#include <math.h>
#include <stdio.h>
#include <string.h>
#include <config.h>
#include "state.h"

float audio_gain(float volume)
{
    if(!isfinite(volume))
        return 0.0f;

    if(volume < 0.0f) {
        return 0.0f;
    }

    if(volume > 1.0f) {
        return 1.0f;
    }

    return volume;
}

bool audio_path(const char* source, char* destination, size_t destination_size)
{
    static const char asset_prefix[] = "file:///android_asset/";
    static const char sdcard_prefix[] = "/mnt/sdcard/arc/";

    if(!source || !destination || destination_size == 0) {
        return false;
    }

    int length = 0;
    if(strncmp(source, asset_prefix, sizeof(asset_prefix) - 1) == 0) {
        length = snprintf(destination, destination_size, "%s/%s", PATH_TO_ASSETS, source + sizeof(asset_prefix) - 1);
    } else if(strncmp(source, sdcard_prefix, sizeof(sdcard_prefix) - 1) == 0) {
        length = snprintf(destination, destination_size, "%s%s", PATH_TO_USER, source + sizeof(sdcard_prefix) - 1);
    } else if(strncmp(source, "assets/", 7) == 0) {
        length = snprintf(destination, destination_size, "%s%s", PATH_TO_PACKAGE, source);
    } else if(strncmp(source, "audio/", 6) == 0 || strncmp(source, "songs/", 6) == 0) {
        length = snprintf(destination, destination_size, "%s/%s", PATH_TO_ASSETS, source);
    } else {
        length = snprintf(destination, destination_size, "%s", source);
    }

    return length >= 0 && (size_t)length < destination_size;
}
