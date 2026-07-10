/*
 * TARFS - Immutable (read-only) filesystem for embedded systems.
 *
 * Copyright (c) 2024-2026 Viacheslav Logunov
 * SPDX-License-Identifier: MIT
 *
 * Author:
 *   Viacheslav Logunov <vvb333007@gmail.com>
 *
 * Project:
 *   https://github.com/vvb333007/tarfs
 */


#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/errno.h>
#include <sys/fcntl.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_vfs.h"
#include "esp_err.h"
#include "esp_partition.h"
#include "esp_rom_spiflash.h"


#include "os.h"
#include "refc.h"
#include "fs.h"
#include "file.h"
#include "inode.h"

enum {

  ESP_PARTITION_SUBTYPE_DATA_TARFS  = 0xF0, /* !< TARFS partition, v0 */
  ESP_PARTITION_SUBTYPE_DATA_TARFS1 = 0xF1, /* !< TARFS partition, v1, encrypted */
  ESP_PARTITION_SUBTYPE_DATA_TARFS2 = 0xF2, /* !< TARFS partition, v2, ImFS overlay support */

};


static const esp_vfs_dir_ops_t s_tarfs_dir = {

    .stat_p      = &tarf_stat,
    .link_p      = &tarf_link,
    .unlink_p    = &tarf_unlink,
    .rename_p    = &tarf_rename,

    .opendir_p   = &tard_opendir,
    .closedir_p  = &tard_closedir,
    .readdir_p   = &tard_readdir,
    .readdir_r_p = NULL,
    .seekdir_p   = &tard_seekdir,
    .telldir_p   = &tard_telldir,

    .mkdir_p     = &tard_mkdir,
    .rmdir_p     = &tard_rmdir,

    .truncate_p  = &tarf_truncate,
    .ftruncate_p = &tarf_ftruncate,

    .utime_p     = &tarf_utime,
};

static const esp_vfs_fs_ops_t s_tarfs_fs = {

    .write_p = &tarf_write,
    .lseek_p = &tarf_lseek,
    .read_p  = &tarf_read,
    .pread_p = &tarf_read,
    .pwrite_p= &tarf_pwrite,
    .open_p  = &tarf_open,
    .close_p = &tarf_close,
    .fstat_p = &tarf_fstat,
    .fcntl_p = &tarf_fcntl,
    .ioctl_p = &tarf_ioctl,
    .fsync_p = &tarf_fsync,

    .dir     = &s_tarfs_dir,
}


static SemaphoreHandle_t s_lock = NULL;



/**
 * Lock access to s_tarfs[] table and to the s_numfs counter only. Affects only mount/unmount
 */
void tarfs_os_create_recursive_mutex() {

  if (s_lock == NULL)
    s_lock = xSemaphoreCreateMutex();
}

void tarfs_os_acquire_mutex() {

  if (s_lock != NULL) do { 
      /* absolutely nothing */
  } while (pdFALSE == xSemaphoreTake(s_lock, portMAX_DELAY));
}

void tarfs_os_release_mutex() {

  if (s_lock != NULL)
    xSemaphoreGive(s_lock);
}


/**
 *
 */
void const *tarfs_os_map_tarfile(const char *label, void **os_handle_out, size_t *size_out) {

  esp_partition_iterator_t    i;
  esp_partition_mmap_handle_t handle;
  const esp_partition_t      *part;
  size_t len;
  void *map;

  if (label == NULL || os_handle_out == NULL) {
    errno = EINVAL;
    return false;
  }
  
  /* Fetch pointer to the FLASH partition descriptor by its label */
  i = esp_partition_find(ESP_PARTITION_TYPE_DATA,
                         ESP_PARTITION_SUBTYPE_DATA_TARFS,
                         label);
  if (i == NULL) {
    printf("partition not found (%s)\r\n", label);
    errno = ENOENT;
    return false;
  }

  part = esp_partition_get(i);
  esp_partition_iterator_release(i);

  assert(part != NULL); // non-NULL iterator can not yield NULL partition pointer, but just in case

  if (ESP_OK == esp_partition_mmap(part,0,part->size,ESP_PARTITION_MMAP_DATA,&map,&handle)) {
    *os_handle_out = (void *)handle;
    if (size_out)
      *size_out = part->size;

    printf("Partition %s, vaddr=%p, size=%u\r\n", label, map, part->size);
    return map;
  }

  printf("esp_partition_mmap() failed\r\n");
  errno = ENOMEM;
  return false;
}

/**
 *
 */
bool tarfs_os_unmap_tarfile(const char *label, void *os_handle, size_t size) {
  label = label;
  size = size;
  return ESP_OK == esp_partition_munmap((esp_partition_mmap_handle_t)os_handle)
}



bool tarfs_os_unregister_fs(const char *prefix) {

  return ESP_OK == esp_vfs_unregister_fs(prefix);
}




bool tarfs_os_register_fs(const char *prefix, void *context) {

  return ESP_OK == esp_vfs_register_fs(prefix,
                                      &s_tarfs_fs,
                                       ESP_VFS_FLAG_CONTEXT_PTR |
                                       ESP_VFS_FLAG_STATIC |
                                       ESP_VFS_FLAG_READONLY_FS,
                                       context);
}

void *tarfs_os_malloc(size_t size) {
  return malloc(size);
}

void  tarfs_os_free(void *buffer) {
  free(buffer);
}

