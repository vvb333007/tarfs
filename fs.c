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


#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/errno.h>
#include <sys/fcntl.h>

#include "config.h"
#include "os.h"
#include "refc.h"
#include "fs.h"
#include "file.h"
#include "inode.h"



/**
 * Globals
 */
static _Atomic int       s_numfs = 0;                    /*!< Number of mounted TARFS filesystems */
static struct tarfs_fs  *s_tarfs[TARFS_MAX_FS] = { 0 };  /*!< Pointers to filesystem descriptors */



/**
 * Lockless, not thread safe, does not increase refcounters.
 * Must be only used when FS's refcounter is guaranteed > 1
 */
struct tarfs_fs *tarfs_getfs(int i) {
  if (i>=0 && i < TARFS_MAX_FS)
    return s_tarfs[i];
  log("filesystem #%d does not exist!\r\n",i);
  return NULL;
}

/**
 * Thread safe, increases refcounter, uses mutex!
 * Must be used when filesystem #i is in unknown state (e.g. is being unmounted)
 * Right now tarf_open() uses it, as well as POSIXs mmap() and readlink()
 *
 */
struct tarfs_fs *tarfs_getfs_addref(int i) {

  struct tarfs_fs *fs;

  tarfs_lock(); /* concurrent commit_unmount() fired */

  if (i>=0 && i < TARFS_MAX_FS) {
    fs = s_tarfs[i];
    if (!tarfs_addref(fs))
      fs = NULL;
  }
  else
    fs = NULL;

  tarfs_unlock();
  return fs;
}



/** Find an empty slot or mounted slot. Must be called under tarfs_lock()
 *
 * @param mountpoint If `mountpoint` is NULL, then this function returns index of first available unused slot
 *
 * @note Slots are indicies of tarfs_fs structures, each representing a mounted filesystem
 */
static int findfs(const char *mountpoint) {

  for (int i = 0; i < TARFS_MAX_FS; i++) {

    /* We are interested in empty slots only if mountpoint is NULL */
    if (s_tarfs[i] == NULL) {
      if (mountpoint == NULL) {
        log("empty slot #%d found\r\n", i);
        return i;
      }

      continue;
    }

    /* We are not interested in occupied slots if mountpoint is NULL */
    if (mountpoint == NULL)
      continue;

    /* Exact name match? Return slot number */
    if (!strcmp(s_tarfs[i]->fs_mountpoint, mountpoint)) {
      log("mountpoint %s slot #%d found\r\n", mountpoint, i);
      return i;
    }
  }

  /* Log, set errno and return -1 */

  if (mountpoint == NULL) {
    log("can not find empty slot\r\n");
    errno = EBUSY;
    return -1;
  }

  log("can not find slot (%s)\r\n", mountpoint);
  errno = ENOENT;
  return -1;
}


/**
 * Find the filesystem responsible for a given path. This function is NOT lockless
 * and it is not fast: it does bruteforce strcmp over the list of mountpoints.
 *
 * @return
 *        Filesystem slot index, or -1 if no mounted filesystem matches the
 *        specified path.
 */
int tarfs_fsindex(const char *path) {

    int best = -1;
    size_t best_len = 0;

    tarfs_lock();
    for (int i = 0; i < TARFS_MAX_FS; i++) {

        const char *mp = s_tarfs[i]->fs_mountpoint; /* can't be NULL, it is an inplace array */
        size_t len = strlen(mp);

        if (len <= best_len)
            continue;

        if (strncmp(path, mp, len) != 0)
            continue;

        /* Match exactly "/foo" or "/foo/..." */
        if (path[len] != '\0' &&
            path[len] != '/' &&
            mp[len - 1] != '/')
            continue;

        best = i;
        best_len = len;
    }
    tarfs_unlock();

    return best;
}

/**
 * Find the filesystem responsible for a given path. This function is NOT lockless
 *
 * @return
 *        Filesystem slot index, or -1 if no mounted filesystem matches the
 *        specified path.
 */
time_t tarfs_getmtime(int fs_idx) {

  time_t t = 0;

  tarfs_lock();

  if (s_tarfs[fs_idx] != NULL)
    t = s_tarfs[fs_idx]->fs_mtime;

  tarfs_unlock();

  return t;
}




