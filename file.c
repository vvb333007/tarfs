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


/* Common functions prologue. Note that tarf_open() uses hardcoded version of PROLOGUE() macro 
 * whenever this macro is changed, then tarf_open() code should be changed accordingly. Right now
 * hardcoded version of PROLOGUE() macro is different from the macro in is_sanefd() check
 */

#define PROLOGUE( TYPE ) \
  struct tarfs_fs *fs = NULL; \
\
  int fs_idx = (intptr_t )ctx; \
\
  if (false == (fs_idx >= 0 && \
                fs_idx < TARFS_MAX_FS && \
                ((fs = tarfs_getfs(fs_idx)) != NULL))) { \
\
    log("bad filesystem context: %d, %p\r\n",fs_idx, fs); \
    errno = EIO; \
    return (TYPE)(-1); \
  } \
  if (!is_sanefd(fs, fd)) { \
    log("bad fd=%d, fs_idx=%d\r\n", fd, fs_idx); \
    errno = EBADF; \
    return (TYPE)(-1); \
  }


/*
 * Thread-safety notes
 * ===================
 *
 * Simultaneious open() + unmount() is not thread-safe:
 *
 *   Subject to a TOCTTOU race inherited from the ESP-IDF VFS.
 *   unmount() and open() may execute concurrently because the VFS passes
 *   a cached filesystem context that can become invalid before
 *   tarfs_addref() is attempted. Not much we can do here unless ESP-IDF
 *   get improved VFS
 *
 * close() is not thread safe: 
 *   closing a file descriptor may create an UB
 *   closing a descriptor while performing active read on it AND opening another descriptor afterwards
 *   may create a race: newly opened FD could be treated by running read() as the sane old descriptor
 *   resulting in wrong data being read
 */ 

#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/utime.h>
#include <assert.h>
#include <fcntl.h>
#include <time.h>

#include "config.h"
#include "os.h"
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
static bool is_sanefd(struct tarfs_fs *fs, int fd) {

    return  (fd >= 0) &&
            (fd < TARFS_MAX_FDS) &&
            ((atomic_load_explicit(&fs->fs_usedfd, memory_order_relaxed) & (1u << fd)) != 0);
}

/* File operation handlers. These are called by VFS (on systems with VFS) or directly
 * When called directly it is user resposibility to pass the FS context as the first
 * argument. The FS context is a number (not a pointer, don't be confused by void *) 
 * in the range [0..TARFS_MAX_FS). This number is returned by tarfs_mount()
 */



/**
 * Open a TARFS file or directory.
 *
 * Opens an existing TAR archive entry and returns a TARFS file descriptor.
 * Both regular files and directories may be opened.
 */
