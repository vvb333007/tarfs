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
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/errno.h>
#include <sys/fcntl.h>

#include "esp_err.h"
#include "esp_tarfs.h"
#include "fs.h"
#include "file.h"
#include "inode.h"



esp_err_t esp_vfs_tarfs_unregister(const char* mountpoint) {

  return tarfs_unmount(mountpoint) < 0 ? ESP_FAIL : ESP_OK;
}


esp_err_t esp_vfs_tarfs_register(const esp_vfs_tarfs_conf_t * conf) {

  esp_err_t err = ESP_FAIL;

  if (conf == NULL)
    return ESP_ERR_INVALID_ARG;
  

  return err;
}



/**
 * Get information for TARFS
 *
 */
esp_err_t esp_tarfs_info(const char* mountpoint, size_t *raw_size, size_t *data_size) {

  

  if (mountpoint == NULL)
    return ESP_ERR_INVALID_ARG;

#if 0
  int slot;
  if (0 > (slot = findfs(mountpoint)))
    return ESP_ERR_NOT_FOUND;

  if (raw_size)
    *raw_size = s_tarfs[slot].fs_size;

  if (data_size)
    *data_size = s_tarfs[slot].fs_dsize;
#endif
  return ESP_OK;
}

/**
 * Dump filesystems and file descriptors using user-provided printer routine
 * The printer (vtyout) and its context (vty) can be as simple as `fprintf` and `stdout`
 */
void esp_tarfs_dump(void *vty, int (*vtyout)(void *, const char *, ...)) {

  vtyout(vty, "TARFS filesystems:\r\n")
}