/** Actual "unmount" procedure. Called by unref(). Finalizes unmount procedure, unmaps memory region,
 *  Thread safety is ensured by refcounting mechanism: the function below can not be called twice with the same
 *  argument, refcounter destructors are called only once, by design
 *
 *  Clears s_tarfs[] entry. This destructor is called by the last user of unmounted filesystem: if filesystem
 *  had no open files then this destructor is called from unmount() only. Otherwise it may be close(), closedir() etc
 *
 */
static void commit_unmount(void *ctx) {

  int i;
  struct tarfs_fs *fs = (struct tarfs_fs *)ctx;

  if (fs == NULL)
    return ;

  log(">>> unmounting FS '%s' started\r\n", fs->fs_mountpoint);


  log("unregistering VFS\r\n");

  /* If we can not properly unregister our FS we don't remove tarfs_fs entry
   * to prevent crashes: unregistered fs can still call read()/write()/open()
   */  
  if (!tarfs_os_unregister_fs(fs->fs_mountpoint)) {

    log(" failed to unregister '%s', memory leaked!\r\n", fs->fs_mountpoint);
    /* This will create a memory leak, but prevents crashes */
    return ;
  }

  /* Once FS is unregistered, no handlers with ctx==fs will be called. 
   * Now we can start data structures removal
   */
  /* Clear slot: pointer is going to become invalid
   */
  log("clearing slot..\r\n");
  tarfs_lock();
  for (i = 0; i< TARFS_MAX_FS; i++)
    if (s_tarfs[i] == fs) {
      s_numfs--;
      s_tarfs[i] = NULL;
      log("FS slot %d cleared\r\n", i);
      break;
    }
  tarfs_unlock();

  /* Release all associated memory: inodes, inode index*/
  if (fs->fs_ino != NULL && fs->fs_nino > 0) {

    log(" deleting inodes (%u)\r\n", fs->fs_nino);
    inode_unmount(fs, (const void *)fs->fs_vaddr, fs->fs_size);
    fs->fs_ino = NULL;
  }

  /* Release the FS image */
  if (fs->fs_vaddr != NULL) {
    if (fs->fs_handle != 0) {
      log(" unmapping filesystem image, vaddr=%p, size=%lu\r\n", fs->fs_vaddr, fs->fs_size);
      tarfs_os_unmap_tarfile((void *)fs->fs_handle, (void *)fs->fs_vaddr, fs->fs_size);
    }
    fs->fs_vaddr = NULL;
  }

  log("deleting FS descriptor %p\r\n", fs);
  tarfs_os_free(fs);
}

/*
 *
 */
int tarfs_addref(struct tarfs_fs *fs) {
  return fs == NULL ? 0 : addref(&fs->fs_ref);
}

/*
 *
 */
int tarfs_unref(struct tarfs_fs *fs) {
  return fs == NULL ? 0 : unrefx(&fs->fs_ref, fs, commit_unmount);
}

/** 
 * Unmounting a filesystem with a zero open files AND opening a file at the same time
 * MAY result in racecond which eventually lead to crash. This must be fixed at ESP-IDF VFS level.
 * To prevent this type of race completely do not unmount filesystems once mounted
 */
int tarfs_unmount(const char *mountpoint) {
  
  int slot,
      err = 0;

  struct tarfs_fs *fs;

  refc_type_t prev_refc;

  if (mountpoint == NULL) {
    errno = EINVAL;
    return -1;
  }

  tarfs_lock(); /* protect s_tarfs[] array, protects from a concurrent tarfs_unmount/mount */
  
  if ((slot = findfs(mountpoint)) >= 0) {

    fs = s_tarfs[slot];

    prev_refc = tarfs_unref(fs);

    if (prev_refc > 1) {
      log("Filesystem %s is in use (%u open fds), umount delayed\r\n", mountpoint, prev_refc - 1);
      err = EAGAIN;
    }

  } else
    err = ENOENT;

  tarfs_unlock();

  if (err != 0) {
    errno = err;
    return -1;
  }

  return 0;
}

/* Mount TARfile which is already mmaped/loaded into RAM
 *
 */
