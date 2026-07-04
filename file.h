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

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#ifndef __XTENSA__
typedef int esp_vfs_fs_ops_t;
#endif


/* Compile-time sanity checks. */
_Static_assert(TARFS_MAX_FDS > 0 && TARFS_MAX_FDS <= 32);


/**
 * This internal file descriptor maps to a local fd number 
 * as descriptor_ptr = s_tarfs[slot]->fs_fds[local_fd];
 *
 * @brief TARFS file descriptor, 12 bytes
 *
 */
struct tarfs_fp {

  uintptr_t fp_vaddr;  /*!< file virtual address, absolute. zero indicates unused fd.  */
  uint32_t  fp_pos;    /*!< current file pointer position */
  size_t    fp_size;   /*!< file size in bytes */

};

void tarf_handlers_install(esp_vfs_fs_ops_t *files);
