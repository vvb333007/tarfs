/*
 * TARFS - Immutable (read-only) filesystem for embedded systems.
 *
 * Copyright (c) 2026 Viacheslav Logunov
 * SPDX-License-Identifier: MIT
 *
 * Author:
 *   Viacheslav Logunov <vvb333007@gmail.com>
 *
 * Project:
 *   https://github.com/vvb333007/tarfs
 */



/*
 * Memory-mapped Flash backend.
 *
 * Examples:
 *   - STM32 internal Flash
 *   - RP2040 XIP Flash
 *   - Linker-provided binary blobs
 */

#define TARFILE_ADDR 0x08000000 /* An example. Change to the real value */
#define TARFILE_SIZE (128*1024)

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "config.h"
#include "os.h"


size_t tarfs_os_mp_maxlen() { return 16; }

/* No-ops */
void tarfs_os_init() { }

void tarfs_os_acquire_mutex() { }
void tarfs_os_release_mutex() { }

bool tarfs_os_unregister_fs(const char *prefix) { return true; }
bool tarfs_os_register_fs(const char *prefix, void *context) { return true; }

/* Memory backend */
void *tarfs_os_malloc(size_t size) { return malloc(size); }
void  tarfs_os_free(void *buffer) { free(buffer); }


/**
 * Return a pointer to the TAR image stored in memory-mapped Flash.
 *
 * On STM32 the internal Flash is directly accessible through the CPU
 * address space, therefore no explicit mapping is required.
 *
 * The filename parameter is ignored.
 */

void const *tarfs_os_map_tarfile(const char *filename, void **os_handle_out, size_t *size_out) {

  void const *tarfile = (void const *)(uintptr_t)(TARFILE_ADDR);

  if (size_out)
    *size_out = (size_t)TARFILE_SIZE;

  *os_handle_out = (void *)tarfile;
  return tarfile;
}

/**
 * No-op
 */
void tarfs_os_unmap_tarfile(void *handle, void const *ptr, size_t size) {

}
