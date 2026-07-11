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

#define CONFIG_TARFS_BIG_ENDIAN 0 /*!< Set to 1 on bige-endian architectures */
#define CONFIG_TARFS_LOG        1 /*!< Enable verbose logging, lots of text! for development or bug hunting */
#define CONFIG_TARFS_INTEGRITY  1 /*!< Support for file data integrity records */

#define CONFIG_TARFS_MAX_FS  4     /*!< Max number of mounted TARFS filesystems */
#define CONFIG_TARFS_MAX_FDS 5     /*!< Max number of active opened files (per filesystem, must be < 33) */
