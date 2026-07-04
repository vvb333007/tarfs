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

#include <stdint.h>
#include <time.h>
#include "tar.h"
/**
 * TARFS inode structure, 12..20 bytes per inode; 1000 files require ~16KiB of RAM to store 
 * the filesystem index
 *
 * Inode represents a resolved filesystem object as a mapping:
 *   (hashed pathname) -> (tar header location in archive memory).
 *
 * Each inode corresponds to one usable tar entry.
 *
 * Usable entries are tar record types:
 *   - 0 : regular file
 *   - 1 : hard link
 *   - 2 : symbolic link
 *   - 5 : directory
 *
 * Notes:
 * - inode resolution always follows link targets until a final
 *   FILE (0) or DIRECTORY (5) entry is reached.
 *
 * A structure like this:
 *    name1 -links--> name2 --link--> name4.txt
 *    will result in 3 inodes:
 *
 * inode1.in_vaddr ==> "name1" entry
 * inode1.in_dvaddr ==> "name4.txt" entry
 *
 * inode2.in_vaddr ==> "name2" entry
 * inode2.in_dvaddr ==> "name4.txt" entry
 *
 * inode3.in_vaddr ==> "name4.txt" entry
 * inode3.in_dvaddr ==> "name4.txt" entry
 *
 * TARFS maintains two independent indexes:
 *
 *  - inode_index[] : sorted by in_hash for O(log N) pathname lookup.
 *  - in_next       : linked list sorted by pathname for directory
 *                    enumeration and prefix scans.
 */

struct tarfs_inode  {

  uint32_t   in_hash;    /*!< Hash of full path of the entry */

  uintptr_t  in_path;    /*!< Pointer to the full pathname. If NULL, the pathname is reconstructed at runtime from the tar header prefix/name fields.
                              The pathname is not guaranteed to be NUL-terminated. It may end
                              at '\0', '\r' or '\n', therefore strcmp() must not be used. */

  uintptr_t  in_vaddr;  /*!< Virtual address of the original tarhdr entry */
  uintptr_t  in_dvaddr; /*!< final resolved target after link resolution, i.e. a file or a directory */
  struct tarfs_inode *in_next;   /*!< Next inode in lexicographical pathname order.
                             The original inode array is never reordered. Sorting is achieved
                             solely by relinking inodes through this field. */
};



static inline bool inode_is_link(struct tarfs_inode const *inode) {
  return ((inode != NULL) && (inode->in_vaddr != inode->in_dvaddr));
}

typedef struct tarfs_inode tarfs_inode_t;

struct tarfs_fs;
int inode_lookup(struct tarfs_inode const * const *index, size_t num_inodes, const char *path);
struct tarfs_inode **inode_alloc(size_t count);
void inode_free(struct tarfs_inode **index, size_t count, uintptr_t tar_start, size_t tar_length);


tart_t inode_type(struct tarfs_inode **index, int idx);

void inode_sort(struct tarfs_inode **iarr, size_t count);
struct tarfs_inode *inode_alphasort(struct tarfs_inode *array, size_t count);
int inode_resolve(struct tarfs_inode **index, size_t count);
void inode_populate(struct tarfs_inode *inodes, size_t nino, const uint8_t *tar_start, size_t tar_length, const char *link_rebase, const char *root_folder);
tart_t inode_getinfo(struct tarfs_inode const * const *index, int idx, size_t *size, time_t *mtime);
void inode_dumphash_sorted(struct tarfs_inode const * const * index, size_t count);
void inode_dumppath_sorted(struct tarfs_inode const * root);
