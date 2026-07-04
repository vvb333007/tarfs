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


#pragma once

#include <stdlib.h>
#include <stdatomic.h>
#include <stddef.h>
#include <errno.h>
#include <time.h>

#include "config.h"
#include "os.h"

#define TARFS_MAX_FS  CONFIG_TARFS_MAX_FS
#define TARFS_MAX_FDS CONFIG_TARFS_MAX_FDS

#include "inode.h"
#include "file.h"
#include "refc.h"


/* Set errno and return 0 or -1, depending on the errno */
static inline int $(int err) {
  return (errno = err) == 0 ? 0 : -1;
}

#define log( Format_, ... ) printf( "%s(): " Format_, __FUNCTION__,  ##__VA_ARGS__ )

//#include "tar.h"
#include "refc.h"
#include "file.h"
#include "inode.h"


/**
 * @brief ioctl() commands which are supported by this driver.
 *
 * @note  TARFS supports one special ioctl (FIOGETFD) to convert system-wide FD numbers to
 *        local fd numbers. This is required for POSIX mmap() implementation
 * 
 */
#define TARFS_IOCTL_BASE  0x54415200 /* "TAR\0" */

#ifndef FIOGETFD
#  define FIOGETFD (TARFS_IOCTL_BASE + 0)     /*!< Convert global fd number to local fd number. arg=&int */
#endif
#ifndef FIONREAD
#  define FIONREAD (TARFS_IOCTL_BASE + 1)   /*!< Support FIONREAD (compatibility layer) */
#endif
#ifndef FIONBIO
#  define FIONBIO (TARFS_IOCTL_BASE + 2)    /*!< Support FIONBIO (compatibility layer) */
#endif

/** 
 * Return/Request argument for FIOGETFD ioctl 
 */
struct ioctl_req {
  struct tarfs_fs *fs;
  int              fd;
};

/**
 * @brief POSIX mmap()/munmap() support for TARFS
 */
#define PROT_READ     1 /*!< Supported */
#define PROT_WRITE    2 /*!< Ignored, if used together with PROT_READ. Alone causes mmap() error (RO filesystem!) */
#define PROT_EXEC     4 /*!< Ignored */

#define MAP_SHARED    1 /*!< Supported */
#define MAP_PRIVATE   2 /*!< Supported */

#define MAP_FIXED     4 /*!< Ignored, no legit use for this flag :) */
#define MAP_ANONYMOUS 8 /*!< Unsupported, use malloc() instead */
#define MAP_ANON      8 

#define MAP_FAILED   ((void *)(-1))





/**
 * This descriptor holds all file descriptors opened. Created by tarfs_mount()
 * destroyed by a refcounter mechanism

 * @brief TARFS filesystem descriptor.
 *
 * @note Descriptor lifetime is defined by its refcounter fs_ref. Once it reaches zero (upon unrefx() call)
 *       the descriptor gets destroyed, memory is freed. These structures are managed by addref() and unrefx()
 *       calls only.
 */
struct tarfs_fs {

  refc_t                        fs_ref;               /*!< Reference counter: open files and active mmap() */
  uintptr_t                     fs_handle;            /*!< OS-specific opaque handle for the whole partition mmap */
  void const                   *fs_vaddr;             /*!< Virtual address to access whole partition */
  size_t                        fs_dsize;             /*!< Data size: Partition size minus size of all TARFS headers */
  size_t                        fs_size;              /*!< Total size: Partition size */
  uint32_t                      fs_nino;              /*!< Number of entries in fs_ino array */
  struct tarfs_inode const * const *fs_ino;           /*!< Sorted (in_hash) array of pointers to inodes. */
  struct tarfs_inode const *    fs_root;              /*!< First record of alphasorted (in_path) list of entries (linked via in_next) */
  _Atomic(uint32_t)             fs_usedfd;            /*!< A bitmask for fs_fd[] array: 0100 means FD #2 is 
                                                           used. Indicates which elements of fs_fd[] array are used */
  struct tarfs_fp               fs_fd[TARFS_MAX_FDS]; /*!< Open files descriptors */
  time_t                        fs_mtime;             /*!< time() at mount. Used by stat()/fstat() to populate mtime field (mtime does not change on ROFS)>*/
  char                          fs_mountpoint[0];     /*!< Mount point */
};

static inline int tarfs_addref(struct tarfs_fs *fs) {
  return fs == NULL ? 0 : addref(&fs->fs_ref);
}

int tarfs_unref(struct tarfs_fs *fs);

void tarfs_lock();
void tarfs_unlock();
int  tarfs_mount(const char *label, const char *mountpoint);
int  tarfs_unmount(const char *mountpoint);
int  tarfs_fsck(const char *label);


