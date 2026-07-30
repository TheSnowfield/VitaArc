#ifndef __BRIDGES_ANDROID_ASSETMGR_H
#define __BRIDGES_ANDROID_ASSETMGR_H

#include <stddef.h>
#include <sys/types.h>

typedef struct a_asset_manager a_asset_manager_t;
typedef struct a_asset_dir a_asset_dir_t;
typedef struct a_asset a_asset_t;

enum
{
  AASSET_MODE_UNKNOWN = 0,
  AASSET_MODE_RANDOM = 1,
  AASSET_MODE_STREAMING = 2,
  AASSET_MODE_BUFFER = 3
};

void a_asset_dir_close(a_asset_dir_t *asset_dir);
const char *a_asset_dir_get_next_file_name(a_asset_dir_t *asset_dir);
void a_asset_dir_rewind(a_asset_dir_t *asset_dir);

a_asset_manager_t *a_asset_manager_from_java(void *env,
                                             void *asset_manager);
a_asset_t *a_asset_manager_open(a_asset_manager_t *manager,
                                const char *filename, int mode);
a_asset_dir_t *a_asset_manager_open_dir(a_asset_manager_t *manager,
                                        const char *directory_name);

void a_asset_close(a_asset_t *asset);
int a_asset_read(a_asset_t *asset, void *buffer, size_t count);
off_t a_asset_seek(a_asset_t *asset, off_t offset, int whence);
const void *a_asset_get_buffer(a_asset_t *asset);
off_t a_asset_get_length(a_asset_t *asset);
off_t a_asset_get_remaining_length(a_asset_t *asset);
int a_asset_open_file_descriptor(a_asset_t *asset, off_t *out_start,
                                 off_t *out_length);
int a_asset_is_allocated(a_asset_t *asset);

#endif /* __BRIDGES_ANDROID_ASSETMGR_H */
