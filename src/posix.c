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
#include <sys/socket.h>
#include <sys/types.h>


#include "config.h"
#include "os.h"
#include "fs.h"
#include "file.h"
#include "inode.h"
#include "tar.h"
#include "dir.h"
#include "posix.h"


#if CONFIG_TARFS_HAVE_MMAP
/**
 * Mimic POSIX mmap().
 *
 */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {

  struct ioctl_req io;

  /* FIOGETFD returns the FS index and local fd, suitable for tarf_ , tard_ functions */
  if ((ioctl(fd, FIOGETFD, &io) >= 0))
    return tarf_mmap((void *)(uintptr_t)io.fs_idx, addr, length, prot, flags, io.fd, offset);

  /*errno should be set by the ioctl() */
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
        return tarf_munmap((void *)(uintptr_t )i, addr, length);
      }
    }
  }
  log("address %p does not belong to mmap()\r\n", addr);
  errno = EINVAL;
  return -1;
}
#endif /* CONFIG_TARFS_HAVE_MMAP */



#if CONFIG_TARFS_HAVE_DUPFD
/**
 * Create an independent duplicate of a file descriptor.
 *
 * This function is similar to POSIX dup(), but the file position is
 * not shared. The new descriptor has its own independent file offset,
 * initialized to the current position of the original descriptor.
 */
int dupfd(int fd) {

  struct ioctl_req io;

  /* FIOGETFD returns the FS index and local fd, suitable for tarf_ , tard_ functions */
  if ((ioctl(fd, FIOGETFD, &io) >= 0))
    return tarf_dupfd((void *)(uintptr_t)io.fs_idx, io.fd);

  /*errno should be set by the ioctl() */
  return -1;
}
#endif /* CONFIG_TARFS_HAVE_DUPFD */


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
  if (ioctl(fd, FIOGETFD, &io) >= 0)
    return tard_fdopendir((void *)(uintptr_t)io.fs_idx, io.fd);

  return NULL;
}
#endif /* CONFIG_TARFS_HAVE_FDOPENDIR */



#if CONFIG_TARFS_HAVE_STATVFS
/**
 * Obtain filesystem statistics.
 *
 * Fills a POSIX statvfs structure with information about the mounted
 * filesystem. Since TARFS is a read-only filesystem, the number of
 * available blocks and inodes is always reported as zero.
 *
 * @param path  Pointer to the path
 * @param st  Pointer to the statvfs structure to fill.
 *
 * @return 0 on success, or -1 on error with errno set appropriately.
 */
int statvfs(const char *path, struct statvfs *st) {

  int fs_idx;

  if ((fs_idx = tarfs_fsindex(path)) >= 0)
    return tarfs_statvfs((void *)(intptr_t)fs_idx, st);
  
  errno = ENODEV;
  return -1;
}
#endif /* CONFIG_TARFS_HAVE_STATVFS */


#if CONFIG_TARFS_HAVE_SENDFILE

/**
 * Zero-overhead, no-buffer file-to-socket tranfer. File descriptor must be a TARFS file descriptor
 * 
 * This function transfers data directly from @p in_fd to @p out_fd without
 * requiring an intermediate user buffer, without read()/pread()
 *
 * @param out_fd  Destination file descriptor. Must be a socket.
 *
 * @param in_fd   Source file descriptor. A tarfs descriptor
 * @param offset  Optional starting offset in the input file. If NULL, the
 *                current file position is used and advanced. Otherwise,
 *                the value pointed to by @p offset is used and updated,
 *                while the file position of @p in_fd remains unchanged.
 * @param count   Maximum number of bytes to transfer.
 *
 * @return Number of bytes transferred on success, or -1 on error with errno set.
 *
 */
ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count) {

  struct ioctl_req io;

  /* Convert our global fd to local fd number so tarf_ and tard_ functions can be used 
   * FIOGETFD returns the FS index and local fd 
   */
  if (ioctl(in_fd, FIOGETFD, &io) >= 0)
    return tarf_sendfile((void *)(uintptr_t)io.fs_idx, out_fd, io.fd, offset, count);

  /*errno should be set by the ioctl() */
  return -1;
}
#endif /* #if CONFIG_TARFS_HAVE_SENDFILE */

