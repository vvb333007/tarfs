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
#include <assert.h>

#include "os.h"


size_t tarfs_os_mp_maxlen() {

  return 16;
}

bool tarfs_os_unregister_fs(const char *prefix) {
  printf("Unregister FS '%s'\r\n", prefix);
  return true;
}

bool tarfs_os_register_fs(const char *prefix) {
  printf("Register FS '%s'\r\n", prefix);
  return true;
}

void tarfs_os_create_recursive_mutex() { }
void tarfs_os_acquire_mutex() { }
void tarfs_os_release_mutex() { }




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

    FILE *f = fopen(filename, "rb");

    if (!f) {
        perror("fopen");
        return NULL;
    }

    // узнать размер файла
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        printf("Invalid file size\n");
        fclose(f);
        return NULL;
    }

    if (size_out)
      *size_out = size;

    // выделяем буфер
    unsigned char *buf = (unsigned char *)malloc(size);
    if (!buf) {
        printf("malloc failed (%ld bytes)\n", size);
        fclose(f);
        return NULL;
    }

    /* os_handle is a necessary arg */
    assert(os_handle_out != NULL);

    *os_handle_out = (void *)buf;

    // читаем файл целиком
    size_t read_bytes = fread(buf, 1, size, f);
    if (read_bytes != (size_t)size) {
        printf("fread failed: got %zu / %ld bytes\n", read_bytes, size);
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);

    return buf;
}

void tarfs_os_unmap_tarfile(void *handle, void *ptr, size_t size) {
  ptr = ptr;
  size = size;
  free(handle);
}
