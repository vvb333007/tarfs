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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_vfs.h"
#include "esp_err.h"
#include "esp_partition.h"
#include "esp_rom_spiflash.h"

#include "esp_refc.h"
#include "esp_tarfs.h"



static int tarfs_stat(void* ctx, const char * path, struct stat * st)
{
    assert(path);
    assert(st);
    tarfs_stat s;
    struct tarfs_fs * efs = (struct tarfs_fs *)ctx;

    off_t res = TARFS_stat(efs->fs, path, &s);
    if (res < 0) {
        errno = tarfs_res_to_errno(TARFS_errno(efs->fs));
        TARFS_clearerr(efs->fs);
        return -1;
    }
    memset(st, 0, sizeof(*st));
    st->st_size = s.size;
    st->st_mode = S_IRWXU | S_IRWXG | S_IRWXO;
    st->st_mode |= (s.type == TARFS_TYPE_DIR)?S_IFDIR:S_IFREG;
    st->st_mtime = tarfs_get_mtime(&s);
    st->st_atime = 0;
    st->st_ctime = 0;
    return res;
}


static DIR* tarfs_opendir(void* ctx, const char* name)
{
    assert(name);
    struct tarfs_fs * efs = (struct tarfs_fs *)ctx;
    tarfs_dir_t * dir = calloc(1, sizeof(tarfs_dir_t));
    if (!dir) {
        errno = ENOMEM;
        return NULL;
    }
    if (!TARFS_opendir(efs->fs, name, &dir->d)) {
        free(dir);
        errno = tarfs_res_to_errno(TARFS_errno(efs->fs));
        TARFS_clearerr(efs->fs);
        return NULL;
    }
    dir->offset = 0;
    strlcpy(dir->path, name, TARFS_OBJ_NAME_LEN);
    return (DIR*) dir;
}

static int tarfs_closedir(void* ctx, DIR* pdir)
{
    assert(pdir);
    struct tarfs_fs * efs = (struct tarfs_fs *)ctx;
    tarfs_dir_t * dir = (tarfs_dir_t *)pdir;
    int res = TARFS_closedir(&dir->d);
    free(dir);
    if (res < 0) {
        errno = tarfs_res_to_errno(TARFS_errno(efs->fs));
        TARFS_clearerr(efs->fs);
        return -1;
    }
    return res;
}

static struct dirent* tarfs_readdir(void* ctx, DIR* pdir)
{
    assert(pdir);
    tarfs_dir_t * dir = (tarfs_dir_t *)pdir;
    struct dirent* out_dirent;
    int err = tarfs_readdir_r(ctx, pdir, &dir->e, &out_dirent);
    if (err != 0) {
        errno = err;
        return NULL;
    }
    return out_dirent;
}

static int tarfs_readdir_r(void* ctx, DIR* pdir, struct dirent* entry,
                                struct dirent** out_dirent)
{
    assert(pdir);
    struct tarfs_fs * efs = (struct tarfs_fs *)ctx;
    tarfs_dir_t * dir = (tarfs_dir_t *)pdir;
    struct tarfs_dirent out;
    size_t plen;
    char * item_name;
    do {
        if (TARFS_readdir(&dir->d, &out) == 0) {
            errno = tarfs_res_to_errno(TARFS_errno(efs->fs));
            TARFS_clearerr(efs->fs);
            if (!errno) {
                *out_dirent = NULL;
            }
            return errno;
        }
        item_name = (char *)out.name;
        plen = strlen(dir->path);

    } while ((plen > 1) && (strncasecmp(dir->path, (const char*)out.name, plen) || out.name[plen] != '/' || !out.name[plen + 1]));

    if (plen > 1) {
        item_name += plen + 1;
    } else if (item_name[0] == '/') {
        item_name++;
    }
    entry->d_ino = 0;
    entry->d_type = out.type;
    strncpy(entry->d_name, item_name, TARFS_OBJ_NAME_LEN);
    entry->d_name[TARFS_OBJ_NAME_LEN - 1] = '\0';
    dir->offset++;
    *out_dirent = entry;
    return 0;
}

static long tarfs_telldir(void* ctx, DIR* pdir)
{
    assert(pdir);
    tarfs_dir_t * dir = (tarfs_dir_t *)pdir;
    return dir->offset;
}

static void tarfs_seekdir(void* ctx, DIR* pdir, long offset)
{
    assert(pdir);
    struct tarfs_fs * efs = (struct tarfs_fs *)ctx;
    tarfs_dir_t * dir = (tarfs_dir_t *)pdir;
    struct tarfs_dirent tmp;
    if (offset < dir->offset) {
        //rewind dir
        TARFS_closedir(&dir->d);
        if (!TARFS_opendir(efs->fs, NULL, &dir->d)) {
            errno = tarfs_res_to_errno(TARFS_errno(efs->fs));
            TARFS_clearerr(efs->fs);
            return;
        }
        dir->offset = 0;
    }
    while (dir->offset < offset) {
        if (TARFS_readdir(&dir->d, &tmp) == 0) {
            errno = tarfs_res_to_errno(TARFS_errno(efs->fs));
            TARFS_clearerr(efs->fs);
            return;
        }
        size_t plen = strlen(dir->path);
        if (plen > 1) {
            if (strncasecmp(dir->path, (const char *)tmp.name, plen) || tmp.name[plen] != '/' || !tmp.name[plen+1]) {
                continue;
            }
        }
        dir->offset++;
    }
}

