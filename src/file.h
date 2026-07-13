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

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include "config.h"
//#include "os.h"
//#include "fs.h"

/* Compile-time sanity checks. */
_Static_assert(TARFS_MAX_FDS > 0 && TARFS_MAX_FDS <= 32, "Code review is required");

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This internal file descriptor maps to a local fd number 
 * as descriptor_ptr = s_tarfs[slot]->fs_fds[local_fd];
 *
 * @brief TARFS file descriptor, 12 bytes
 *
 */
struct tarfs_fp {

  uintptr_t fp_vaddr;  /*!< file virtual address, absolute. zero indicates unused fd.  */
  uint32_t  fp_pos;    /*!< current file pointer position. can be set 1 byte past the file end, i.e. fp_pos = fp_size */
  size_t    fp_size;   /*!< file size in bytes. NOTE: If fp_size is 0, then fp_vaddr could be invalid and must not be read */
  int       fp_idx;    /*!< associated inode index */
};


/* VFS handlers below are normally invoked through a VFS.
 *
 * On systems without VFS support, applications should call the tarfs_(),
 * tarf_() and tard_() APIs directly. Unlike their POSIX counterparts, these
 * functions take an additional first argument: an index of the mounted
 * filesystem
 *
 * Typical usage without a VFS:
 *
 * 1. Mount the filesystem and save its identifier.
 *
 *      int ctx = tarfs_mount(...);
 *      if (ctx < 0)
 *          exit_error("failed to mount filesystem");
 *
 * 2. Use the context pointer in subsequent filesystem calls.
 *
 *      int fd1 = tarf_open((void *)ctx, "/home/file1.txt", flags);
 *      int fd2 = tarf_open((void *)ctx, "/home/file2.txt", flags);
 *      int fd3 = tarf_open((void *)ctx, "/home/file3.txt", flags);
 */




/* close()
 * FS has at least 1 extra ref, because of open()
 *
 * close() is not thread-safe. It is not as bad as it seems: no crashes, no sigsegv.
 * Just DO NOT close() files being read() at the same time by another thread. Or, if you do, 
 * do not open another file immediately :)
 *
 * This is because of:
 *
 * Closing a file descriptor while another thread is executing read()
 * (or any operation using the same descriptor) is undefined behaviour.
 *
 * Since file descriptor slots are immediately reusable, a concurrent
 * open() may repurpose the same slot for another file before the running
 * operation completes, causing it to access the newly opened file instead
 * of the original one.
 *
 */
int tarf_close(void* ctx, int fd);

/**
 * @brief Open a TARFS file or directory.
 *
 * Opens an existing TAR archive entry and returns a TARFS file descriptor.
 * Both regular files and directories may be opened. Directories cannot be
 * read using read(), but the returned descriptor may be used with functions
 * such as fstat(), fcntl(), ioctl(), close(), etc.
 *
 * TARFS is a read-only filesystem. Attempts to open an entry for writing or
 * with @c O_TRUNC fail with @c EROFS.
 *
 * @param ctx   Filesystem instance.
 * @param path  Absolute path within the mounted TARFS.
 * @param flags POSIX open() flags.
 * @param mode  Creation mode (ignored).
 *
 * @return Non-negative TARFS file descriptor on success.
 * @retval -1 Error. Possible @c errno values include:
 *         - @c EINVAL  Invalid path.
 *         - @c ENOENT  Entry not found.
 *         - @c EROFS   Write access requested on a read-only filesystem.
 *         - @c EMFILE  No free file descriptors.
 *         - @c ENODEV  Filesystem has been unmounted.
 *         - @c EIO     Corrupted inode.
 */
int tarf_open(void* ctx, const char * path, int flags, int mode);


/**
 * @brief Write data to a TARFS file.
 *
 * TARFS is a read-only filesystem and does not support write operations.
 * This function always fails.
 *
 * @param ctx  Filesystem instance.
 * @param fd   TARFS file descriptor.
 * @param data Data to write (unused).
 * @param size Number of bytes to write (unused).
 *
 * @retval -1 Always. Possible @c errno values are:
 *         - @c EBADF  Invalid file descriptor.
 *         - @c EROFS  Filesystem is read-only.
 */
ssize_t tarf_write(void* ctx, int fd, const void * data, size_t size);

