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


/* Platform abstraction layer.
 *
 * The only mandatory operation is mapping the TAR image into a contiguous
 * read-only memory region. Mutex support is optional and may be implemented
 * as no-ops on single-threaded systems. Likewise, unmap may be a no-op if
 * the mapping has no associated resources.
 */

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/errno.h>
#include <sys/fcntl.h>


/* OS API */

/* TARFS uses a single recursive mutex to protect its internal state.
 *
 * On single-threaded systems, or systems without suitable synchronization
 * primitives, all three functions may be implemented as no-ops.
 */
void  tarfs_os_create_recursive_mutex();
void  tarfs_os_acquire_mutex();
void  tarfs_os_release_mutex();

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
void *tarfs_os_map_tarfile(const char *name, void **os_handle_out, size_t *size_out);
void  tarfs_os_unmap_tarfile(void *os_handle, void *ptr, size_t size);

/* Register or unregister TARFS with the platform VFS.
 * TARFS driver calls this as a final part of the nount() process. Registration function
 * is responsible for registering FS handlers in host VFS
 *
 * On systems providing a Virtual Filesystem layer, these functions should
 * expose TARFS through the native file API (open(), read(), stat(), etc.).
 * See os_esp32.c for the sample implementation
 *
 * On systems without a VFS, both functions may be implemented as `{ return true; }`
 * Applications can then access the filesystem through the TARFS API directly.
 * See os_cygwin.c for the sample implementation
 *
 * @param prefix Path prefix (a mount point)
 * @param context A raw pointer to the driver filesystem descriptor. This pointer will be passed to
 *                each handler function (e.g. to tarfs_open(), tarfs_read() etc)
 * @return
 *   tarfs_os_register_fs():
 *     true  - registration succeeded.
 *     false - registration failed. Mounting is aborted and all resources
 *             allocated during the mount procedure are released.
 *
 *   tarfs_os_unregister_fs():
 *     true  - unregistration succeeded and TARFS may immediately release
 *             all associated resources.
 *
 *     false - unregistration could not complete because the platform may
 *             still issue delayed callbacks into this filesystem. TARFS
 *             will keep critical data structures alive to avoid use-after-
 *             free crashes. This intentionally trades a memory leak for
 *             safety.
 */
bool tarfs_os_register_fs(const char *prefix, void *context);
bool tarfs_os_unregister_fs(const char *prefix);

/**
 * Return the maximum mount point name length supported by the platform.
 *
 * Some operating systems impose a limit on mount point names. TARFS uses
 * this value when validating the archive root directory before mounting.
 *
 * For platforms without such a restriction, return SIZE_MAX.
 *
 * Example:
 *   - ESP-IDF returns 16.
 */
size_t tarfs_os_mp_maxlen();

#define TARFS_OS_MP_MAXLEN 16 /* Max length of the mountpoint name */

/* Windows test environment; only one partition is available */
#ifdef __CYGWIN__
#  define ESP_OK               0
#  define ESP_FAIL            -1
#  define ESP_ERR_INVALID_ARG -2
#  define ESP_PARTITION_TYPE_DATA 1
#  define ESP_PARTITION_SUBTYPE_DATA_TARFS 2
#  define ESP_PARTITION_MMAP_DATA 3
#  define pdFALSE 0
#  define pdTRUE 1
#  define portMAX_DELAY 0xffffffffU
#  define xSemaphoreTakeRecursive(A, B) pdTRUE
#  define xSemaphoreGiveRecursive(A)
#  define xSemaphoreCreateRecursiveMutex() (SemaphoreHandle_t)(1)
#  define esp_partition_find(A,B,C) (esp_partition_iterator_t )(1)

#  define esp_partition_iterator_release(A)
#  define esp_vfs_unregister_fs(A) ESP_OK

typedef void * SemaphoreHandle_t;
typedef int  esp_err_t;
typedef void *  esp_partition_iterator_t;
typedef uintptr_t esp_partition_mmap_handle_t;

typedef struct {
  char label[17];
  size_t size;
} esp_partition_t;

esp_partition_t *esp_partition_get(esp_partition_iterator_t i);
void esp_partition_munmap(esp_partition_mmap_handle_t handle);
esp_err_t esp_partition_mmap(const esp_partition_t *p, size_t off, size_t size, int memory, const void **out_ptr, esp_partition_mmap_handle_t *out_handle);

#else
#  include "freertos/FreeRTOS.h"
#  include "freertos/task.h"
#  include "freertos/semphr.h"
#  include "esp_vfs.h"
#  include "esp_err.h"
#  include "esp_partition.h"
#  include "esp_rom_spiflash.h"
#endif

#undef likely
#undef unlikely
#define unlikely(_X)   __builtin_expect(!!(_X), 0)
#define likely(_X)     __builtin_expect(!!(_X), 1)