int tarfs_mount_memory(const void *map, size_t size, const char *mountpoint, const char *link_rebase, const char *path_rebase) {
  
  int len;
  

  int slot = -1;
  struct tarfs_fs *fs = NULL;

  char base_dir[128] = { '/' };

  log("Running filesystem check, wait..\r\n");
  int bad = tar_verify_crc(map,size, false);
  if (bad)
    log("FS checked: %d bad entries\r\n", bad);
  else
    log("Filesystem has no errors\r\n");

  if (link_rebase == NULL)
    link_rebase = "";

  if (path_rebase) {
    strncpy(&base_dir[1], path_rebase, sizeof(base_dir)-1);
  } else {
    if (false == tar_rootdir(map, size, &base_dir[1], sizeof(base_dir) - 1))
      base_dir[0] = '\0';
  }

  if (mountpoint == NULL)
    mountpoint = base_dir;
  else {
    log("detected MP '%s' is overriden with '%s'\r\n", base_dir, mountpoint);
  }

  len = strlen(mountpoint);



    if (false == (len > 1 && mountpoint[0] == '/' && mountpoint[len - 1] != '/')) {
      log("ERR: mountpoint is too short or invalid '%s'\r\n", mountpoint);
      errno = EINVAL;
unmap_and_return_error:
      
      return -1;
    }

    log("mountpoint: '%s'\r\n", mountpoint);

    if (base_dir[1] == '\0') {
      log("WARN: tarfile has no root directory\r\n");
    } else {
      log("common prefix: '%s' (will be stripped)\r\n", &base_dir[1]);
    }

    if (*link_rebase != '\0') {
      log("absolute path rewrite: '%s'\r\n",link_rebase);
    } else {
      log("preserving absolute paths in hardlinks/symlinks\r\n");
    }

    /* Allocate FS descriptor */
    if ( NULL == (fs = tarfs_calloc(1, sizeof(struct tarfs_fs) + len + 1))) {
      log("ERR: out of memory\r\n");
      errno = ENOMEM;
      goto unmap_and_return_error;
    }

    /* Allocate free FS slot */
    /* ------- locked -------*/
    tarfs_lock();
    if (0 > (slot = findfs( NULL ))) {
      tarfs_unlock();
      tarfs_os_free(fs);
      log("ERR: too many mounted filesystems (%u)\r\n",s_numfs);
      errno = EBUSY;
      goto unmap_and_return_error;
    }
  
    /* Occupy FS slot and release the lock ASAP */
    s_tarfs[slot] = fs;
    s_numfs++;
    tarfs_unlock();
  /* ------- unlocked -------*/

    log("allocated new FS descriptor s_tarfs[%d] = %p\r\n", slot, fs);

    /* set refcounter to 1 */
    initref(&fs->fs_ref);

    /* Store tarfile mapping parameters in the FS descriptor */
    fs->fs_vaddr  = map;
    fs->fs_handle = 0;     /* populated by tarfs_mount() if required */
    fs->fs_size   = size;

    /* Copy mountpoint. Trailing zero is there already */
    memcpy(fs->fs_mountpoint, mountpoint, len);

    log("mounting..\r\n");
    if (inode_mount(fs, map, size, link_rebase, &base_dir[1]) < 0) {
      if (fs->fs_ino == NULL) {
        log("ERR: filesystem is unusable, no valid inodes were found\r\n");
        tarfs_lock();
        s_tarfs[slot] = NULL;
        s_numfs--;
        tarfs_unlock();
        /* errno must be set in inode_mount() */
        goto unmap_and_return_error;
      } 
      log("WARN: running in degraded mode\r\n");
    }

  log("addr %p:%lu --> %u inodes successfully mounted\r\n", map, size, fs->fs_nino);

  /* Registering VFS */
  log("registering TARFS in VFS..\r\n");
  if (tarfs_os_register_fs(mountpoint, (void *)(intptr_t)slot) == false) {
    log("WARN: can not register POSIX handlers, only native tarfs API is available\r\n");
  } else {
    log("registered. (prefix '%s' in VFS)\r\n", mountpoint);
  }

  log("Congrats! Mount is done. Filesystem slot is %d\r\n", slot);
  return slot;
}


/**
 * Actual mount procedure
 * We expect sane label pointer (ASCIIZ) and a sane mountpoint (i.e. len>1, 
 * fisrt sym is `/`, last sym != `/`)
 */
