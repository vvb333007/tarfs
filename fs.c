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
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/errno.h>
#include <sys/fcntl.h>

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
 * Lock access to s_tarfs[] table and to the s_numfs counter
 */
void tarfs_lock() {
  tarfs_os_acquire_mutex();  
}

/**
 * Unlock access to s_tarfs[] table and to the s_numfs counter
 *  
 */
void tarfs_unlock() {
  tarfs_os_release_mutex();  
}



/** Find an empty slot or mounted slot
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
        printf("finds() : empty slot #%d found\r\n", i);
        return i;
      }

      continue;
    }

    /* We are not interested in occupied slots if mountpoint is NULL */
    if (mountpoint == NULL)
      continue;

    /* Exact name match? Return slot number */
    if (!strcmp(s_tarfs[i]->fs_mountpoint, mountpoint)) {
      printf("finds() : mountpoint %s slot #%d found\r\n", mountpoint, i);
      return i;
    }
  }

  /* Log, set errno and return -1 */

  if (mountpoint == NULL) {
    log("can not find empty slot\r\n");
    return  $(EBUSY);
  }

  log("can not find slot (%s)\r\n", mountpoint);
  return $(ENOENT);
}

/**
 * Find filesystem slot [0..TARFS_MAX_FS) which is used to hold given fs
 *
 */
static int slot_index(struct tarfs_fs *fs) {

  if (fs != NULL) {
    intptr_t diff = (uintptr_t)fs - (uintptr_t)(&s_tarfs[0]);
    if (diff >= 0 && (diff % sizeof(struct tarfs_fs) == 0)) {
      int slot;
      slot = diff / sizeof(struct tarfs_fs);
      if (slot < TARFS_MAX_FS)
        return slot;
    }
  }
  return -1;
}


/** Actual "unmount" procedure. Called by unref(). Finalizes unmount procedure, unmaps memory region,
 *  clears s_tarfs[] entry. This destructor is called by the last user of unmounted filesystem: if filesystem
 *  had no open files then this destructor is called from unmount() only. Otherwise it may be close(), closedir() etc
 *
 */
static void commit_unmount(void *ctx) {

  int tmp;

  int slot;
  struct tarfs_fs *fs = (struct tarfs_fs *)ctx;

  if (fs == NULL)
    return ;

  slot = slot_index(fs);

  log("unmounting FS slot #%d\r\n\r\n", slot);

  /* If we can not properly unregister our FS we don't remove tarfs_fs entry
   * to prevent crashes: unregistered fs can still call read()/write()/open()
   */  
  if (!tarfs_os_unregister_fs(fs->fs_mountpoint)) {

    log(" failed to unregister (%s ; slot=%d), memory leaked!\r\n", fs->fs_mountpoint, slot);
    /* This will create a memory leak, but prevents crashes */
    goto clear_slot;
  }

  if (fs->fs_ino != NULL && fs->fs_nino > 0) {

    log(" deleting inodes (%u)\r\n", fs->fs_nino);
    inode_unmount(fs, (const void *)fs->fs_vaddr, fs->fs_size);
  }

  if (fs->fs_vaddr != NULL) {
    log(" unmapping (%s), vaddr=%p, size=%u\r\n", fs->fs_mountpoint, fs->fs_size);
    tarfs_os_unmap_tarfile((void *)fs->fs_handle, (void *)fs->fs_vaddr, fs->fs_size);
    fs->fs_vaddr = NULL;
  }

  free(fs);

clear_slot:
  tarfs_lock();
  s_numfs--;
  if (slot >= 0)
    s_tarfs[slot] = NULL;
  tarfs_unlock();

  log(" %d more filesystems left\r\n", s_numfs);
}

int tarfs_unref(struct tarfs_fs *fs) {
  return fs == NULL ? 0 : unrefx(&fs->fs_ref, fs, commit_unmount);
}

#if 0
/**
 * Actual mount procedure
 * We expect sane label pointer (ASCIIZ) and a sane mountpoint (i.e. len>1, 
 * fisrt sym is `/`, last sym != `/`)
 */
static int tarfs_mount(const char *label, const char *mountpoint) {

  const void                 *vaddr;
  int slot;
  struct tarfs_fs *fs = NULL;
  size_t len;

  assert(label != NULL);
  /* mount procedure */

  /* Create a FS descriptor, mount the filesystem */
  lock();
  if (s_numfs < TARFS_MAX_FS) {
    if (0 <= (slot = findfs( NULL ))) {
      if ( NULL != (fs = calloc(1, sizeof(struct tarfs_fs) + len + 1))) {
  
        void *map;
        // Copy mountpoint. Trailing zero is there already
        memcpy(fs->fs_mountpoint, mountpoint, len);

        // Actual memory mapping
        if (NULL != (map = tarfs_os_map_tarfile( label, &os_handle, &size))) {

          initref(&fs->fs_ref);

          fs->fs_vaddr = map;
          fs->fs_handle = handle;
          fs->fs_size = size;

          if (inode_mount(fs) < 0) {
            if (fs->fs_ino == NULL) {
              log("filesystem is unusable, no valid inodes were found\r\n");
              goto free_memory_and_fail;
            }
            log("filesystem is in degraded mode\r\n");
          }

          s_tarfs[slot] = fs;
          s_numfs++;
          unlock();

          log("mounted [%s]\r\n", label);

          return $(0);
        } 
free_memory_and_fail:
        free(fs);
        err = EIO;    /* MMU errors are reported as EIO */
      } else
        err = ENOMEM; /* calloc() failed */
    } else
      err = EBUSY;    /* Too many fs: all slots are occupied, unmount something first */
  } else
    err = EBUSY;      /* Too many fs: s_numfs hit the limit */



  log("failed: errno=%d, s_numfs=%d, part=%s, mountpoint=%s,\r\n", errno, s_numfs, label, mountpoint);

  unlock();
  
  return $(err);
}



/** 
 * Unmounting a filesystem with a zero open files AND opening a file at the same time
 * MAY result in racecond which eventually lead to crash. This must be fixed at ESP-IDF VFS level.

 */
int tarfs_unmount(const char *mountpoint) {
  
  int slot,
      err = 0;

  struct tarfs_fs *fs;

  refc_type_t prev_refc;

  if (mountpoint == NULL)
    return $(EINVAL);

  lock(); /* protect s_tarfs[] array, protects from a concurrent tarfs_unmount/mount */
  
  if ((slot = findfs(mountpoint)) >= 0) {
    fs = s_tarfs[slot];

//  XXX: deadlock! Must use recursive locks
    prev_refc = tarfs_unref(fs);
    if (prev_refc > 1) {
      log("Filesystem %s is in use (%u FD), umount delayed\r\n", mountpoint, prev_refc - 1);
      err = EAGAIN;
    }

  } else
    err = ENOENT;

  unlock();
  return $(err);
}
#endif