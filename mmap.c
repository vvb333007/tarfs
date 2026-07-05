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

#include "config.h"
#include "os.h"
#include "fs.h"
#include "file.h"
#include "inode.h"
#include "tar.h"

/**
 * POSIX mmap().
 * Intended to be used like this:
 * 
 * ptr = mmap(NULL, length, PROT_READ, flags, fd, offset);
 */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {

  struct ioctl_req io;
  struct tarfs_fp *fp;
  struct tarfs_fs *fs;

  if (((flags & MAP_FIXED) && addr != NULL) ||
      ((flags & MAP_ANONYMOUS) && fd < 0) ||
      ((prot & (PROT_WRITE|PROT_EXEC)) != 0)) {

    log("MAP_FIXED, MAP_ANONYMOUS, PROT_WRITE and PROT_EXEC make no sense for RO TARFS\r\n");
    errno = EINVAL; 
    return -1;
  }

  /* For RO filesystem MAP_SHARED equals MAP_PRIVATE. ESP32's MMU only allows for shared entries
   * Obtain corresponding FS and FD pointers, add reference to the filesystem (done in ioctl()) 
   */
  if ((ioctl(fd, FIOGETFD, &io) >= 0)) {

    fs = req.fs;
    fp = &fs->fs_fds[req.fd];

    if (fp->fp_vaddr == NULL ||
        offset < 0 || 
        offset > fp->fp_size || 
        length > (fp->fp_size - offset)) {

      /* addref'ed in the ioctl() */

      log("failed: fp_vaddr=0x%x, offset=%d, fp_size=%u, length=%u\r\n",fp->fp_vaddr, offset, fp->fp_size, length);

      tarfs_unref(fs);
      if (errno == 0)
        errno = EBADF;

      return MAP_FAILED;
    }


    /* Partition is mmaped already by mount, here we just calculate the right 
     * memory offset
     */
    log("fd=%d, mapped %u bytes vaddr=0x%x, offset=0x%x\r\n",fd, fp->fp_vaddr, offset);
    return (void *)(fp->fp_vaddr + offset);
  }
  if (errno == 0)
    errno = EBADF;

  log("fd=%d, ioctl(FIOGETFD) failed\r\n",fd);
  
  return MAP_FAILED;
}


/**
 * POSIX munmap().
 * 
 */
int munmap(void *addr, size_t length) {

  struct tarfs_fs *fs;
  uintptr_t vaddr = (uintptr_t )addr;

  if (addr != NULL && length > 0) {
    tarfs_lock();
    for (int i = 0; i < TARFS_MAX_FS; i++) {

      fs = &s_tarfs[i];

      if (fs != NULL && 
          vaddr >= fs->fs_vaddr &&
          vaddr < (fs->fs_vaddr + fs->fs_size)) {
        tarfs_unlock();
        tarfs_unref(fs);
        return 0;
      }
    }

    tarfs_unlock();
  }

  errno = EINVAL;
  return -1;
}
