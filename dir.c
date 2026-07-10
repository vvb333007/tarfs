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
#include <string.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/dirent.h>

#include "config.h"
#include "os.h"
#include "fs.h"
#include "inode.h"
#include "refc.h"
#include "tar.h"
#include "dir.h"


/* How readdir() works:
 *
 * Inodes are stored in two independent orderings:
 *
 *  1. Hash-sorted index used for fast pathname lookup (fs->fs_ino array)
 *  2. Lexicographically sorted list of full pathnames, used by readdir() (linked list via inode->in_next ).
 *
 * Example:
 *
 *      /dir3/dir33/              <-- opendir() / open()
 *      /dir3/dir33/file33.txt    <-- direct child readdir()
 *      /dir3/dir33/file44.txt    <-- direct child readdir()
 *      /dir3/dir33/file55/       <-- direct child readdir()
 *      /dir3/dir33/file55/jopa   <-- indirect child skip
 *      /dir3/dir33/file55/ssaka  <-- indirect child skip
 *      /dir3/dir33/file66/       <-- direct child readdir()
 *      /dir3/dir33/file55.txt    <-- direct child readdir()
 *      /dir3/dir33/file77.txt    <-- direct child readdir()
 *      /dir3/dir44/              <-- end of this directory, readdir() returns NULL
 *      /dir3/dir44/test          
 *
 * Because all pathnames are lexicographically sorted, every direct child of a
 * directory immediately follows its own inode in the sorted list.
 *
 * opendir() returns the index of the directory inode. readdir() then starts
 * scanning from the next entry (idx + 1), returning only direct children while
 * skipping descendants located in subdirectories (e.g. file55/jopa). Scanning
 * stops as soon as the pathname no longer belongs to the opened directory
 * (e.g. "/dir3/dir44/").
 */



typedef struct {
  int a;
  int b;
} xDIR;



/**
 * @brief TARFS DIR structure
 */
struct tarfs_dir {

    xDIR          di_dir;       /*!< Opaque VFS DIR struct */
    struct dirent di_ent;       /*!< dirent to return to the caller */
    size_t        di_off;       /*!< offset to the current dir; actually - the next inode to read */
    int           di_fd;
    char         *di_prefix;    /*!< Directory name with trailing slash removed (populated by opendir()) */
    struct tarfs_inode const *di_ino;
    struct tarfs_inode const *di_cino;

};

[[maybe_unused]] static const char *remove_subpath(const char *path, const char *subpath) {

  const char *text = path;

  while(*text && *text != '\r' && *text != '\n' &&
        *subpath && *subpath != '\r' && *subpath != '\n' &&
        *text == *subpath) {
    text++;
    subpath++;
  }

  if (*subpath != 0 && *subpath != '\r' && *subpath != '\n')  /* subpath failed: prefix differs */
    text = path; 

  return text;
}


/**
 * Check if 'path' is direct child of 'prefix'; 
 * Prefix MUST NOT have '/' at the end.
 */
[[maybe_unused]] static int is_direct_child(const char *path, const char *prefix) {

    const char *tail;
    const char *slash;
    size_t plen;

    plen = strlen(prefix);

    if (tar_strncmp(path, prefix, plen) != 0)
        return -1;

    tail = path + plen;

    /* the dir itself */
    if (*tail == '\0' || *tail == '\r' || *tail == '\n')
        return 1;


    /* the dir itself when prefix does not end with '/' */
    if (*tail == '/') {
      
        tail++;
        if (*tail == '\0' || *tail == '\r' || *tail == '\n')
          return 1;
    } else
    /* we only tolerate / and NUL here. Any other character just mean that paths are different */
      return -1;

    slash = strchr(tail, '/');

    /* файл */
    if (slash == NULL)
        return 1;

    /* каталог первого уровня */
    return slash[1] == '\0' || slash[1] == '\r' || slash[1] == '\n';
}



/**
 * @brief Open a directory for reading.
 *
 * Opens an existing directory and returns a directory stream that can be
 * used with tard_readdir(), tard_telldir(), tard_seekdir() and
 * tard_closedir().
 */

DIR* tard_opendir(void* ctx, const char* name) {

  struct tarfs_dir *dir;

  if (name && *name) {

    int nlen = strlen(name);

    int fd = tarf_open(ctx, name, O_DIRECTORY|O_RDONLY, 0);
    if (fd >= 0) {

      dir = tarfs_calloc(1, sizeof(struct tarfs_dir));

      if (dir != NULL) {

        struct tarfs_fs *fs = tarfs_getfs((int)(uintptr_t)ctx);

        if (fs != NULL) {

          dir->di_off  = 0;           /* Current directory position (0 = before first entry) */
          dir->di_fd   = fd;          /* Underlying directory file descriptor */
          dir->di_ino  = fs->fs_ino[fs->fs_fd[fd].fp_idx]; /* Directory inode */
          dir->di_cino = dir->di_ino;                      /* Current inode used by readdir()/seekdir() */

          dir->di_prefix = tarfs_strdup(name);

          if (dir->di_prefix[nlen - 1] == '/')
            dir->di_prefix[nlen - 1] = '\0';

          if (dir->di_prefix != NULL) {
            log("dir '%s' opened, fd=%d\r\n", name, fd);
            return (DIR*)dir;
          }
        }
        log("ERR: NULL fs index %d\r\n", (int)(uintptr_t)ctx);
        tarfs_os_free(dir);
      } else
        errno = ENOMEM;
      tarf_close(ctx, fd);
    }
  }
  log("failed for '%s'\r\n", name);
  return NULL;
}

