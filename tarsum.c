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
#include "hash.h"
#include "inode.h"



int main(int argc, char **argv) {

    void *os_handle;
    size_t size;
    int processed = 0;

    if (argc < 2) {
      puts("Usage: tarsum INPUT.TAR [OUTPUT.TAR]");
      puts("\r\nEmbed CRC64 integrity values into a TAR file");
      return 0;
    }

    const char *filename     = argv[1];
    const char *filename2    = argc > 2 ? argv[2] : "out.tar";


    unsigned char *buf = (unsigned char *)tarfs_os_map_tarfile(filename, &os_handle, &size);

    if (buf == NULL) {
      log("failed to load '%s', errno=%d\n", filename, errno);
      return -1;
    }

    log("TAR file '%s' loaded (%ld bytes), processing..\n", filename, size);

    processed = tar_addsum(buf, size);

    log("Done: %d entries were processed\r\n", processed);      

    if (processed) {
      FILE *f = fopen(filename2, "wb");

      if (f != NULL) {

        fwrite(buf, size, 1, f);
        fclose(f);
        log("output file '%s' has been created\r\n", filename2);      
        goto unmap_and_exit;
      }
    }
    log("No output produced: %s\r\n", processed ? "can not create output file" : "no updatable entries");
unmap_and_exit:
    tarfs_os_unmap_tarfile(os_handle, buf, size);
    return 0;
}
