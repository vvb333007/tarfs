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


/**
 * Subset of POSIX functions
 *
 */


#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/errno.h>
#include <sys/ioctl.h>

#include "config.h"
#include "os.h"
#include "fs.h"
#include "file.h"
#include "inode.h"
#include "tar.h"
#include "posix.h"



//ssize_t readlink(const char *path, char *buf, size_t bufsiz) {
//}

/**
 * Mimic POSIX mmap().
 *
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
    return MAP_FAILED;
  }


  /* The 'fd' argument is a global VFS file descriptor and must be translated
   * into the local file descriptor used by this filesystem. This is done via
   * ioctl(): the VFS resolves the global descriptor to its local counterpart,
   * allowing us to retrieve the local file descriptor from the returned data.
   *
   * Yes, the filesystem is asking the VFS to translate its own file descriptor.
   *
   * ioctl() also returns a pointer to the corresponding filesystem instance and
   * increments its reference count.
   */
  if ((ioctl(fd, FIOGETFD, &io) >= 0)) {

    fs = tarfs_getfs(io.fs_idx);
    fp = &fs->fs_fd[io.fd];

    if (fp->fp_vaddr == 0 ||
        offset < 0 || 
        offset > fp->fp_size || 
        length > (fp->fp_size - offset)) {

      /* addref'ed in the ioctl() */
      tarfs_unref(fs);

      log("failed: offset=%ld, fp_size=%lu, length=%lu\r\n", offset, fp->fp_size, length);
      
      if (errno == 0)
        errno = EBADF;

      return MAP_FAILED;
    }
    


    /* Partition is mmaped already by mount, here we just calculate the right 
     * memory offset
     */
    log("fd=%d, mapped %lu bytes vaddr=%px, offset=%ld\r\n",fd, length, (void *)fp->fp_vaddr, offset);
    return (void *)(fp->fp_vaddr + offset);
  }
  if (errno == 0)
    errno = EBADF;

  log("fd=%d, ioctl(FIOGETFD) failed or is not supported\r\n",fd);
  
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

      fs = tarfs_getfs(i);

      if (fs != NULL && 
          vaddr >= (uintptr_t)fs->fs_vaddr &&
          vaddr < ((uintptr_t)fs->fs_vaddr + fs->fs_size)) {

        tarfs_unlock();
        tarfs_unref(fs);
        log(" success\r\n");
        return 0;
      }
    }

    tarfs_unlock();
  }
  log(" address %p is not virtual mmap() address\r\n", addr);
  errno = EINVAL;
  return -1;
}