/**
 * @brief Close a directory stream.
 *
 * Releases all resources associated with a directory stream previously
 * returned by tard_opendir().
 */
int tard_closedir(void* ctx, DIR* pdir) {

  if (pdir) {

    struct tarfs_dir *dir = (struct tarfs_dir *)pdir;

    log("closing dir, fd=%d\r\n", dir->di_fd);

    tarf_close(ctx, dir->di_fd);

    if (dir->di_prefix != NULL)
      tarfs_os_free((void *)dir->di_prefix);

    tarfs_os_free(dir);

    return 0;
  }

  errno = EINVAL;
  return -1;
}


/**
 * @brief Read the next directory entry.
 *
 * Returns the next direct child of the opened directory. The returned
 * pointer remains valid until the next call to tard_readdir() on the same
 * directory stream.
 */
struct dirent* tard_readdir(void* ctx, DIR* pdir) {

  struct tarfs_dir *dir = (struct tarfs_dir *)pdir;
  struct tarfs_inode const *cur;

  cur = dir->di_cino;

  while (cur->in_next != NULL) {

    cur = cur->in_next;

    int x = is_direct_child((char const *)cur->in_path, dir->di_prefix);

    if (x < 0) {
      log("end of directory '%s' reached\r\n", dir->di_prefix);
      return NULL;
    }

    if (x > 0) {
//      log("next entry '%s' read\r\n", (char const *)cur->in_path);
      const char *p = remove_subpath((char const *)cur->in_path, dir->di_prefix);

      if (*p == '/') /* normally yes */
        p++;

      int len = tar_strlen(p, NULL);
      if (len < sizeof(dir->di_ent.d_name)) {

        memcpy(dir->di_ent.d_name, p, len);
        dir->di_ent.d_name[len] = '\0';
        
        switch(inode_rawtype(cur)) {
          case TART_DIR:  dir->di_ent.d_type = DT_DIR; break;
          case TART_FILE:
          case TART_AFILE:
          case TART_CONT: dir->di_ent.d_type = DT_REG; break;
          default:        dir->di_ent.d_type = DT_UNKNOWN; break;
        }

        dir->di_ent.d_ino = 0; //TODO: inode number.
        

        dir->di_cino = cur;
        dir->di_off++; 

        return &dir->di_ent;
      } else
        errno = ENAMETOOLONG;
    }
  }  

  log("end of alphalist is reached\r\n");
  return NULL;
}


/**
 * @brief Return the current directory position.
 *
 * Obtains the current position within the directory stream. The returned
 * value may later be passed to tard_seekdir() to restore the same position.
 */
long tard_telldir(void* ctx, DIR* pdir) {

  struct tarfs_dir * dir = (struct tarfs_dir *)pdir;
  return dir->di_off;
}


/**
 * @brief Reposition a directory stream.
 *
 * Sets the current position within the directory stream to a value
 * previously obtained by tard_telldir().
 */
void tard_seekdir(void* ctx, DIR* pdir, long offset) {

  struct tarfs_dir * dir = (struct tarfs_dir *)pdir;

  /* We cant setp backwards - we are using single-linked list
   * so instead we perform full rewind() and then simply do 'offset' numbers of readdir() calls
   */
  if (dir->di_off > offset) {
    dir->di_off = 0; 
    dir->di_cino = dir->di_ino;
  }

  while(dir->di_off < offset) {
    if (NULL == tard_readdir(ctx, pdir)) {
      log("offset %ld is not reachable, stopped at offset %lu\r\n", offset, dir->di_off);
      break;
    }
  }
}

/**
 * The function dirfd() returns the file descriptor associated with the directory stream pdir.
*/
int tard_dirfd(void* ctx, DIR *pdir) {
  if (pdir != NULL) {
    struct tarfs_dir * dir = (struct tarfs_dir *)pdir;
    return dir->di_fd;
  }
  errno = EINVAL;
  return -1;
}


/**
 * @brief Create a directory.
 */
int tard_mkdir(void* ctx, const char* name, mode_t mode) {

  log("read-only file system\r\n");
  errno = EROFS;

  return -1;
}

/**
 * @brief Remove a directory.
 */
int tard_rmdir(void* ctx, const char* name) {

  log("read-only file system\r\n");
  errno = EROFS;

  return -1;
}