/**
 * @brief Read data from an open TARFS file.
 *
 * Copies up to @p size bytes from the current file position into @p dst and
 * advances the file position by the number of bytes actually read.
 *
 * @return Number of bytes read, or -1 on error.
 */
ssize_t tarf_read(void* ctx, int fd, void *dst, size_t size);

/**
 * @brief Read data from an open TARFS file at a specified offset.
 *
 * Copies up to @p size bytes starting at @p offset into @p dst.
 * Unlike read(), this function does not modify the current file position.
 *
 * @return Number of bytes read, or -1 on error.
 */
ssize_t tarf_pread(void* ctx, int fd, void *dst, size_t size, off_t offset);

/**
 * @brief lseek()
 *
 * Copies up to @p size bytes from the current file position into @p dst and
 * advances the file position by the number of bytes actually read.
 *
 * @param ctx Filesystem instance.
 * @param fd  TARFS file descriptor.
 * @return Number of bytes read, or -1 on error.
 */
off_t tarf_lseek(void* ctx, int fd, off_t offset, int whence);

/**
 * @brief Get file status information.
 *
 * Fills a POSIX @ref stat structure for an open TARFS file descriptor.
 * Since TARFS is read-only, only read permission bits are reported.
 * Directory entries additionally have execute bits set to allow traversal.
 *
 * @param ctx Filesystem instance.
 * @param fd  TARFS file descriptor.
 * @param st  Output structure to receive file information.
 *
 * @retval 0   Success.
 * @retval -1  Invalid file descriptor (@c errno = EBADF).
 */
int tarf_fstat(void* ctx, int fd, struct stat * st);

/**
 * @brief Synchronize file contents.
 *
 * TARFS is a read-only filesystem, therefore there is nothing to flush.
 * This function validates the file descriptor and always succeeds.
 *
 * @param ctx Filesystem instance.
 * @param fd  TARFS file descriptor.
 *
 * @retval 0   Success.
 * @retval -1  Invalid file descriptor (@c errno = EBADF).
 */
int tarf_fsync(void* ctx, int fd);

/**
 * @brief Perform file descriptor control operations.
 *
 * Supports a minimal subset of POSIX @c fcntl() commands.
 * TARFS is inherently non-blocking, therefore @c F_SETFL is accepted
 * but ignored. @c F_GETFL always reports @c O_NONBLOCK.
 *
 * @param ctx Filesystem instance.
 * @param fd  TARFS file descriptor.
 * @param cmd Control command.
 * @param arg Command argument.
 *
 * @retval >=0 Command-dependent result.
 * @retval -1 Unsupported command (@c errno = ENOSYS).
 */
int tarf_fcntl(void *ctx, int fd, int cmd, int arg);

/**
 * @brief Perform TARFS-specific I/O control operations.
 *
 * Supported commands:
 * - @c FIONREAD  - returns the number of unread bytes remaining.
 * - @c FIONBIO   - accepted for compatibility and ignored.
 * - @c FIOGETFD  - exports the TARFS descriptor and local file descriptor.
 *
 * The @c FIOGETFD command increments the filesystem reference count before
 * returning the filesystem pointer. The caller is responsible for releasing
 * it with @c unrefx() when it is no longer needed.
 *
 * @param ctx  Filesystem instance.
 * @param fd   TARFS file descriptor.
 * @param cmd  I/O control command.
 * @param args Command-specific arguments.
 *
 * @retval 0   Success.
 * @retval -1  Error. Possible @c errno values include:
 *             - @c EBADF   Invalid file descriptor.
 *             - @c EINVAL  Invalid command argument.
 *             - @c ENODEV  Filesystem is no longer available.
 *             - @c ENOSYS  Unsupported ioctl command.
 */
int tarf_ioctl(void *ctx, int fd, int cmd, va_list args);

/**
 * stat() system call
 */
int tarf_stat(void* ctx, const char * path, struct stat * st);





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
int tarf_truncate(void *ctx, const char *path, off_t length);

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
int tarf_ftruncate(void* ctx, int fd, off_t length);

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
int tarf_link(void* ctx, const char* n1, const char* n2);

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
int tarf_unlink(void* ctx, const char *path);

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
int tarf_rename(void* ctx, const char *src, const char *dst);

#ifdef __cplusplus
};
#endif