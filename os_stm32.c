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

#include "config.h"
#include "os.h"


size_t tarfs_os_mp_maxlen() {

  return 16;
}

bool tarfs_os_unregister_fs(const char *prefix) {

  return true;
}

bool tarfs_os_register_fs(const char *prefix, void *context) {

  return true;
}

void tarfs_os_init() { }
void tarfs_os_acquire_mutex() { }
void tarfs_os_release_mutex() { }




/* FLASH is already mapped to our address space
 *
 *   - memory-mapped flash (e.g. RP2040 XIP, STM32),  e.g.
 *    via direct address access like 0x40000000: address and size must be known
 *   - linker magic (linker-added binary blob)        e.g. 
 *    via extern const uint8_t __tarfs_start[]; extern const size_t __tarfs_end;
 *    address and size are defined by linker, binary blob is attached to the firmware directly
 */
#define TARFILE_ADDR (uintptr_t)0x40000000
#define TARFILE_SIZE (size_t)0x100000

void const *tarfs_os_map_tarfile(const char *filename, void **os_handle_out, size_t *size_out) {

  if (size_out)
    *size_out = TARFILE_SIZE;

    return (void *)(TARFILE_ADDR);
}

void tarfs_os_unmap_tarfile(void *handle, void const *ptr, size_t size) {

}