static int tarfs_mkdir(void* ctx, const char* name, mode_t mode) {
  return $(EROFS);
}

static int tarfs_rmdir(void* ctx, const char* name) {
  return $(EROFS);
}

static int tarfs_truncate(void* ctx, const char *path, off_t length) {
  return $(EROFS);
}

static int tarfs_ftruncate(void* ctx, int fd, off_t length) {
  if (!allocated(fd))
    return $(EBADF);
  return $(EROFS);
}

static int tarfs_link(void* ctx, const char* n1, const char* n2) {
  return $(EROFS);
}

static int tarfs_unlink(void* ctx, const char *path) {
  return $(EROFS);
}

static int tarfs_rename(void* ctx, const char *src, const char *dst) {
  return $(EROFS);
}

static int tarfs_utime(void *ctx, const char *path, const struct utimbuf *times) {
  return $(EROFS);
}



static const esp_vfs_dir_ops_t s_tarfs_dir = {

    .stat_p      = &tarfs_stat,

    .link_p      = &tarfs_link,
    .unlink_p    = &tarfs_unlink,
    .rename_p    = &tarfs_rename,

    .opendir_p   = &tarfs_opendir,
    .closedir_p  = &tarfs_closedir,
    .readdir_p   = &tarfs_readdir,
    .readdir_r_p = &tarfs_readdir_r,
    .seekdir_p   = &tarfs_seekdir,
    .telldir_p   = &tarfs_telldir,

    .mkdir_p     = &tarfs_mkdir,
    .rmdir_p     = &tarfs_rmdir,

    .truncate_p  = &tarfs_truncate,
    .ftruncate_p = &tarfs_ftruncate,

    .utime_p     = &tarfs_utime,
};


/* Same as $() helper, but cast to void* */
#define $$( Errno_ ) (void *)$( Errno_ )


/**
 * POSIX mmap().
 * Intended to be used like this:
 * 
 * ptr = mmap(NULL, length, PROT_READ, int flags, int fd, off_t offset) {
 */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {

  struct ioctl_req io;
  struct tarfs_fp *fp;
  struct tarfs_fs *fs;

  // XXX: implement MAP_ANON via heap_alloc_with_caps()

  /* Do not support WRITE|EXEC */
  if ((prot & (PROT_WRITE | PROT_EXEC)) != 0)
    return $$(ENOTSUP);

  /* -1 is only relevant for MAP_ANON */
  if (fd < 0)
    return $$(ENOTSUP);

  /* For RO filesystem MAP_SHARED equals MAP_PRIVATE. ESP32's MMU only allows for shared entries */
  

 /* Obtain corresponding FS and FD pointers, add reference to the filesystem (done in ioctl()) */
  if ((ioctl(fd, FIOGETFD, &io) >= 0)) {

    fs = req.fs;
    fp = &fs->fs_fds[req.fd];

    if (offset < 0 || 
        offset > fp->fp_size || 
        length > (fp->fp_size - offset)) {
      /* addref'ed in the ioctl() */
      tarfs_unref(fs);
      return $$(EINVAL);
    }


    /* Partition is mmaped already by mount, here we just calculate the right 
     * memory offset
     */
    return (void *)(fp->fp_vaddr + offset);
  }

  return $$(EBADF);
}


/**
 * POSIX munmap().
 * 
 */
int munmap(void *addr, size_t length) {

  struct tarfs_fs *fs;
  uintptr_t vaddr = (uintptr_t )addr;

  if (addr != NULL && length > 0) {
    for (int i = 0; i < TARFS_MAX_FS; i++) {

      fs = &s_tarfs[i];

      if (fs != NULL && 
          vaddr >= fs->fs_vaddr &&
          vaddr < (fs->fs_vaddr + fs->fs_size)) {

        tarfs_unref(fs);
        return $(0);
      }
    }
  }

  log("failed for addr=%p, length=%u\r\n", addr, length);

  return $(EINVAL);
}

/**
 * Dump filesystems and file descriptors using user-provided printer routine
 * The printer (vtyout) and its context (vty) can be as simple as `fprintf` and `stdout`
 */
void esp_tarfs_dump(void *vty, int (*vtyout)(void *, const char *, ...)) {

  vtyout(vty, "TARFS filesystems:\r\n")
}







/**
 * @brief TARFS DIR structure
 */
struct tarfs_DIR {

    DIR dir;            /*!< VFS DIR struct, goes first so this struct can be cast to DIR */
    struct dirent e;    /*!< Last open dirent */
    long offset;        /*!< Offset of the current dirent */
    char path[TARFS_OBJ_NAME_LEN]; /*!< Requested directory name */
    tarfs_DIR d;        /*!< TARFS DIR struct */

} tarfs_dir_t;