int tarfs_mount(const char *label, const char *mountpoint, const char *link_rebase, const char *path_rebase) {

  int slot;
  size_t size;
  void const *map;
  void *os_handle;


  /* Actual memory mapping */
  log("loading OS-specific resource '%s'..\r\n", label);

  if (NULL != (map = tarfs_os_map_tarfile( label, &os_handle, &size))) {

    log("resource is available at %p, %lu bytes. mounting from memory..\r\n", map, size);

    slot = tarfs_mount_memory(map, size, mountpoint, link_rebase, path_rebase);
    if (slot >= 0) {

      log("success, filesystem slot %d was assigned\r\n", slot);

      struct tarfs_fs *fs = tarfs_getfs(slot);

      log("resource handle is %p, commit_unmount() will do tarfs_os_unmap_tarfile()\r\n", (void *)os_handle);

      fs->fs_handle = (uintptr_t)os_handle;

      return slot;
    }
    tarfs_os_unmap_tarfile(os_handle, map, size);
  }
  
  if (errno == 0)
    errno = EIO;

  log("tarfile map failed: errno=%d, label=%s\r\n", errno, label);

  return -1;
}

/**
 * calloc() based on a memory backend
 */
void *tarfs_calloc(size_t count, size_t size) {

  void *buffer;

  if ((buffer = tarfs_os_malloc(size)) != NULL)
    memset(buffer, 0, size);

  return buffer;
}

/*
 * strdup() based on a memory backend
 */
char *tarfs_strdup(char const *str) {

  if (str != NULL) {

    size_t len = strlen(str);
    char  *dst = tarfs_os_malloc(len + 1);

    if (dst != NULL) {

      memcpy(dst, str, len);
      dst[len] = '\0';

      return dst;
    }
  }
  return NULL;
}



/**
 * Get size information for TARFS
 * @param fs_idx the value returned by tarfs_mount(), a non-negative value
 * @param[out] raw_size a pointer to a stored value. Can be NULL. TAR file size
 * @param[out] data_size a pointer to a stored value. Can be NULL. Real data (files)
 * @return
 *    true if filesystem is found, false otherwise
 */
bool tarfs_info(int fs_idx, size_t *raw_size, size_t *data_size) {

  struct tarfs_fs *fs = tarfs_getfs_addref(fs_idx);

  if (fs != NULL) {

    if (raw_size)
      *raw_size = fs->fs_size;

    if (data_size)
      *data_size = fs->fs_dsize;
    tarfs_unref(fs);
    return true;
  }

  return false;
}

/**
 * Dump FS debug information.
 * 
 * @return
 */
void tarfs_dump(int fs_idx, void *vty, int (*vtyout)(void *, const char *, ...)) {

  struct tarfs_fs *fs = tarfs_getfs_addref(fs_idx);

  if (fs == NULL) {
    vtyout(vty,"filesystem %d is not mounted\r\n");
    return ;
  }

  vtyout(vty," --- TARFS '%s' ---\r\n", fs->fs_mountpoint);
  vtyout(vty,"ref   : %u\r\n"
             "vaddr : %p\r\n"
             "size  : %u\r\n"
             "dsize : %u\r\n"
             "nino  : %u\r\n"
             "usedfd: %08x\r\n", fs->fs_ref - 1, fs->fs_vaddr, fs->fs_size, fs->fs_dsize, fs->fs_nino, fs->fs_usedfd);

  for (int fd = 0; fd < TARFS_MAX_FDS; fd++) {
    
    if ((atomic_load_explicit(&fs->fs_usedfd, memory_order_relaxed) & (1u << fd)) != 0) {
      vtyout(vty, "fd#%d: allocated: ", fd);
      struct tarfs_fp  *fp = &fs->fs_fd[fd];
      vtyout(vty,"vaddr=%p, pos=%p, size=%u, inode=%d\r\n", fp->fp_vaddr, fp->fp_pos, fp->fp_size, fp->fp_idx);
    } else
      vtyout(vty, "fd#%d: free\r\n", fd);
  }

  tarfs_unref(fs);
}


/**
 * Perform a deep filesystem integrity check.
 *
 * Verifies the integrity of file contents if CRC64 checksums are present
 * in the archive.
 *
 * @param label Filesystem label.
 * @return Number of entries that failed verification.
 */
int tarfs_fsck(const char *label) {

  size_t size;
  void const *map;
  void *os_handle;

  log("Checking filesystem '%s'..\r\n", label);

  if (NULL != (map = tarfs_os_map_tarfile( label, &os_handle, &size))) {
    int num = tar_verify_crc(map, size, false);
    tarfs_os_unmap_tarfile(os_handle, map, size);
    return num;
  }

  return -1;
}

