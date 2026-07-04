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

#ifdef __XTENSA__
#  include "esp_heap_caps.h"
#else
#  define heap_caps_malloc(length_, caps_) malloc(length_) 
#  define heap_caps_free(ptr_) free(ptr_) 
#endif
#include "fs.h"
#include "file.h"

#define $$( Errno_ ) ({ errno = (Errno_); MAP_FAILED; })


static void *mmap_anon_emulated(size_t length, int prot) {

  void *ret;

  if (prot & PROT_EXEC)
    ret = heap_caps_malloc(length, MALLOC_CAP_EXEC);
  else
    ret = heap_caps_malloc(length, MALLOC_CAP_8BIT);

  if (ret == NULL) {
    return $$(ENOMEM);
  }

  errno = 0;
  return ret;
}

static int munmap_anon_emulated(void *addr, size_t length) {

  int ret = EINVAL;

  if (addr != NULL && length != 0) {

    heap_caps_free(addr);
    ret = 0;
  }
  return $(ret);
}

/**
 * POSIX mmap().
 * Intended to be used like this:
 * 
 * ptr = mmap(NULL, length, PROT_READ, flags, fd, offset);
 */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {

  struct ioctl_req io;
  struct tarfs_fp *fp;
  struct tarfs_fs *fs;

  if ((flags & MAP_FIXED) && addr != NULL) {
    log("esp32 mmu does not support MAP_FIXED\r\n");
    return $$(EINVAL);
  }

  if ((flags & MAP_ANONYMOUS) && fd < 0)
    return mmap_anon_emulated(length,prot);

  /* Do not support WRITE|EXEC */
  if ((prot & (PROT_WRITE|PROT_EXEC)) != 0 || fd < 0)
    return $$(ENOTSUP);

  /* For RO filesystem MAP_SHARED equals MAP_PRIVATE. ESP32's MMU only allows for shared entries */
  

  /* Obtain corresponding FS and FD pointers, add reference to the filesystem (done in ioctl()) */
  if ((ioctl(fd, FIOGETFD, &io) >= 0)) {

    fs = req.fs;
    fp = &fs->fs_fds[req.fd];

    if (fp->fp_vaddr == NULL ||
        offset < 0 || 
        offset > fp->fp_size || 
        length > (fp->fp_size - offset)) {
      /* addref'ed in the ioctl() */
      log("failed: fp_vaddr=0x%x, offset=%d, fp_size=%u, length=%u\r\n",fp->fp_vaddr, offset, fp->fp_size, length);
      tarfs_unref(fs);
      return $$(EINVAL);
    }


    /* Partition is mmaped already by mount, here we just calculate the right 
     * memory offset
     */
    log("fd=%d, mapped %u bytes vaddr=0x%x, offset=0x%x\r\n",fd, fp->fp_vaddr, offset);
    errno = 0;
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
    lock();
    for (int i = 0; i < TARFS_MAX_FS; i++) {

      fs = &s_tarfs[i];

      if (fs != NULL && 
          vaddr >= fs->fs_vaddr &&
          vaddr < (fs->fs_vaddr + fs->fs_size)) {
        unlock();
        tarfs_unref(fs);
        return $(0);
      }
    }
    unlock();
  }

  /* must be malloced or we are doomed */
  munmap_anon_emulated(addr, length);
  return $(0);
}
