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
  logerr("filesystem #%d does not exist!\r\n",i);
  return NULL;
}

/**
 * Thread safe, increases refcounter, uses mutex!
 * Must be used when filesystem #i is in unknown state (e.g. is being unmounted)
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

      if (s_tarfs[i] != NULL) {
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
    }
    tarfs_unlock();

    return best;
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

  log("unmounting FS '%s' started\r\n", fs->fs_mountpoint);


  log("unregistering VFS\r\n");

  /* If we can not properly unregister our FS we don't remove tarfs_fs entry
   * to prevent crashes: unregistered fs can still call read()/write()/open()
   */  
  if (!tarfs_os_unregister_fs(fs->fs_mountpoint)) {

    logerr(" failed to unregister '%s', memory leaked!\r\n", fs->fs_mountpoint);
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

    log(" deleting inodes (%u)\r\n", (unsigned int)fs->fs_nino);
    inode_unmount(fs, (const void *)fs->fs_vaddr, fs->fs_size);
    fs->fs_ino = NULL;
  }

  /* Release the FS image */
  if (fs->fs_vaddr != NULL) {
    if (fs->fs_handle != 0) {
      log(" unmapping filesystem image, vaddr=%p, size=%u\r\n", (const void *)fs->fs_vaddr, (unsigned int)fs->fs_size);
      tarfs_os_unmap_tarfile((void *)fs->fs_handle, (void *)fs->fs_vaddr, fs->fs_size);
    }
    fs->fs_vaddr = NULL;
  }

  log("deleting FS descriptor %p\r\n", (void *)fs);
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
 *
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
      logerr("Filesystem %s is in use (%u open fds), unmount delayed\r\n", mountpoint, prev_refc - 1);
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

#if CONFIG_TARFS_INTEGRITY
  log("TAR-CRC64 filesystem is expected\r\n");
#endif

    if (false == (len > 1 && mountpoint[0] == '/' && mountpoint[len - 1] != '/')) {
      logerr("mountpoint is too short or invalid '%s'\r\n", mountpoint);
      errno = EINVAL;
unmap_and_return_error:
      
      return -1;
    }

    log("mountpoint: '%s'\r\n", mountpoint);

    if (base_dir[1] == '\0') {
      logerr("WARN: tarfile has no root directory\r\n");
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
      logerr("ERR: out of memory\r\n");
      errno = ENOMEM;
      goto unmap_and_return_error;
    }

    /* Allocate free FS slot */
    /* ------- locked -------*/
    tarfs_lock();
    if (0 > (slot = findfs( NULL ))) {
      tarfs_unlock();
      tarfs_os_free(fs);
      logerr("too many mounted filesystems (%u)\r\n",s_numfs);
      errno = EBUSY;
      goto unmap_and_return_error;
    }
  
    /* Occupy FS slot and release the lock ASAP */
    s_tarfs[slot] = fs;
    s_numfs++;
    tarfs_unlock();
  /* ------- unlocked -------*/

    log("allocated new FS descriptor s_tarfs[%d] = %p\r\n", slot, (void *)fs);

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
        logerr("filesystem is unusable, no valid inodes were found\r\n");
        tarfs_lock();
        s_tarfs[slot] = NULL;
        s_numfs--;
        tarfs_unlock();
        /* errno must be set in inode_mount() */
        goto unmap_and_return_error;
      } 
      logerr("WARN: running in degraded mode\r\n");
    }

  log("addr %p:%u --> %u inodes successfully mounted\r\n", map, (unsigned int)size, (unsigned int)fs->fs_nino);

  /* Registering VFS */
  log("registering TARFS in VFS..\r\n");
  if (tarfs_os_register_fs(mountpoint, (void *)(intptr_t)slot) == false) {
    logerr("WARN: can not register POSIX handlers, only native tarfs API is available\r\n");
  } else {
    log("registered. (prefix '%s' in VFS)\r\n", mountpoint);
  }

  log("mount is done. filesystem slot is %d\r\n", slot);
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

    log("resource is available at %p, %u bytes. mounting from memory..\r\n", map, (unsigned int)size);

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

  logerr("tarfile map failed: errno=%d, label=%s\r\n", errno, label);

  return -1;
}

/**
 * calloc() based on a memory backend
 */
