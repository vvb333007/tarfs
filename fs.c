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



struct tarfs_fs *tarfs_getfs(int i) {
  return s_tarfs[i];
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


/** Actual "unmount" procedure. Called by unref(). Finalizes unmount procedure, unmaps memory region,
 *  clears s_tarfs[] entry. This destructor is called by the last user of unmounted filesystem: if filesystem
 *  had no open files then this destructor is called from unmount() only. Otherwise it may be close(), closedir() etc
 *
 */
static void commit_unmount(void *ctx) {



  int slot = -1;
  struct tarfs_fs *fs = (struct tarfs_fs *)ctx;

  if (fs == NULL)
    return ;

  log("unmounting FS '%s'\r\n", fs->fs_mountpoint);

  for (int i = 0; i< TARFS_MAX_FS; i++)
    if (s_tarfs[i] == fs) {
      slot = i;
      break;
    }

  log("unregistering VFS\r\n");

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
    log(" unmapping filesystem image, vaddr=%p, size=%lu\r\n", fs->fs_vaddr, fs->fs_size);
    tarfs_os_unmap_tarfile((void *)fs->fs_handle, (void *)fs->fs_vaddr, fs->fs_size);
    fs->fs_vaddr = NULL;
  }

  log("deleting FS descriptor %p\r\n", fs);
  free(fs);

clear_slot:

  printf("unmount() : clear FS entry\r\n");

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
  puts("0");
  tarfs_lock(); /* protect s_tarfs[] array, protects from a concurrent tarfs_unmount/mount */
  
  puts("1");
  if ((slot = findfs(mountpoint)) >= 0) {

    puts("2");
    fs = s_tarfs[slot];

    prev_refc = tarfs_unref(fs);
    puts("3");
    if (prev_refc > 1) {
      log("Filesystem %s is in use (%u FD), umount delayed\r\n", mountpoint, prev_refc - 1);
      err = EAGAIN;
    }

  } else
    err = ENOENT;
  puts("4");
  tarfs_unlock();
  return $(err);
}


#if 1
/**
 * Actual mount procedure
 * We expect sane label pointer (ASCIIZ) and a sane mountpoint (i.e. len>1, 
 * fisrt sym is `/`, last sym != `/`)
 */
int tarfs_mount(const char *label, const char *mountpoint, const char *link_rebase) {

  size_t size;
  int len;
  void *map;
  void *os_handle;
  int slot = -1;
  struct tarfs_fs *fs = NULL;
  char base_dir[100];

  if (link_rebase == NULL)
    link_rebase = "";


  /* Actual memory mapping */
  if (NULL != (map = tarfs_os_map_tarfile( label, &os_handle, &size))) {

    if (false == tar_rootdir(map, size, base_dir, sizeof(base_dir)))
      base_dir[0] = '\0';

    if (mountpoint == NULL)
      mountpoint = base_dir;
    else {
      printf("Detected MP '%s' is overriden with '%s'\r\n", base_dir, mountpoint);
    }

    len = strlen(mountpoint);


    /* Allocate free FS slot */
    tarfs_lock();
    if (0 > (slot = findfs( NULL ))) {
      printf("Too many mounted filesystems\r\n");
      errno = EBUSY;
error:
      tarfs_os_unmap_tarfile(os_handle, map, size);
      tarfs_unlock();
      return -1;
    }

    /* Allocate FS descriptor */
    if ( NULL == (fs = calloc(1, sizeof(struct tarfs_fs) + len + 1))) {
      printf("Out of memory\r\n");
      errno = ENOMEM;
      goto error;
    }
  
    /* Occupy FS slot and release the lock ASAP */
    s_tarfs[slot] = fs;
    s_numfs++;
    tarfs_unlock();

    log("allocated new FS descriptor %p, slot %d\r\n", fs, slot);

    initref(&fs->fs_ref);

    /* Store tarfile mapping parameters */
    fs->fs_vaddr = map;
    fs->fs_handle = (uintptr_t )os_handle;
    fs->fs_size = size;

    /* Copy mountpoint. Trailing zero is there already */
    log("mountpoint is '%s'\r\n", mountpoint);
    memcpy(fs->fs_mountpoint, mountpoint, len);

    log("mounting..\r\n");
    if (inode_mount(fs, map, size, link_rebase) < 0) {
      if (fs->fs_ino == NULL) {
        log("filesystem is unusable, no valid inodes were found\r\n");
        tarfs_lock();
        s_tarfs[slot] = NULL;
        s_numfs--;
        /* errno must be set in inode_mount() */
        goto error;
      }
      log("running in degraded mode\r\n");
    }

    log("attached to resource '%s', %u inodes\r\n", label, fs->fs_nino);

    /* Registering VFS */
    if (tarfs_os_register_fs(mountpoint) == false) {
      log("can not register POSIX handlers, FS is unusable\r\n");
    } else {
      log("registered prefix '/%s' in VFS\r\n", mountpoint);
    }

    log("done\r\n");
    return slot;
  } 

  if (errno == 0)
    errno = EIO;

  log("tarfile map failed: errno=%d, label=%s\r\n", errno, label);

  return -1;
}



#endif