int tarf_open(void* ctx, const char * path0, int flags, int mode) {

  tart_t type;
  size_t size;
  time_t mtime;
  int    fd = 0;
  const char *path = path0;

  
  struct tarfs_fs *fs = NULL; 

  int fs_idx = (intptr_t )ctx; 

  if (false == (fs_idx >= 0 && 
                fs_idx < TARFS_MAX_FS && 
                ((fs = tarfs_getfs(fs_idx)) != NULL))) { 

    log("bad filesystem context: %d, %p\r\n",fs_idx, fs); 
    errno = EIO;
    return -1; 
  } 

  if (((flags &  O_ACCMODE) == O_WRONLY) || (flags &  O_TRUNC)) {

      log("file %s bad flags %08x, read-only filesystem!\r\n",path, flags);
      errno = EROFS;
      return -1;
  }


  /* add a reference to the filesystem. this will ensure that context remain alive until tarf_close() */
  if (path && tarfs_addref(fs)) {

    
    /* If directory is opened - fix up the directory name
     * Internally , 'directory' and 'directory/' are two different paths so we have
     * to take this into account
     */
    if (flags & O_DIRECTORY) {
      log("O_DIRECTORY, path='%s'\r\n", path);
      int i = strlen(path);
      if (i > 0) {
        if (path[i - 1] != '/') {
          log("O_DIRECTORY, appending a slash (strdup)\r\n");
          char *p = tar_strdup1(path, NULL); /* NUL+NUL terminated string */
          if (p != NULL) {
            p[i] = '/';
            path = (const char *)p;
            log("path has been changed to '%s'\r\n", path);
          }
        }
      }
    }

    /* Find corresponding inode */
    int idx = inode_lookup(fs->fs_ino, fs->fs_nino,path);

    if (idx < 0) {
      log("file %s not found\r\n",path);
      /* errno may be set by inode_lookup or may be not */
      if (errno == 0)
        errno = ENOENT;
      goto unref_and_exit;
    }

    struct tarfs_inode const *inode = fs->fs_ino[idx];

    /* bad inode */
    if (inode->in_dvaddr == 0) {

      log("inode '%s' was found but has NULL DVADDR\r\n", path);
      errno = EIO;
      goto unref_and_exit;
    }


    /* Determine object type and size. */
    type = inode_getinfo(fs->fs_ino, idx, &size, &mtime);

    /* O_DIRECTORY requires the target to be a directory. */
    if ((flags & O_DIRECTORY) && type != TART_DIR) {
      log("O_DIRECTORY specified for a non-directory object\r\n");
      errno = ENOTDIR;
      goto unref_and_exit;
    }

    /* Allocate a file descriptor. It is atomic bitmap but we do not use publish/consume semantics.
     * Instead we rely on that fact that fd becomes available only upon open() return, so publishing is done by
     * function return
     */
    if ((fd = allocfd(fs)) >= 0) {

      struct tarfs_fp *fp = &fs->fs_fd[fd];
      log("fd=%d allocated\r\n", fd);

      /* Set size to 0 for directories. It must be zero by default but just to be sure. Th reason for that
       * is that read() must fail when reading any fd associated with a directory
       */
      if (type == TART_DIR) {

        log("size forced to 0 (it is a directory)\r\n");
        size = 0;
      }

      fp->fp_vaddr = inode->in_dvaddr + sizeof(struct tarhdr);
      fp->fp_pos   = 0;
      fp->fp_size  = size;
      fp->fp_idx   = idx;

      log("success, ino=%d, type=%c, path=%s, fd=%d, size=%lu, vaddr=%p\r\n",idx, type, path, fd,fp->fp_size, (void *)fp->fp_vaddr);
      errno = 0;

      /* free strduped path */
      if (path != path0) {
        log("free strduped path\r\n");
        tarfs_os_free((void *)path);
      }

      return fd;
    }
    log("allocfd failed for path=%s\r\n",path);
    errno = EMFILE;
  } else {
    log("%s://%s, bad path or dead object\r\n",fs->fs_mountpoint, path0);
    errno = (path == NULL) ? EINVAL : ENODEV;
  }

unref_and_exit:

  /* free strduped path */
  if (path != path0) {
    log("free strduped path\r\n");
    tarfs_os_free((void *)path);
  }

  tarfs_unref(fs);
  return -1;
}

/* close()
 * FS has at least 1 extra ref, because of open()
 *
 */
int tarf_close(void* ctx, int fd) {

  
  PROLOGUE( int );

  log("closing fd=%d, fs_idx=%d\r\n", fd, fs_idx);

  freefd(fs, fd);
  tarfs_unref(fs);

  return 0;
}


/**
 * Write data to a TARFS file.
 *
 * TARFS is a read-only filesystem and does not support write operations.
 * This function always fails.
 */
ssize_t tarf_write(void* ctx, int fd, const void * data, size_t size) {

  
  PROLOGUE( ssize_t );

  errno = EROFS;

  return (ssize_t)(-1);    
}

/**
 * Read data from an open TARFS file.
 */
ssize_t tarf_read(void* ctx, int fd, void *dst, size_t size) {

  
  PROLOGUE( ssize_t );

  struct tarfs_fp *fp = &fs->fs_fd[fd];

  /* Compute the source address: file_base + current_position.
   * In case of EOF this address will point 1 byte past the buffer. This is allowed by the C standart
   * as long as we do not dereference that pointer
   */
  void const *src = (void *)(fp->fp_vaddr + fp->fp_pos);

  /* Clamp the read size so we never read past the end of the file.
   * `size` can become zero after the clamp (happens for directories for example), it is normal
   */
  if (fp->fp_size - fp->fp_pos < size)
    size = fp->fp_size - fp->fp_pos;

  /*
   * Advance the file position.
   */
  fp->fp_pos += size;

  /* Copy the requested data.
   * TODO: Use an architecture-optimized memcpy() where available (e.g. ESP32-S3, P4, etc.).
   */
  if (size > 0)
    memcpy(dst, src, size);

  /* Return number of data copied */
  return size;
}

/**
 * Read data from an open TARFS file at a specified offset.
 * Copies up to @p size bytes starting at @p offset into @p dst.
 * Unlike read(), this function does not modify the current file position.
 */
