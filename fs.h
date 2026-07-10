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
  int fs_idx; /*!< filesystem index, [0..TARFS_MAX_FS) */
  int fd; /*!< fd number */
};

#if CONFIG_HAVE_READLINK
struct tarfs_link {
  uint32_t    li_hash; /*!< hashed link name */
  const char *li_src;  /*!<        link name, for collision resolution */
  const char *li_dest; /*!< "points to" name (has to be free()ed, allocated by inode_populate() for inode_resolve()) */
};
#endif

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
#if CONFIG_HAVE_READLINK
  struct tarfs_link const      *fs_lin;               /*!< for readlink() */
  uint32_t                      fs_nlin;              /*!< Number of entries in fs_lin array */
#endif
  struct tarfs_fp               fs_fd[TARFS_MAX_FDS]; /*!< Open files descriptors */
  time_t                        fs_mtime;             /*!< time() at mount. Used by stat()/fstat() to populate mtime field (mtime does not change on ROFS)>*/
  char                          fs_mountpoint[0];     /*!< Mount point */
};

/**
 * Add 1 to the FS' reference count
 */
static inline int tarfs_addref(struct tarfs_fs *fs) {
  return fs == NULL ? 0 : addref(&fs->fs_ref);
}

/**
 * Substract 1. If counter reaches zero then  the FS destructor is called
 */
int tarfs_unref(struct tarfs_fs *fs);

/**
 * Lock access to s_tarfs[] table and to the s_numfs counter
 */
static inline void tarfs_lock() {
  tarfs_os_acquire_mutex();  
}

/**
 * Unlock access to s_tarfs[] table and to the s_numfs counter
 *  
 */
static inline void tarfs_unlock() {
  tarfs_os_release_mutex();  
}



/**
 * @brief Mount a TARFS filesystem from an OS-specific resource.
 *
 * The filesystem image is obtained using the OS abstraction layer function
 * tarfs_os_map_tarfile(). The `label` parameter is passed to this function
 * unchanged and is treated as an opaque identifier by TARFS.
 *
 * Depending on the platform, `label` may represent a partition name,
 * resource name, device identifier, or any other OS-specific object.
 *
 * The mount point is normally obtained from the TARFS image metadata.
 * It can be overridden explicitly by providing `mountpoint`.
 *
 * @param label
 *        Opaque resource identifier passed to tarfs_os_map_tarfile().
 *
 * @param mountpoint
 *        Optional mount point override. If NULL, the mount point stored
 *        in the TARFS image is used.
 *
 * @param link_rebase
 *        Optional path prefix used to rebase symbolic links.
 *
 * @return
 *        Filesystem slot index. The returned value can be passed to
 *        tarfs_getfs() to obtain the raw filesystem pointer.
 *
 * @note
 *        On platforms without a native VFS layer, the returned filesystem
 *        index can be used for direct TARFS access.
 */
int tarfs_mount(const char *label, const char *mountpoint, const char *link_rebase);


/**
 * @brief Mount a TARFS filesystem from an already mapped memory buffer.
 *
 * Unlike tarfs_mount(), this function does not use OS-specific resource
 * mapping. The caller provides a pointer to an existing TAR image and its
 * size directly.
 *
 * This mode is suitable for TARFS images stored as linked firmware resources,
 * embedded binary blobs, or any other memory region accessible by the
 * application.
 * @note
 *        TARFS does not copy the filesystem image. The supplied memory region
 *        must remain valid and unchanged while the filesystem is mounted.
 *
 * @param addr
 *        Pointer to the beginning of the TAR image.
 *
 * @param length
 *        Size of the TAR image in bytes.
 *
 * @param mountpoint
 *        Mount point assigned to the filesystem.
 *
 * @param link_rebase
 *        Optional path prefix used to rebase symbolic links.
 *
 * @return
 *        Filesystem slot index. The returned value can be passed to
 *        tarfs_getfs() to obtain the raw filesystem pointer.
 */
int tarfs_mount_memory(const void *addr, size_t length,
                       const char *mountpoint,
                       const char *link_rebase);
/**
 * Unmount tar file system
 * @return 0  on success
 *         <0 FS is scheduled for unmount, but unmount is delayed because FS has some active users
 */
int  tarfs_unmount(const char *mountpoint);

/**
 * Deeper filesystem check: verifies content integrity (if enabled), 
 *
 */
int  tarfs_fsck(const char *label);


/**
 * Obtain raw pointer to the filesystem descriptor by filesystem slot
 * (value returned by tarfs_mount())
 * Platforms without VFS support will require user to pass FS descriptor manually to
 * tarfs_read()/tarfs_open()/etc. Pointer returned by this function can not be stored
 */
struct tarfs_fs *tarfs_getfs(int i);

/**
 * @brief Find the filesystem responsible for a given path.
 *
 * Searches all mounted filesystems and returns the filesystem whose mount
 * point is the longest prefix of the supplied path. This allows nested mount
 * points to work correctly. For example, if both `/data` and `/data/logs`
 * are mounted, the path `/data/logs/app.txt` resolves to the latter.
 *
 * A mount point matches only if it forms a complete path component. For
 * example, `/foo` matches `/foo` and `/foo/bar`, but does not match
 * `/foobar`.
 *
 * @param path
 *        Absolute path to resolve.
 *
 * @return
 *        Filesystem slot index, or -1 if no mounted filesystem matches the
 *        specified path.
 */
int tarfs_fsindex(const char *path);


/**
 * Find the filesystem responsible for a given path. This function is NOT lockless
 *
 * @return
 *        Filesystem slot index, or -1 if no mounted filesystem matches the
 *        specified path.
 */
time_t tarfs_getmtime(int fs_idx);

/* Implementation of a calloc() and a strdup() via memory backend
 *
 */
void *tarfs_calloc(size_t count, size_t size);
char *tarfs_strdup(char const *str);
