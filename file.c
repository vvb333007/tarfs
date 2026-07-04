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

#include "tar.h"
#include "fs.h"
#include "refc.h"

#define $$( Errno_ ) ({ errno = (Errno_); (void *)NULL; })

/* Compile-time sanity checks */
_Static_assert(TARFS_MAX_FDS > 0 && TARFS_MAX_FDS <= 32);


static const uint32_t    s_valid_mask   = (TARFS_MAX_FDS == 32) ? 0xffffffffUL : ((1UL << TARFS_MAX_FDS) - 1UL);



/* Bitmap is used only for fd allocation. It is NOT used as a publication mechanism.
 * Descriptor contents are initialized before open() returns and become visible through
 * the VFS call chain. Therefore memory_order_relaxed is sufficient. Period.
 */


/**
 * This function must be called under addref() protocol:
 * if (addref(&fs->fs_ref)) {
 *   allocfd(fs);
 * }
 * Returns free index in range [0..TARFS_MAX_FDS-1]
 * Returns -1 if no free indices are available
 */
static int allocfd(struct tarfs_fs *fs) {

  int index;
  uint32_t free_mask;
  uint32_t used_indices, new_value;
    
  do {
    used_indices = atomic_load_explicit(&fs->fs_usedfd, memory_order_relaxed);
    free_mask = ~used_indices & s_valid_mask;

    if (free_mask == 0)
      return -1;

    index = __builtin_ctz(free_mask);
    new_value = used_indices | (1u << index);

  } while (!atomic_compare_exchange_weak_explicit( 
              &fs->fs_usedfd,
              &used_indices,
              new_value,
              memory_order_relaxed,
              memory_order_relaxed));

  return index;
}

/**
 * Marks previously allocated index as free. Function is tolerant to double free()
*/
static void freefd(struct tarfs_fs *fs, int index) {

    uint32_t used_indices, new_value;

    assert(index >= 0);
    assert(index < TARFS_MAX_FDS);

    do {
      used_indices = atomic_load_explicit(&fs->fs_usedfd, memory_order_relaxed);
      new_value = used_indices & ~(1u << index);

    } while (!atomic_compare_exchange_weak_explicit( 
              &fs->fs_usedfd,
              &used_indices,
              new_value,
              memory_order_relaxed,
              memory_order_relaxed));
}


/**
 * Extra paranoia: we check fds which are passed to us by VFS layer
 * for being in our range [0 .. TARFS_MAX_FDS]. Check if that fd is alive (file is opened)
 */
static inline bool is_sanefd(struct tarfs_fs *fs, int fd) {

    return  (fd >= 0) &&
            (fd < TARFS_MAX_FDS) &&
            ((atomic_load_explicit(&fs->fs_usedfd, memory_order_relaxed) & (1u << fd)) != 0);
}

/* File operation handlers. These are called by VFS
 *
 */


/* close()
 *
 */
static int tarf_close(void* ctx, int fd) {

  struct tarfs_fs *fs = (struct tarfs_fs *)ctx;

  if (!is_sanefd(fs, fd)) {
    log("bad fd=%d\r\n", fd);
    return $(EBADF);
  }

  freefd(fs, fd);
  log("closed fd=%d\r\n", fd);
  tarfs_unref(fs);
  return $(0);
}

/* open()
 *
 */
static int tarf_open(void* ctx, const char * path, int flags, int mode) {

  int fd = 0;
  struct tarfs_fs *fs = (struct tarfs_fs *)ctx;

  assert(path);

  /* XXX:
   * TOCTTOU inherited from ESP-IDF VFS (unmount() and open() race).
   * ESP32: The VFS passes a cached filesystem context that may become invalid
   *        before tarfs_addref() is attempted.
   */
  if (tarfs_addref(fs)) {

    int idx = inode_lookup(fs->fs_ino, fs->fs_nino,path);

    if (idx < 0) {
      log("file %s not found\r\n",path);
      /* errno may be set by inode_lookup or may be not */
      if (errno == 0)
        errno = ENOENT;
      goto unref_and_exit;
    }

    struct tarfs_inode const *inode = fs->fs_ino[idx];

    if (inode->in_dvaddr == 0) {

      errno = EIO;
      goto unref_and_exit;
    }

    if (((flags &  O_ACCMODE) == O_WRONLY) || (flags &  O_TRUNC)) {

      log("file %s bad flags %08x, read-only filesystem!\r\n",path, flags);
      errno = EROFS;
      goto unref_and_exit;
    }

    /* Allocate a file descriptor. It is atomic bitmap but we do not use publish/consume semantics.
     * Instead we rely on that fact that fd becomes available only upon open() return, so publishing is done by
     * function return
     */
    if ((fd = allocfd(fs)) >= 0) {

      struct tarfs_fp *fp = &fs->fs_fd[fd];

      fp->fp_vaddr = inode->in_dvaddr + sizeof(struct tarhdr);
      fp->fp_pos   = 0;
      fp->fp_size  = inode_size(inode);

      log("success, file=%s, fd=%d, size=%lu\r\n",path, fd,fp->fp_size);
      errno = 0;
      return fd;
    }
    log("allocfd failed for mp=%s\r\n",fs->fs_mountpoint);
    errno = EMFILE;
  } else {
    log("mp=%s addref() failed\r\n",fs->fs_mountpoint);
    errno = ENODEV;
  }

unref_and_exit:

  tarfs_unref(fs);
  return -1;
}