ssize_t tarf_pread(void* ctx, int fd, void *dst, size_t size, off_t offset) {

  
  PROLOGUE( ssize_t );

  struct tarfs_fp *fp = &fs->fs_fd[fd];

  if (offset <0) {
    errno = EINVAL;
    return -1;
  }

  size_t off = (size_t)offset;

  if (fp->fp_size <= off)
    return 0;


  /* Compute the source address: file_base + requested offset.
   * In case of EOF this address will point one byte past the file end.
   * This is permitted by the C standard as long as the pointer is not
   * dereferenced.
   */

  void const *src = (void *)(fp->fp_vaddr + off);

  /* Clamp the read size so we never read past the end of the file.
   * `size` can become zero after the clamp (happens for directories for example), it is normal
   */
  if (fp->fp_size - off < size)
    size = fp->fp_size - off;

  /* Copy the requested data.
   * TODO: Use an architecture-optimized memcpy() where available
   * (e.g. ESP32-S3, P4, etc.).
   */
  if (size > 0)
    memcpy(dst, src, size);

  /* Return number of data copied */
  return size;
}

/**
 * lseek()
 *
 */
off_t tarf_lseek(void* ctx, int fd, off_t offset, int whence) {

  
  PROLOGUE( off_t );

  struct tarfs_fp *fp = (struct tarfs_fp *)(&fs->fs_fd[fd]);

  /* Check if lseek() results in a valid fp_pos:
   * it is different from POSIX which allows lseek() to set pos beyond the file end.
   * We allow pointer to be set to the first byte after the file end. In this case
   * lseek(fd, 0, SEEK_END) will position the pointer, while read() will return 0;
   */

  if (whence == SEEK_SET) {

    if (offset < 0 || offset > fp->fp_size) {
return_einval:
      errno = EINVAL;
      return (off_t)(-1);
    }
    fp->fp_pos = offset;
    return offset;

  } else if (whence == SEEK_END) {

    /* Unlike POSIX, TARFS never allows the file position to move past EOF.
     * The only valid position at the end of the file is exactly EOF:
     *
     *     lseek(fd, 0, SEEK_END)
     *
     * After positioning at EOF, subsequent read() calls return 0.
     */
    if (offset > 0 || offset < -fp->fp_size)
      goto return_einval;

    fp->fp_pos = (off_t)fp->fp_size + offset; /* offset is <= 0*/

  } else if (whence == SEEK_CUR) {

    off_t new_offset = offset + (off_t)fp->fp_pos;
    if (new_offset < 0 || new_offset > fp->fp_size)
      goto return_einval;

    fp->fp_pos = new_offset;
  } else
    goto return_einval;

  return fp->fp_pos;
}

/**
 * Get file status information.
 *
 */
int tarf_fstat(void* ctx, int fd, struct stat * st) {

  PROLOGUE( int ); /* defines fs_idx */

  tart_t type;
  time_t mtime;

  struct tarfs_fp *fp = &fs->fs_fd[fd];

  type = inode_getinfo(fs->fs_ino, fp->fp_idx, NULL, &mtime);

  memset(st, 0, sizeof(struct stat));

  uint32_t perm = S_IRUSR|S_IRGRP|S_IROTH; /* default read-only permissions */
        
  if (type == TART_DIR)
    perm |= S_IXUSR|S_IXGRP|S_IXOTH|S_IFDIR;
  else
    perm |= S_IFREG;

  st->st_dev = (dev_t)fs_idx;         /* Filesystem index [0..TARFS_MAX_FS) */
  st->st_ino = (ino_t)fp->fp_idx;     /* Inode index */
  st->st_blksize = 512;
  st->st_blocks =  ((fp->fp_size + 511) & ~511) / 512;
  st->st_size = fp->fp_size;
  st->st_mode = perm;
  st->st_mtime = fs->fs_mtime;
  st->st_atime = 0;
  st->st_ctime = fs->fs_mtime;

  return 0;
}

/**
 * Synchronize file contents.
 *
 */
int tarf_fsync(void* ctx, int fd) {
  
  PROLOGUE( int );

  return 0;
}

/*
 * Perform file descriptor control operations.
 *
 * Supports a minimal subset of POSIX @c fcntl() commands.
 * TARFS is inherently non-blocking, therefore @c F_SETFL is accepted
 * but ignored. @c F_GETFL always reports @c O_NONBLOCK.
 */
int tarf_fcntl(void *ctx, int fd, int cmd, int arg) {

  PROLOGUE( int );

  switch (cmd) {

    case F_GETFL:
      return O_NONBLOCK;

    case F_SETFL:
      return 0;

    default:
      errno = ENOSYS;
  };
  return -1;
}

/**
 * Perform TARFS-specific I/O control operations.
 *
 */
