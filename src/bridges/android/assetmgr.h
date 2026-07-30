#ifndef __BRIDGES_ANDROID_ASSETMGR_H
#define __BRIDGES_ANDROID_ASSETMGR_H

#include <stddef.h>

void a_asset_dir_close(void *asset_dir);

void *a_asset_dir_get_next_file_name(void *asset_dir);

void *a_asset_manager_from_java(void *env, void *asset_manager);

void *a_asset_manager_open(void *manager, const char *filename, int mode);

void *a_asset_manager_open_dir(void *manager, const char *directory_name);

void a_asset_close(void *asset);

void a_asset_get_length(void *asset);

int a_asset_open_file_descriptor(void *asset, void *out_start,
                                 void *out_length);

int a_asset_read(void *asset, void *buffer, size_t count);

#endif /* __BRIDGES_ANDROID_ASSETMGR_H */
