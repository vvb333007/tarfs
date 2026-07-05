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
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "fs.h"
#include "tar.h"
#include "fnv1a.h"
#include "inode.h"



int main(int argc, char **argv) {

    void *os_handle;
    size_t size;
    int processed = 0;

    const char *filename     = argc > 1 ? argv[1] : "tarfs.tar";
    const char *filename2    = argc > 2 ? argv[2] : "out.tar";

    unsigned char *buf = (unsigned char *)tarfs_os_map_tarfile(filename, &os_handle, &size);

    if (buf == NULL) {
      printf("tarfs: failed to mmap() '%s', err=%d\n", filename, errno);
      return -1;
    }
    printf("tarfs: resource '%s' is mapped (%ld bytes), VADDR=%p\n", filename, size, buf);

    processed = tar_addsum(buf, size);

    printf("%d entries were processed\r\n", processed);      

    if (processed) {
      FILE *f = fopen(filename2, "wb");

      if (f != NULL) {

        fwrite(buf, size, 1, f);
        fclose(f);
        printf("output file '%s' has been created\r\n", filename2);      
        goto unmap_and_exit;
      }
    }
    printf("no output produced\r\n");      
unmap_and_exit:
    printf("tarfs: unmap the filesystem blob\r\n");
    tarfs_os_unmap_tarfile(os_handle, buf, size);

    return 0;
}
