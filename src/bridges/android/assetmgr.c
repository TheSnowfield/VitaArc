#include "assetmgr.h"

void a_asset_dir_close(void *asset_dir)
{
  (void)asset_dir;
}

void *a_asset_dir_get_next_file_name(void *asset_dir)
{
  (void)asset_dir;
  return NULL;
}

void *a_asset_manager_from_java(void *env, void *asset_manager)
{
  (void)env;
  (void)asset_manager;
  return NULL;
}

void *a_asset_manager_open(void *manager, const char *filename, int mode)
{
  (void)manager;
  (void)filename;
  (void)mode;
  return NULL;
}

void *a_asset_manager_open_dir(void *manager, const char *directory_name)
{
  (void)manager;
  (void)directory_name;
  return NULL;
}

void a_asset_close(void *asset)
{
  (void)asset;
}

void a_asset_get_length(void *asset)
{
  (void)asset;
}

int a_asset_open_file_descriptor(void *asset, void *out_start,
                                 void *out_length)
{
  (void)asset;
  (void)out_start;
  (void)out_length;
  return 0;
}

int a_asset_read(void *asset, void *buffer, size_t count)
{
  (void)asset;
  (void)buffer;
  (void)count;
  return 0;
}
