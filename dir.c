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

#include "esp_vfs.h"
#include "esp_err.h"
#include "esp_partition.h"
#include "esp_rom_spiflash.h"

#include "fs.h"
#include "inode.h"
#include "refc.h"
#include "tar.h"



/**
 * @brief TARFS DIR structure
 */
struct tarfs_DIR {

    DIR           di_dir;       /*!< Opaque VFS DIR struct */
    struct dirent di_ent;       /*!< Last open dirent */
    long          di_off;       /*!< Offset of the current dirent */
    char          di_path[256]; /*!< Requested directory name, populated by opendir() */

};

static int tarfs_is_direct_child(const char *path, const char *prefix)
{
    const char *tail;
    const char *slash;
    size_t plen;

    plen = strlen(prefix);

    if (strncmp(path, prefix, plen) != 0)
        return 0;

    tail = path + plen;

    /* сам каталог */
    if (*tail == '\0')
        return 1;

    slash = strchr(tail, '/');

    /* файл */
    if (slash == NULL)
        return 1;

    /* каталог первого уровня */
    return slash[1] == '\0';
}



/**
 * stat() system call
 */
static int tarfs_stat(void* ctx, const char * path, struct stat * st) {

    int fd = tarf_open(ctx, path, O_RDONLY, 0);

    if (fd < 0)
        return -1;

    int rc = tarf_fstat(ctx, fd, st);

    tarf_close(ctx, fd);

    return rc;
}


/**
 * opendir() system call
 */
static DIR* tarfs_opendir(void* ctx, const char* name) {

    assert(name);

    struct tarfs_fs * fs = (struct tarfs_fs *)ctx;

    if (tarfs_addref(fs)) {

      /* XXX: name must end with '/' */
      int idx = inode_lookup(fs, name);
      if (idx >= 0 && inode_type(fs, idx) ==  TART_DIR) {

        /* 
            Paths of intereset start from inode index idx+1 since all paths are alphasorted.
            If for example we are opening /dir/dir33, then idx will point to the /dir/dir33's inode
            while idx+1, idx+2 and so on are direct child of that dir

            /dir2/dir22/file33_symlink.txt",
            /dir2/dir33_symlink",
            /dir3/",
            /dir3/dir22_symlink -> /dir4/dir3_dir_junction/dir22_symlink",
            /dir3/dir33/",         <--------------------------------------------- ino
            /dir3/dir33/file33.txt",   <-- direct child
            /dir3/dir33/file44.txt",   <-- direct child
            /dir3/dir33/file55/"        <-- direct child
            /dir3/dir33/file55/jopa"  
            /dir3/dir33/file55/ssaka"  
            /dir3/dir33/file66/",          <-- indirect child
            /dir3/dir33/file55.txt
            /dir3/dir33/file77.txt
            /dir3/dir44/",           <-- readdir() limit
            

        */
      }
      tarfs_unref(fs);
    }

    

//    dir->offset = 0;
//    strlcpy(dir->path, name, TARFS_OBJ_NAME_LEN);

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




void tard_handlers_install(void *opaque) {

  esp_vfs_dir_ops_t *files = (esp_vfs_dir_ops_t *)opaque;

  if (files != NULL)
    files->dir = &s_tarfs_dir,
}