int tarf_ioctl(void *ctx, int fd, int cmd, va_list args) {

  
  PROLOGUE( int );

  switch (cmd) {

    /* Bytes left == File_Size - File_Current_Position */
    case FIONREAD:
      int *o = va_arg(args, int *);
      if (o == NULL) {
        errno = EINVAL;
        return -1;
      }
      *o = fs->fs_fd[fd].fp_size - fs->fs_fd[fd].fp_pos;
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
          out->fs_idx = fs_idx;
          return 0;
        }
        errno = ENODEV;
        return -1;
      }
      errno = EINVAL;
      return -1;

    default:
      break;
  };
  errno = ENOSYS;
  return -1;
}


/**
 * stat() system call
 */
int tarf_stat(void* ctx, const char * path, struct stat * st) {

    int fd;


    if ((fd = tarf_open(ctx, path, O_RDONLY, 0)) < 0) {
      if ((fd = tarf_open(ctx, path, O_RDONLY | O_DIRECTORY, 0)) < 0) {
        log("path does not exist (not file not directory)");
        return -1;
      }
    }

    int rc = tarf_fstat(ctx, fd, st);

    tarf_close(ctx, fd);

    return rc;
}


/**
 * Query or update file timestamps.
 *
 * TARFS is a read-only filesystem and does not support changing file
 * timestamps. If @p times is non-NULL, the structure is filled with the
 * current access time and the file modification time stored in the TAR
 * archive. No filesystem metadata is modified.
 *
 * @param ctx    Filesystem context.
 * @param path   Path to the file (currently unused).
 * @param times  Pointer to a structure to receive timestamps.
 *
 * @return 0 on success, or -1 if @p times is NULL.
 *
 * @retval EINVAL  @p times is NULL.
 *
 * @note TARFS does not store file access times. Therefore the returned
 *       access time is set to the current system time.
 */
int tarf_utime(void *ctx, const char *path, struct utimbuf *times) {

  if (times != NULL) {

    times->actime = time(NULL);  /* Access time; We do not keep atime in inodes, so what else we can do? */
    times->modtime = tarfs_getmtime((int)(uintptr_t)ctx);

    return 0;
  }
  errno = EINVAL;
  return -1;
}


/**
 * Truncate a file to the specified length.
 *
 * TARFS is a read-only filesystem and does not support modifying files.
 * This function always fails with ::EROFS.
 *
 * @param ctx     Filesystem context.
 * @param path    Path to the file.
 * @param length  Requested file length.
 *
 * @return Always -1.
 *
 * @retval EROFS  Filesystem is read-only.
 */
int tarf_truncate(void *ctx, const char *path, off_t length) {

  log("read-only file system\r\n");
  errno = EROFS;

  return -1;
}

/**
 * Truncate an open file to the specified length.
 *
 * TARFS is a read-only filesystem and does not support modifying files.
 * This function always fails with ::EROFS.
 *
 * @param ctx     Filesystem context.
 * @param fd      File descriptor.
 * @param length  Requested file length.
 *
 * @return Always -1.
 *
 * @retval EROFS  Filesystem is read-only.
 */
int tarf_ftruncate(void* ctx, int fd, off_t length) {

  log("read-only file system\r\n");
  errno = EROFS;

  return -1;
}

/**
 * Create a hard link.
 *
 * TARFS is a read-only filesystem and does not support creating links.
 * This function always fails with ::EROFS.
 *
 * @param ctx  Filesystem context.
 * @param n1   Existing pathname.
 * @param n2   New pathname.
 *
 * @return Always -1.
 *
 * @retval EROFS  Filesystem is read-only.
 */
int tarf_link(void* ctx, const char* n1, const char* n2) {

  log("read-only file system\r\n");
  errno = EROFS;

  return -1;
}

/**
 * Remove a directory entry.
 *
 * TARFS is a read-only filesystem and does not support deleting files.
 * This function always fails with ::EROFS.
 *
 * @param ctx   Filesystem context.
 * @param path  Path to the file.
 *
 * @return Always -1.
 *
 * @retval EROFS  Filesystem is read-only.
 */
int tarf_unlink(void* ctx, const char *path) {

  log("read-only file system\r\n");
  errno = EROFS;

  return -1;
}

/**
 * Rename or move a file.
 *
 * TARFS is a read-only filesystem and does not support renaming files.
 * This function always fails with ::EROFS.
 *
 * @param ctx  Filesystem context.
 * @param src  Source pathname.
 * @param dst  Destination pathname.
 *
 * @return Always -1.
 *
 * @retval EROFS  Filesystem is read-only.
 */
int tarf_rename(void* ctx, const char *src, const char *dst) {

  log("read-only file system\r\n");
  errno = EROFS;

  return -1;
}

