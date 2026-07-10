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

#include "os.h"


size_t tarfs_os_mp_maxlen() {

  return 255;
}

bool tarfs_os_unregister_fs(const char *prefix) {

  return true;
}

bool tarfs_os_register_fs(const char *prefix, void *context) {

  return true;
}

void tarfs_os_create_recursive_mutex() {
}

void tarfs_os_acquire_mutex() {
}

void tarfs_os_release_mutex() {
}

void *tarfs_os_malloc(size_t size) { return malloc(size); }
void  tarfs_os_free(void *buffer) { free(buffer); }


/* Map a TAR filesystem image into the process address space.
 *
 * TARFS expects the archive to be accessible as a contiguous, read-only
 * memory region. How this is achieved is platform-specific:
 *
 *   - memory-mapped flash (e.g. RP2040 XIP),
 *   - runtime memory mapping (e.g. Linux, ESP-IDF),
 *   - linker-embedded binary image,
 *   - or any other mechanism that provides a linear memory view.
 *
 * The returned pointer must remain valid until tarfs_os_unmap_tarfile()
 * is called.
 */
void *tarfs_os_map_tarfile(const char *filename, void **os_handle_out, size_t *size_out) {


    return NULL;
}

void tarfs_os_unmap_tarfile(void *handle, void *ptr, size_t size) {

}
