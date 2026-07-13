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

#pragma once

#define CONFIG_TARFS_HAVE_FDOPENDIR  1   /*!< Support for fdopendir() */
#define CONFIG_TARFS_HAVE_MMAP  1        /*!< Support for mmap()/munmap() */
#define CONFIG_TARFS_HAVE_DUPFD  1       /*!< Support for dupfd() */
//#define CONFIG_TARFS_HAVE_READLINK  1  /*!< Support for readlink() */

#define CONFIG_TARFS_BIG_ENDIAN 0   /*!< Set to 1 on bige-endian architectures */
//#define CONFIG_TARFS_LOG        1 /*!< Enable verbose logging, lots of text! for development or bug hunting */
#define CONFIG_TARFS_INTEGRITY  1   /*!< Support for file data integrity records */

#define CONFIG_TARFS_MAX_FS  4      /*!< Max number of mounted TARFS filesystems */
#define CONFIG_TARFS_MAX_FDS 5      /*!< Max number of active opened files (per filesystem, must be < 33) */
