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
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

/**
 * @brief POSIX mmap()/munmap() support for TARFS
 */
#define PROT_READ     1 /*!< Supported */
#define PROT_WRITE    2 /*!< Ignored, if used together with PROT_READ. Alone causes mmap() error (RO filesystem!) */
#define PROT_EXEC     4 /*!< Ignored */

#define MAP_SHARED    1 /*!< Supported */
#define MAP_PRIVATE   2 /*!< Supported */

#define MAP_FIXED     4 /*!< Ignored, no legit use for this flag :) */
#define MAP_ANONYMOUS 8 /*!< Unsupported, use malloc() instead */
#define MAP_ANON      8 

#define MAP_FAILED   ((void *)(-1))


/**
 * POSIX mmap().
 * Intended to be used like this:
 * 
 * ptr = mmap(NULL, length, PROT_READ, flags, fd, offset);
 */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);

/**
 * POSIX munmap().
 * 
 */
int munmap(void *addr, size_t length);
