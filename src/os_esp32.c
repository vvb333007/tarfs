/*
 * TARFS - Immutable (read-only) filesystem for embedded systems.
 *
 * Copyright (c) 2026 Viacheslav Logunov
 * SPDX-License-Identifier: MMIT
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

#include "config.h"
#include "os.h"
#include "fs.h"
#include "file.h"
#include "dir.h"
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
    .seekdir_p   = &tard_seekdir,
    .telldir_p   = &tard_telldir,

    .mkdir_p     = &tard_mkdir,
    .rmdir_p     = &tard_rmdir,

    .truncate_p  = &tarf_truncate,
    .ftruncate_p = &tarf_ftruncate,
};

static const esp_vfs_fs_ops_t s_tarfs_fs = {

    .write_p = &tarf_write,
    .lseek_p = &tarf_lseek,
    .read_p  = &tarf_read,
    .pread_p = &tarf_pread,
  
    .open_p  = &tarf_open,
    .close_p = &tarf_close,
    .fstat_p = &tarf_fstat,
    .fcntl_p = &tarf_fcntl,
    .ioctl_p = &tarf_ioctl,
    .fsync_p = &tarf_fsync,

    .dir     = &s_tarfs_dir,
};


static SemaphoreHandle_t s_lock = NULL;


/**
 * Create a recursive sync object
 */
void tarfs_os_init() {

  if (s_lock == NULL)
    s_lock = xSemaphoreCreateRecursiveMutex();
}

/**
 * Lock access to s_tarfs[] table and to the s_numfs counter only. 
 */
void tarfs_os_acquire_mutex() {

  if (s_lock != NULL) do { 
      /* absolutely nothing */
  } while (pdFALSE == xSemaphoreTake(s_lock, portMAX_DELAY));
}

/**
 * Unlock access to s_tarfs[] table and to the s_numfs counter only. 
 */
void tarfs_os_release_mutex() {

  if (s_lock != NULL)
    xSemaphoreGive(s_lock);
}


size_t tarfs_os_mp_maxlen() {
  return 16;
}

/* Default allocator 
 * TODO: use heap_caps_malloc(SPIRAM) where available
 */
void *tarfs_os_malloc(size_t size) {

  return malloc(size);
}

/* Default free() 
 * TODO: use heap_caps_free() where available
 */
void  tarfs_os_free(void *buffer) {
 
  free(buffer);
}


/**
 * Map a ESP32 flash partition to a virtual address space
 * @param label esp32 partition label
 * @param os_handle_out a pointer to an opaque OS-specific mapping handle. MUST NOT be NULL
 * @param size_out a pointer to a size_t var where total mapped region size will be stored. Can be NULL
 * @return an address where resource named "label" is accessible
 */
void const *tarfs_os_map_tarfile(const char *label, void **os_handle_out, size_t *size_out) {

  esp_partition_iterator_t    i;
  esp_partition_mmap_handle_t handle;
  const esp_partition_t      *part;
  
  void const *map;

  if (label == NULL || os_handle_out == NULL) {
    logerr("ESP32: invalid arguments\r\n");
    errno = EINVAL;
    return NULL;
  }
  
  /* Fetch pointer to the FLASH partition descriptor by its label */
  i = esp_partition_find(ESP_PARTITION_TYPE_DATA,
                         ESP_PARTITION_SUBTYPE_ANY,
                         label);
  if (i == NULL) {
    logerr("ESP32: partition not found '%s'\r\n", label);
    errno = ENOENT;
    return NULL;
  }

  part = esp_partition_get(i);
  esp_partition_iterator_release(i);

  /* non-NULL iterator can not yield NULL partition pointer, but just in case */
  if (part == NULL) {
    logerr("esp_partition_get() returned NULL, this must not happen!\r\n");
    errno = EIO;
    return NULL;
  }

  if (ESP_OK == esp_partition_mmap(part,0,part->size,ESP_PARTITION_MMAP_DATA,&map,&handle)) {
    *os_handle_out = (void *)handle;
    if (size_out)
      *size_out = part->size;

    log("ESP32: partition '%s' -> vaddr=%p, size=%u\r\n", label, map, part->size);

    return map;
  }

  logerr("esp_partition_mmap() failed\r\n");
  errno = ENOMEM;
  return NULL;
}

/**
 * Opposite of tarfs_os_map_tarfile()
 *
 * @param label esp32 partition label
 * @param os_handle an opaque value as returned by tarfs_os_map_tarfile()
 * @param size total mapped region size
 *
 */
void tarfs_os_unmap_tarfile(void *os_handle, const void *map, size_t size) {
  map = map;
  size = size;
  esp_partition_munmap((esp_partition_mmap_handle_t)os_handle);
}


/**
 * Tell the VFS that path 'prefix' is not handled by tarfs anymore.
 * Called by commit_unmoount()
 */
bool tarfs_os_unregister_fs(const char *prefix) {

  return ESP_OK == esp_vfs_unregister_fs(prefix);
}



/**
 * Tell the VFS that TARFS is now handle all the paths starting from 'prefix'.
 * VFS in turn promises that it will pass the 'context' in every handler function
 * later (e.g. tarf_open(context,) etc)
 *
 */
bool tarfs_os_register_fs(const char *prefix, void *context) {

  return ESP_OK == esp_vfs_register_fs(prefix,
                                      &s_tarfs_fs,
                                       ESP_VFS_FLAG_CONTEXT_PTR |
                                       ESP_VFS_FLAG_STATIC |
                                       ESP_VFS_FLAG_READONLY_FS,
                                       context);
}
