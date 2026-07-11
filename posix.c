/*
 * TARFS - Immutable (read-only) filesystem for embedded systems.
 *
 * Copyright (c) 2026 Viacheslav Logunov
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
#include "dir.h"
#include "posix.h"



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
   */
  if ((ioctl(fd, FIOGETFD, &io) >= 0)) {

    fs = tarfs_getfs_addref(io.fs_idx);
    if (fs == NULL) {
      log("filesystem gone\r\n");
      return MAP_FAILED;
    }

    fp = &fs->fs_fd[io.fd];

    if (fp->fp_vaddr == 0 ||
        offset < 0 || 
        offset > fp->fp_size || 
        length > (fp->fp_size - offset)) {

      tarfs_unref(fs);

      log("failed: offset=%ld, fp_size=%lu, length=%lu\r\n", offset, fp->fp_size, length);
      
      if (errno == 0)
        errno = EINVAL;

      return MAP_FAILED;
    }
    


    /* Partition is mmaped already by mount, here we just calculate the right 
     * memory offset
     */
    log("fd=%d, mapped %lu bytes vaddr=%p, offset=%ld\r\n",fd, length, (void *)fp->fp_vaddr, offset);
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

#if CONFIG_TARFS_HAVE_FDOPENDIR

/**
 * @brief Associate an open directory file descriptor with a directory stream.
 *
 * Creates a directory stream from an existing directory file descriptor.
 * After a successful call, the file descriptor is owned by the returned
 * directory stream and must not be closed directly. It will be closed
 * automatically by tard_closedir().
 */
DIR *fdopendir(int fd) {

  
  struct ioctl_req io;

  /* Convert our global fd to local fd number so tarf_ and tard_ functions can be used 
   * FIOGETFD returns the FS index and local fd 
   */
  if ((ioctl(fd, FIOGETFD, &io) >= 0))
    return tard_fdopendir((void *)(uintptr_t)io.fs_idx, io.fd);

  errno = ENOSYS;
  return NULL;
}
#endif



#if CONFIG_TARFS_HAVE_READLINK
/**
 * TODO: Implement
 * readlink() relies on a mount-time link table: when image is mounted, the inode_resolve() function,
 * which does all kind of link resolution, also builds a "link index"
 *
 */
ssize_t readlink(const char *path, char *buf, size_t bufsiz) {

  errno = ENOTSUP;
  return (ssize_t)(-1);
}
#endif

#if CONFIG_TARFS_HAVE_DIRFD
/**
 * TODO: how we can convert local_fd to global_fd?!
 */
int dirfd(DIR *dir) {
  errno = ENOTSUP;
  return -1;
}
#endif