void *tarfs_calloc(size_t count, size_t size) {

  void *buffer;

  if ((buffer = tarfs_os_malloc(size * count)) != NULL)
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
 * @param mp mount point
 * @param[out] raw_size a pointer to a stored value. Can be NULL. TAR file size
 * @param[out] data_size a pointer to a stored value. Can be NULL. Real data (files)
 * @return
 *    0 if success
 */
int tarfs_info(const char *mp, size_t *raw_size, size_t *data_size) {

  int fs_idx = tarfs_fsindex(mp);

  if (fs_idx >= 0) {

    struct tarfs_fs *fs = tarfs_getfs_addref(fs_idx);

    if (fs != NULL) {

      if (raw_size)
        *raw_size = fs->fs_size;

      if (data_size)
        *data_size = fs->fs_dsize;
      tarfs_unref(fs);

      return 0;
    }
  }

  errno = ENODEV;
  return -1;
}


/**
 * Dump FS debug information.
 * 
 * @return
 */
void tarfs_dump(int fs_idx) {

  struct tarfs_fs *fs = tarfs_getfs_addref(fs_idx);

#define vtyout fprintf
#define vty stdout

  if (fs == NULL) {
    vtyout(vty,"filesystem %d is not mounted\r\n", fs_idx);
    return ;
  }

  vtyout(vty," --- TARFS '%s' ---\r\n", fs->fs_mountpoint);
  vtyout(vty,"ref   : %u\r\n"
             "vaddr : %p\r\n"
             "size  : %u\r\n"
             "dsize : %u\r\n"
             "nino  : %u\r\n"
             "usedfd: %08x\r\n", 
              fs->fs_ref - 1,
              fs->fs_vaddr,
              (unsigned int)fs->fs_size,    /* TODO: use crossplatform printf formatters PRu32 and friends*/
              (unsigned int)fs->fs_dsize,
              (unsigned int)fs->fs_nino,
              (unsigned int)fs->fs_usedfd);

  vtyout(vty," --- File descriptrors ---\r\n");

  for (int fd = 0; fd < TARFS_MAX_FDS; fd++) {
    
    if ((atomic_load_explicit(&fs->fs_usedfd, memory_order_relaxed) & (1u << fd)) != 0) {
      vtyout(vty, "fd#%d: allocated: ", fd);
      struct tarfs_fp  *fp = &fs->fs_fd[fd];
      vtyout(vty,"vaddr=%p, pos=%u, size=%u, inode=%d\r\n", (void *)fp->fp_vaddr, (unsigned int)fp->fp_pos, (unsigned int)fp->fp_size, fp->fp_idx);
    } else
      vtyout(vty, "fd#%d: free\r\n", fd);
  }


  inode_dumppath_sorted(fs->fs_root);

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
unsigned int tarfs_fsck(const char *label) {

  size_t tar_length;
  void const *map;
  void *os_handle;


  printf("Checking filesystem '%s'..\r\n", label);

  if (NULL != (map = tarfs_os_map_tarfile( label, &os_handle, &tar_length))) {
    int num = 0;
    do {

      uintptr_t tar_start = (uintptr_t )map;
      uint32_t size;
      size_t off = 0;
      int bad = 0, files = 0, dirs = 0, links = 0, bad_total = 0, hdr_no = 0, bad_files = 0;
      uintptr_t tar_end = (uintptr_t )((const uint8_t *)map + tar_length);

      while (off + sizeof(tarhdr_t) <= tar_length) {

        const tarhdr_t *hdr = (const tarhdr_t *)(tar_start + off);

        if (tar_badhdr(hdr)) {

          if (!bad) {
bad_header:
            printf("Inode#%u : bad metadata, switching to scan\r\n", hdr_no);
          }
          bad++;

          off += sizeof(tarhdr_t);
          continue;
        }

        if (bad) {
          bad_total += bad;
          printf("Inode#%u : %u headers were skipped, continuing to mount..\r\n", hdr_no, bad);
          bad = 0;
        }

        size = tar_octal(hdr->size, sizeof(hdr->size));

        /* Check if size is sane: current pointer + 512 bytes + size must be < tar_end */
        if (((uintptr_t)(hdr + 1)) + size >= tar_end) {
          printf("Inode#%u : element extends beyond the end of the archive\r\n", hdr_no);
          goto bad_header;
        }

        switch(hdr->type) {
          case TART_AFILE:
          case TART_CONT:
          case TART_FILE: files++; break;

          case TART_SYMLINK:
          case TART_HARDLINK: links++; break;

          case TART_DIR: dirs++; break;
          case TART_PAX:
          case TART_PAX_G: break;

          /* Unrecognized entry */
          default:
            printf("Unrecognized entry #%u, type=0x%02x\r\n",hdr_no, hdr->type);
        };


        if (tar_baddata(hdr, size)) {
          printf("Inode#%u : bad data\r\n", hdr_no);
          bad_files++;
        }

  
        off += sizeof(tarhdr_t);


        /* Real size is 512 bytes aligned */
        off += ((size_t)size + 511) & ~511u;

        hdr_no++;
      }

      bad_total += bad;

      unsigned int total = files + links + dirs;

      printf("Check finished.\r\n\r\n"
             "Inodes processed: %u, number of bad metadata headers: %u\r\n\r\n", hdr_no, bad_total);
      printf("Trailing garbage: %u blocks, ~%u bytes\r\n", bad, bad * 512);
      printf("Damaged data: (%u files / pax headers)\r\n",bad_files);
      printf("Available: %u entries (%u files, %u dirs, %u links)\r\n",total, files, dirs, links);

      num = bad_total + bad_files;
    } while (0);

    tarfs_os_unmap_tarfile(os_handle, map, tar_length);

    /* fsck() returns total number of unrecognized/bad headers */
    return num;
  }

  /* all headers are bad */
  return -1;
}


/**
 * Obtain filesystem statistics.
 *
 * Fills a POSIX statvfs structure with information about the mounted
 * filesystem. Since TARFS is a read-only filesystem, the number of
 * available blocks and inodes is always reported as zero.
 */

int tarfs_statvfs(void *ctx, struct statvfs *st) {

    int fs_idx;
    struct tarfs_fs *fs;

    if (!st) {
        errno = EFAULT; /* Linux sets EFAULT instead of EINVAL so we do the same */
        return -1;
    }

    fs_idx = (int)(uintptr_t)ctx;
    fs = tarfs_getfs_addref( fs_idx );

    if (fs == NULL) {
      errno = EIO;
      return -1;
    }

    memset(st, 0, sizeof(*st));

    /* FS ID*/
    st->f_fsid = fs_idx;

    /* Logical block size and total filesystem size */
    st->f_bsize  = 512;
    st->f_frsize = 512;
    st->f_blocks = ((fs->fs_dsize + 511) & ~511) / 512;
    
    /* Number of files (ROFS!) 
     * TODO: Number of files, not number of inodes
     */
    st->f_files  = fs->fs_nino;

    /* Maximum filename length and flags */
    st->f_namemax = 255; //sizeof(dir->di_ent.d_name) - 1;
    st->f_flag = ST_RDONLY|ST_NOATIME|ST_NODEV|ST_NODIRATIME|ST_NOEXEC|ST_NOSUID;

    /* Counters */  
#if CONFIG_TARFS_COUNTERS
    st->f_bread = fs->fs_bread;
    st->f_bmmap = fs->fs_bmmap;
    st->f_nfail = fs->fs_nfail;
#endif

    tarfs_unref(fs);

    return 0;
}

/**
 * Enable/Disable/Get fs_opencrc flag for a filesystem with index fs_idx
 * This flag enable optinal CRC64 checking on every open() call
 *
 * @param fs_idx A filesystem index as returned by the tarfs_mount() or by the tarfs_fsindex()
 * @param en 0 - diable, 1 - enable, -1 - do not change (to get current value see below)
 * @return previous value, before applying `en`
 */
int tarfs_integrity_on_open(int fs_idx, int en) {

#if CONFIG_TARFS_INTEGRITY
  struct tarfs_fs *fs = tarfs_getfs_addref(fs_idx);

  if (fs == NULL) {

    log("no such filesystem: %d\r\n", fs_idx);
    errno = ENODEV;

    return -1;
  }

  int ret = fs->fs_opencrc;

  if (en >= 0)
    fs->fs_opencrc = (int)(bool)en;
  tarfs_unref(fs);

  log(" CRC64 on each open() : %s\r\n", en < 0 ? "unchanged" : (en ? "enabled (slow open)" : "disabled"));
  return ret;
#else
  logerr("CONFIG_TARFS_INTEGRITY is disabled, flag ignored\r\n");
  return 0;
#endif
}