#if 0
//
//
//
static ssize_t tarf_write(void* ctx, int fd, const void * data, size_t size) {

  if (!allocated(fd))
    return $(EBADF);

  return $(EROFS);
}

static ssize_t tarf_read(void* ctx, int fd, void * dst, size_t size) {

  if (!allocated(fd))
    return $(EBADF);

    struct tarfs_fs * efs = (struct tarfs_fs *)ctx;
    ssize_t res = TARFS_read(efs->fs, fd, dst, size);
    if (res < 0) {
        errno = tarfs_res_to_errno(TARFS_errno(efs->fs));
        TARFS_clearerr(efs->fs);
        return -1;
    }
    return res;
}


static off_t tarf_lseek(void* ctx, int fd, off_t offset, int mode) {

    struct tarfs_fs * efs = (struct tarfs_fs *)ctx;
  if (!allocated(fd))
    return $(EBADF);

    off_t res = TARFS_lseek(efs->fs, fd, offset, mode);
    if (res < 0) {
        errno = tarf_res_to_errno(TARFS_errno(efs->fs));
        TARFS_clearerr(efs->fs);
        return -1;
    }
    return res;
}

static int tarf_fstat(void* ctx, int fd, struct stat * st) {

    assert(st);
    tarfs_stat s;
    struct tarfs_fs * efs = (struct tarfs_fs *)ctx;
  if (!allocated(fd))
    return $(EBADF);

    off_t res = TARFS_fstat(efs->fs, fd, &s);
    if (res < 0) {
        errno = tarf_res_to_errno(TARFS_errno(efs->fs));
        TARFS_clearerr(efs->fs);
        return -1;
    }
    memset(st, 0, sizeof(*st));
    st->st_size = s.size;
    st->st_mode = S_IRWXU | S_IRWXG | S_IRWXO | S_IFREG;
    st->st_mtime = tarf_get_mtime(&s);
    st->st_atime = 0;
    st->st_ctime = 0;
    return res;
}

static int tarf_fsync(void* ctx, int fd) {
  if (!allocated(fd))
    return $(EBADF);
  return $(0);
}

static int tarf_ioctl(void *ctx, int fd, int cmd, va_list args) {

  struct tarfs_fs *fs = (struct tarfs_fs *)ctx;

  if (!allocated(fd))
    return $(EBADF);

  switch (cmd) {

    /* Bytes left == File_Size - File_Current_Position */
    case FIONREAD:
      int *out = va_arg(args, int *);
      *out = fs->fs_fds[fd].fp_size - fs->fs_fds[fd].fp_pos;
      return 0;

    /* TARFS is non-blocking by design. Just ignore O_NONBLOCK */
    case FIONBIO:
      return 0;

    /* Return local FD number and the FS descriptor  pointer
     * caller must unrefx() this descriptor after he/she finishes with FS pointer
     */
    case FIOGETFD:

      struct ioctl_req *out = va_arg(args, struct ioctl_req *);

      if (out) {
        /* We are exporting a live pointer - must addref to keep it alive.
         * The caller is responsible for unrefx()!
         */
        if (tarfs_addref(fs)) {
          out->fd = fd;
          out->fs = fs;
          return 0;
        }
        return $(EPIPE);
      }
      return $(EINVAL);

    default:
    break;
  };

  return $(ENOSYS);
}


static int tarf_fcntl(void *ctx, int fd, int cmd, int arg) {

    switch (cmd) {
    default:
        return $(ENOSYS);
    }
}


void tarf_handlers_install(esp_vfs_fs_ops_t *files) {

  if (files) {
    files->write_p = &tarf_write,
    files->lseek_p = &tarf_lseek,
    files->read_p  = &tarf_read,
    files->open_p  = &tarf_open,
    files->close_p = &tarf_close,
    files->fstat_p = &tarf_fstat,
    files->fcntl_p = &tarf_fcntl,
    files->ioctl_p = &tarf_ioctl,
    files->fsync_p = &tarf_fsync,

  }
}
#endif
