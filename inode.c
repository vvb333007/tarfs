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

#if CONFIG_HAVE_READLINK
#  error Not yet
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "inode.h"
#include "hash.h"
#include "fs.h"
#include "tar.h"
#include "hash.h"
#include "inode.h"


static const char *remove_subpath(const char *path, const char *path_end, const char *subpath) {

  const char *text = path;

  while((uintptr_t)text < (uintptr_t)path_end &&
        *text &&
        *subpath &&
        *text == *subpath) {
    text++;
    subpath++;
  }

  if (*subpath != 0)  /* subpath failed: prefix differs */
    text = path; 

  return text;
}




/**
 * returns pointer inside buffer (zero-copy)
 * or NULL if not found
 * Used to parse PAX-Header data section which is key=value format:
 *
 * 20 path=/some/data
 * 20 linkpath=/some/data
 *
 * Here we depend on \n or \r at the end of every key=value pair. 
 */
static const char* path_from_pax_header(const char *buf, size_t size, const char *templ) {

    size_t line_start,i = 0, j, digi = 0;
    const char *line;
    int templ_len = strlen(templ);

    while (i < size)
    {
        // 1. parse length prefix
        size_t len = 0;

        // read until space
        j = i;
        digi = 0; /* how many digits was in the number */

        while (j < size && buf[j] != ' ')
        {
            if (buf[j] < '0' || buf[j] > '9')
                return NULL;
            len = len * 10 + (buf[j] - '0');
            j++;
            digi++;
        }

        if (j >= size || buf[j] != ' ')
            return NULL;

        line_start = j + 1;
        if (line_start >= size)
            return NULL;

        if (line_start + len > size)
            return NULL;

        // line = buf[line_start .. line_start+len)

        line = buf + line_start;

        // 2. check prefix "path="
        if (len >= templ_len && memcmp(line, templ, templ_len) == 0)
        {
            // Check if found key value ends with \r \n or NUL within the buf
            bool good_line = false;
            for (int i=templ_len; i < (len - digi - 1); i++)
              if (line[i] == 0 || line[i] == '\r' || line[i] == '\n') {
                good_line = true;
                break;
              }
  
            if (!good_line)
                log("BAD/MALFORMED archive\r\n");
  
            return good_line ? line + templ_len : NULL; // value starts here
        }

        // 3. jump to next record
        i = line_start + len - digi - 1;

    }

    return NULL;
}

/**
 * Inode is represented by struct tarfs_inode; Every inode contains a pointer to a corresponding tarfile entry
 * and a hashed filename (32bit FNV1a hash). Sorted inodes reside in the fs->fs_ino array and are the primary
 * indexing mechanism. Binary array search is used for paths lookup (i.e. 16 lookups at worst for 65535 files)
 */


/** 
 * Comparator function for our array sortin routine;
 * Compare two inodes 
 */
static int inode_compare(const struct tarfs_inode *a, const struct tarfs_inode *b) {

    if (a->in_hash < b->in_hash) return -1;
    if (a->in_hash > b->in_hash) return 1;

    return 0;
}

/**
 * Exchange two node indicies 
 */
static inline void inode_exchange(struct tarfs_inode **a, struct tarfs_inode **b) {

    struct tarfs_inode *t = *a;
    *a = *b;
    *b = t;
}


/**
 *
 * Sift subarray down
 */
static void inode_siftdown(struct tarfs_inode **v, size_t root, size_t end)
{
    while (1) {
        size_t child = root * 2 + 1;

        if (child >= end)
            return;

        if (child + 1 < end && inode_compare(v[child], v[child + 1]) < 0)
            child++;

        if (inode_compare(v[root], v[child]) >= 0)
            return;

        inode_exchange(&v[root], &v[child]);
        root = child;
    }
}

/**
 * Sort inode indicies. Sorting is done by ->in_hash member
 */
void inode_sort(struct tarfs_inode **iarr, size_t count) {

    struct tarfs_inode **v = (struct tarfs_inode **)iarr;

    if (count < 2)
        return;

    for (size_t i = count / 2; i-- > 0;)
        inode_siftdown(v, i, count);

    for (size_t end = count; end > 1; end--) {

        inode_exchange(&v[0], &v[end - 1]);
        inode_siftdown(v, 0, end - 1);
    }
}





/**
 * Alphaberical order sorting routines
 * Inodes once created are "position-immutable": that means, inode, once populated stays where she is
 * and only index pointers are manipulated to get a sorted array.
 *
 * In a similar way inode's in_next field is used to create an alphasorted list (singly-linked list)
 */

static struct tarfs_inode *merge(struct tarfs_inode *a, struct tarfs_inode *b) {

    struct tarfs_inode *head = NULL;
    struct tarfs_inode **tail = &head;

    while (a && b) {

        if (tar_strcmp((char const *)a->in_path, NULL,(char const *)b->in_path) <= 0) {
            *tail = a;
            a = a->in_next;
        } else {
            *tail = b;
            b = b->in_next;
        }

        tail = &(*tail)->in_next;
    }

    *tail = a ? a : b;

    return head;
}

/*--------------------------------------------------------------------------*/

static struct tarfs_inode *merge_sort(struct tarfs_inode *head) {

    if (!head || !head->in_next)
        return head;

    /* найти середину списка */
    struct tarfs_inode *slow = head;
    struct tarfs_inode *fast = head->in_next;

    while (fast && fast->in_next) {
        slow = slow->in_next;
        fast = fast->in_next->in_next;
    }

    struct tarfs_inode *right = slow->in_next;
    slow->in_next = NULL;

    struct tarfs_inode *left = merge_sort(head);
    right = merge_sort(right);

    return merge(left, right);
}

/*--------------------------------------------------------------------------*/
struct tarfs_inode *inode_alphasort(struct tarfs_inode *array, size_t count) {

    if (count == 0)
        return NULL;

    /* превращаем массив в список */
    for (size_t i = 0; i + 1 < count; i++)
        array[i].in_next = &array[i + 1];

    array[count - 1].in_next = NULL;

    return merge_sort(array);
}

/**
 * Returns an array of pointers to tarfs_inode structures.
 * Inodes are allocated and initialized to all zeros, array of pointers is allocated in populated
 * with pointers to individual inodes
 *  Memory layout, single chunk:
 *      0x3fc00000 .. [[index][inodes]] ..3fcxxxxx ---> memory grows this way
 *
 */
struct tarfs_inode **inode_alloc(size_t count) {

    char *ptr;
    struct tarfs_inode *nodes, **index;
    size_t index_size, nodes_size;


    if (count < 1) {
      errno = EINVAL;
      return NULL;
    }

    index_size = count * sizeof(struct tarfs_inode *);
    nodes_size = count * sizeof(struct tarfs_inode);

    if (NULL == (ptr = tarfs_calloc(1, index_size + nodes_size))) {
      errno = ENOMEM;
      return NULL;

    }

    index = (struct tarfs_inode **)ptr;
    nodes = (struct tarfs_inode *)(ptr + index_size);

    /* After creation, all index elements are pointing to the corresponding inode structure:
     * index[0] --> inode[0], index[10] --> inode[10]
     */
    for (size_t i = 0; i < count; i++)
      index[i] = &nodes[i];

    log("Allocated %lu inodes, %lu bytes (%lu + %lu)\r\n",count,index_size + nodes_size,index_size, nodes_size);
    return index;
}

/**
 * Free inodes. All memory associated with inodes is freed: inodes and index array.
 * Some inodes may have allocated names. There is no way to distiguish allocated name
 * from a TAR name, except for checking pointer address to NOT BE in the mmaped TAR file
 *
 */
void inode_free(struct tarfs_inode **index, size_t count, uintptr_t tar_start, size_t tar_length) {

  if (index != NULL) {
    for (int i = 0; i < count; i++) {
      if (index[i]->in_path != 0) {
        /* address is from the tar file range or beyond? */
        if (index[i]->in_path < tar_start || index[i]->in_path >= (tar_start + tar_length)) {
          tarfs_os_free((void *)(index[i]->in_path));
        }
      }
    }
    tarfs_os_free(index);
  }
}

/**
 * Check if given inode is exactly given path. This function is used as final check as collision avoidance
 * step after hash-based binary search. Unless we are using perfect hash (which should be possible to generate 
 * on mount()) we have to verify our search results.
 */
static bool inode_pathcmp(const struct tarfs_inode *inode, const char *src) {

  if (inode == NULL || src == NULL)
    return NULL;

  //struct tarhdr const *in_vaddr = (struct tarhdr const *)inode->in_vaddr;
  const char *in_path = (const char *)inode->in_path;

  if (in_path == NULL)
    return false;

  while(*in_path == *src) {
      
      if (*src == 0)
        return true;

      src++;
      in_path++;
  }

  return (*src == 0 && (*in_path == '\r' || *in_path == '\n' || *in_path == '\0'));
}




/**
 * Find an inode that corresponds to given path name. Performs a binary search in the array
 * of pointers to struct tarfs_inode (`index array`). The array is sorted by ->in_hash
 *
 * Inode pointer can be retrieved as `node_ptr = index[i]` where `i` is node index
 * as returned by inode_lookup()
 */
int inode_lookup(struct tarfs_inode const * const *index, size_t num_inodes, const char *path) {


  uint32_t hash;
  size_t   left,
           right;

  if (index == NULL || path == NULL || num_inodes < 1)
    return -EINVAL;
  
  left = 0;
  right = num_inodes;
  hash = hash32(HASH32_IV, (uint8_t const *)path, strlen(path));

  while (left < right) {

    size_t   mid      = left + ((right - left) >> 1);
    uint32_t mid_hash = index[mid]->in_hash;

    if (hash < mid_hash)
      right = mid;
    else if (hash > mid_hash)
      left = mid + 1;
    else {

      size_t first;

     /* Alright, walk left, looking for the first inode with the same in_hash. Once found we walk from
      * the left to the right comparing string literals
      */
      first = mid;
      while (first > 0 && index[first - 1]->in_hash == mid_hash)
        first--;


      /* Scan collision chain, left to right */
      for (int i = first;  i < num_inodes && index[i]->in_hash == hash; i++) {
        //struct tarhdr *hdr = (struct tarhdr *)index[i]->in_vaddr;
        if (inode_pathcmp(index[i], path)) {
          log("found inode#%u, hash=<e8bb5ed2> path='%s'\r\n", i, path);
          return i;
        }
      }

      log("unresolved collision, path='%s'\r\n", path);
      break;
    }
  } /* while left < right */
  log("hash=<%08x> path='%s' not found\r\n",hash, path);
  return -ENOENT;
}

/**
 *  inode_getinfo() : get inode's Type, Size and Mtime
 * These are not precached and must be calculated every time. Having this information in the tarfs_inode
 * will increase the size of inode list dramatically
 *
 * @param fs a pointer to mounted FS
 * @param idx inode of intereset (inode index)
 * @param size if not NULL, provides the location to store the entry size
 * @param mtime if not NULL, provides the location to store the entry mtime
 * @return Entry type (e.g. TART_FILE or TART_DIR etc). If return value is TART_BAD, then this is an
 *         indication of an invalid/unusable inode
 */
tart_t inode_getinfo(struct tarfs_inode const * const *index, int idx, size_t *size, time_t *mtime) {

  if (index != NULL) {
    struct tarfs_inode const *ino = index[idx];
    struct tarhdr const *hdr;

    if (NULL != (hdr = (struct tarhdr const *)ino->in_dvaddr)) {

       if (size != NULL)
         *size = tar_octal(hdr->size, sizeof(hdr->size));

       if (mtime != NULL)
         *mtime = tar_octal(hdr->mtime, sizeof(hdr->mtime));

        return hdr->type;
    }
  }
  return TART_BAD;
}

/* Return raw inode type: TART_DIR, TART_FILE or TART_BAD. Links are resolved to their
 * final destination. To check if inode is a link, use inode_islink() function instead
 *
 */
tart_t inode_rawtype(struct tarfs_inode const *ino) {

  if (ino != NULL) {
    struct tarhdr const *hdr;

    if (NULL != (hdr = (struct tarhdr const *)ino->in_dvaddr))
      return hdr->type;
  }
  return TART_BAD;
}

/* Check if inode ino is a link or not.
 *
 */
bool inode_islink(struct tarfs_inode const *ino) {

  if (ino == NULL)
    return false;

  struct tarhdr const *hdr = (struct tarhdr const *)ino->in_vaddr;
  return hdr->type == TART_SYMLINK || hdr->type == TART_HARDLINK;
}



/* 
 *
 */
time_t inode_mtime(struct tarfs_fs *fs, int idx, size_t *size) {

  if (fs != NULL && idx >= 0 && idx < fs->fs_nino) {

      struct tarfs_inode const *ino;

      if (NULL != (ino = fs->fs_ino[idx])) {

        struct tarhdr const *hdr;

        if (NULL != (hdr = (struct tarhdr const *)ino->in_dvaddr)) {

          if (size != NULL)
            *size = tar_octal(hdr->size, sizeof(hdr->size));

          return hdr->type;
        }
      }
  }
  return TART_BAD;
}


/* Scan through inodes, find all inodes with types 1 and 2 (links)
 * and resolve them to their final destination (type=0 File or type=5 Directory)
 */
int inode_resolve(struct tarfs_inode **index, size_t count) {

  int floating = 0, resolved = 0, attempted = 0;

#if CONFIG_HAVE_READLINK
  int links = 0;

  /* count total number of links. this could be done in inode_populate() but
   * it is too complex already. ENOMEM on building link descriptors database is not a big deal - we continue
   * to mount, just disable readlink()
   */
  for (int i = 0; i < count; i++ ) {
    if (index[i]->in_next != 0) /* inode_populate() put link_name here for links and 0 for everything else
                                   inode_resolve() clears this field. Later, alphasorting routine start to use this field
                                 */
      links++;
  }
  log("total %d links", links);

  struct tarfs_link *ldesc = tarfs_os_malloc(links * sizeof(struct tarfs_links));
  if (ldesc != NULL) {
    memset(ldesc,0, links * sizeof(struct tarfs_links));
  }

  int ln = 0;
#endif /* CONFIG_HAVE_READLINK */

  for (int i = 0; i < count; i++ ) {

    char *link_name = (char *)index[i]->in_next;

    if (link_name != NULL) {

      attempted++;

      index[i]->in_dvaddr = 0;

#if CONFIG_HAVE_READLINK
      const char *p = (const char *)index[i]->in_path;
      ldesc[ln].li_hash = hash32(HASH32_IV, p, tar_strlen(p, NULL));
      ldesc[ln].li_src  = p;
      ldesc[ln].li_dest = (const char *)link_name;

      /* prevent pointer deletion: link_name is sytdup()ed memory which
       * is freed at the end of this function. Since we transferred memory owbership to ldesc,
       *lets zeroize corresoinding field
       */
      index[i]->in_next = 0;
      ln++;
#endif /* CONFIG_HAVE_READLINK */

      int depth = 16;
      do {
        int dest = inode_lookup((struct tarfs_inode const * const *)index, count, link_name);
        /* This ugly two-times lookup is required 
         * if we want to support all kind so Windows links/directory junctions/sym/hardlinks and
         * all other entry types.
         * The problem is that TAR utility stores directories with a trailing slash while
         * PAX-symlinks do not end with "/" even if they point to a directory
         */
        if (dest < 0) {
          int last_byte = strlen(link_name);
          /* Link name (in_next) is created with tar_strdup1() which guarantees TWO NUL at the end,
           * so adding a slash at position of the first NUL is completely safe. We modify inode's field here
           * so subsequent lookups should be fine
           */
          link_name[last_byte] = '/';
          dest = inode_lookup((struct tarfs_inode const * const *)index, count, link_name);
          if (dest < 0) {
            
            log("failed to resolve '%s' in two attempts\r\n", link_name);
            break;
          }
        }
        tart_t type = inode_getinfo((struct tarfs_inode const * const *)index, dest, NULL, NULL);
        if (type == TART_BAD) {
          log("can not get info on inode %d\r\n", dest);
          break;
        }
          
        if (type != TART_HARDLINK && type != TART_SYMLINK) {
#if CONFIG_TARFS_LOG
          log("inode_resolve() : %d->%d, final destination ", i, dest);
          tar_print((const char *)index[dest]->in_path, NULL);
          puts("");
#endif
          index[i]->in_dvaddr = index[dest]->in_dvaddr;
          resolved++;
          break;
        }

        link_name = (char *)index[dest]->in_next;
        log("link to a link, continuing  to resolve..\r\n");

      } while(--depth > 0);

      if (index[i]->in_dvaddr == 0) {
        log("inode #%d is a floating link\r\n",i);
        floating++;
      }

    }
  }

  log("memory cleanup, release unneded memory chunks\r\n");

  for (int i = 0; i < count; i++ ) {
    
    const char *link_name = (const char *)index[i]->in_next;
    if (link_name != NULL) {
      index[i]->in_next = NULL;
      tarfs_os_free((void *)link_name);
    }
  }

  log("%u of %u links were resolved, floating inodes: %u\r\n",resolved, attempted, floating);
  return 0;
}

static char const * s_bad_path = "<bad path>";

/**
 * Populate inodes. Inodes must be allocated, and the allocation size must match
 * real amount of inodes, which can be obtained through tar_getnino
 *
 * @return Total size of all files containing data
 */
size_t inode_populate(struct tarfs_inode *inodes, size_t nino, const uint8_t *tar_start, size_t tar_length, const char *link_rebase, const char *root_folder) {

    uint32_t total_data_size = 0;
    uint32_t total_headers_size = 0;
    uint32_t overhead = 0;

    int files = 0, dirs = 0, links = 0, pax_headers = 0, idx = 0, bad_path = 0;
    
    
    size_t off = 0, bad_start;
    size_t hdr_no = 0;
    unsigned int bad = 0, total_bad = 0;
    const char *pax_entry_path = NULL, *pax_entry_link = NULL, *pax_entry_end;
    uintptr_t tar_end = (uintptr_t )((const uint8_t *)tar_start + tar_length);


    /* Basic overhead created by inodes and the inode index + FS descriptor */
    overhead = sizeof(struct tarfs_fs) + nino * (sizeof(struct tarfs_inode *) + sizeof(struct tarfs_inode));

    while (off + sizeof(tarhdr_t) <= tar_length) {

        const tarhdr_t *hdr = (const tarhdr_t *)(tar_start + off);

        if (tar_badhdr(hdr)) {

          pax_entry_path = NULL;
          pax_entry_link = NULL;

          if (!bad) {
            log("Header #%lu is invalid (or NUL-header)\r\n", hdr_no);
bad_header:
            log("Skipping blocks starting from offset %lu..\r\n", off);
            bad++;
            bad_start = off;
          }

          off += sizeof(tarhdr_t);
          continue;
        }

        if (bad) {
          log("Resuming at offset %lu; (%lu blocks/ %lu bytes) were lost \n", off, (off - bad_start)/sizeof(struct tarhdr), off - bad_start);
          total_bad += bad;
          bad = 0;
          
        }

        uint64_t size = tar_octal(hdr->size, sizeof(hdr->size));

        /* Check if size is sane: current pointer + 512 bytes + size must be < tar_end */
        if (((uintptr_t)(hdr + 1)) + size >= tar_end) {
          log("Invalid entry size, sector marked as bad\r\n");
          goto bad_header;
        }

        total_headers_size += 512;


        /* Only create inodes of type FILE, SYMLINK, HARDLINK and DIRECTORY*/
        switch(hdr->type) {
          case TART_AFILE:
          case TART_CONT:
          case TART_FILE:
                files++;
                break;

          case TART_HARDLINK:
          case TART_SYMLINK:
                links++;
                break;

          case TART_DIR:
                dirs++;
                break;

          case TART_PAX_G:
          case TART_PAX:
                pax_headers++;
                total_headers_size += size;
                goto is_pax;

          default:
                goto skip_header_and_data;
        };

        total_data_size += size;



        /*All checks are done, start populating inode.
         * inodes[idx].in_path = (uintptr_t)strdup("Hello Hello");
         */


        /* NAME */
        if (pax_entry_path) {
          pax_entry_path = remove_subpath(pax_entry_path, pax_entry_end, root_folder);
          //tar_print(pax_entry_path, pax_entry_end);

          inodes[idx].in_path = (uintptr_t)pax_entry_path; // path_from_pax_header() guarantees that there is a path terminator (\r, \n or \0)

          pax_entry_path = NULL;

        } else {

          /* No PAX override - fallback to prefix/name logic:
           * If we do have non-empty prefix, then we have the worst case scenario #1, which requires
           * us to malloc() a linear buffer and reconstruct full path there.
           */
          if (hdr->prefix[0]) {
              char tmp[sizeof(hdr->prefix) + sizeof(hdr->name) + 1 + 1];

              int nlen = tar_strlen(hdr->name, &hdr->name[0] + sizeof(hdr->name));
              int plen = tar_strlen(hdr->prefix, &hdr->prefix[0] + sizeof(hdr->prefix));

              memcpy(tmp,hdr->prefix,plen);
              tmp[plen] = '/';
              memcpy(tmp + plen + 1,hdr->name,nlen);
              tmp[plen+nlen+1] = '\0';

              const char *t = remove_subpath(tmp, tmp+sizeof(tmp), root_folder);

              inodes[idx].in_path = (uintptr_t)tarfs_strdup(t);
              overhead += strlen(t);

          } else {
              /* Entry name is exactly 100 bytes long: this is the worst case scenario #2: that means
               * we do not have any path terminator in our hdr->name. This is quite rare case so we 
               * simply strdup() this kind of strings
               */
              const char *reb = remove_subpath(hdr->name, &hdr->name[0] + sizeof(hdr->name), root_folder);

              if (hdr->name[sizeof(hdr->name) - 1] != 0) {
                
                inodes[idx].in_path = (uintptr_t )tar_strdup1(reb , &hdr->name[0] + sizeof(hdr->name));
                overhead += tar_strlen(reb , &hdr->name[0] + sizeof(hdr->name)); 

              } else {
                inodes[idx].in_path = (uintptr_t)reb;
              }
              //tar_print(reb, (char *)(hdr->name) + sizeof(hdr->name));
          }
        }


        /* LINK 
         * Temporary use in_next as a pointer to the link name.
         * We will be freed in link resolution pass (inode_resolve()). 
         * Skipping inode_resolve() step may create memory leaks
         */

        if (hdr->type == TART_SYMLINK || hdr->type == TART_HARDLINK) {

            
          /* PAX has preference over link_name field */
          if (pax_entry_link) {

            if (pax_entry_link[0] == '/')
              pax_entry_link = remove_subpath(pax_entry_link, pax_entry_end, link_rebase);
            else
              pax_entry_link = remove_subpath(pax_entry_link, pax_entry_end, root_folder);

            inodes[idx].in_next = (void *)tar_strdup1(pax_entry_link, pax_entry_end);

            pax_entry_link = NULL;
          
          } else {
            const char *t =  remove_subpath(hdr->link_name, (char *)(hdr->link_name) + sizeof(hdr->link_name), hdr->type == TART_SYMLINK ? link_rebase : root_folder); //XXX: ugly hack
            inodes[idx].in_next = (void *)tar_strdup1(t, (char *)(hdr->link_name) + sizeof(hdr->link_name));
    //        tar_print(t, (char *)(hdr->link_name) + sizeof(hdr->link_name)); 
          }
        }

        /* Mark node as invalid if we had problems with in_path (e.g. out of memory on strdup etc).
         * We do not want to check in_path for validity each time we want to use it.
         */
        if (inodes[idx].in_path == 0) {
          inodes[idx].in_path = (uintptr_t )s_bad_path;
          inodes[idx].in_hash = 0 /* hash that does not match inode's name. will be rejected at inode_lookup */;
          bad_path++;
        } else {

          /* Previous code guarantees that in_path field has a string terminator (NUL, CR or LF)
           * so it is safe to cal tar_strlen with the second argument set to NULL
           */
          int path_len = tar_strlen((const char *)inodes[idx].in_path, NULL);
          inodes[idx].in_hash = hash32(HASH32_IV, (uint8_t const *)inodes[idx].in_path, path_len);
        }

        inodes[idx].in_vaddr = (uintptr_t)hdr;
        inodes[idx].in_dvaddr = (uintptr_t)hdr;

        /* go to the next inode index */
        idx++;


        /**/
is_pax:
        if (hdr->type == TART_PAX) {
          /* both of these can be NULL. that simply means X record will be ignored */
          pax_entry_path = path_from_pax_header((const char *)(hdr + 1), size, "path=");
          pax_entry_link = path_from_pax_header((const char *)(hdr + 1), size, "linkpath=");
          pax_entry_end = ((const char *)(hdr + 1)) + size;
        }
skip_header_and_data:

        /* Real size is 512 bytes aligned */
        off += sizeof(tarhdr_t) + (((size_t)size + 511) & ~511u);
        hdr_no++;
    }

    log("end of file reached\r\n");
    if (total_bad)
      log("%u blocks (%u bytes) were skipped\n", total_bad, total_bad * 512);
      

  log("TAR archive has %u files, %u links and %u dirs (%u PaxHeaders)\r\n", files, links, dirs, pax_headers);  
  log("TAR data/headers ratio: %u data bytes, %u header bytes\r\n", total_data_size, total_headers_size);  
  log("RAM overhead (total RAM used by the FS): %u bytes\r\n", overhead);

  return total_data_size;
}


/* Free all inodes and associated data
 *
 */
void inode_unmount(struct tarfs_fs *fs, const void * tar_start, size_t tar_size) {

  if (fs != NULL && fs->fs_ino != NULL) {
    log("free inodes\r\n");
    inode_free((struct tarfs_inode **)fs->fs_ino, fs->fs_nino, (uintptr_t )tar_start, tar_size);
  }
}

/* Create inodes (filesystem index), perform all sortings, link resolution and etc
 * to make things faster later
 */
int inode_mount(struct tarfs_fs *fs, const unsigned char *buf, size_t size, const char *rebase_link, const char *base_dir) {


    int nino;

    fs->fs_vaddr= buf;
    fs->fs_size = size;
    fs->fs_dsize= 0;
    fs->fs_ino  = NULL;
    fs->fs_nino = 0;
    fs->fs_root = NULL;


    // PASS1: count inodes, count all required memory
    log("PASS1, analyzing..\n");
    nino = tar_getnino(buf, size);

    log("%u inodes, expected RAM usage: %lu bytes of RAM\n",nino, sizeof(struct tarfs_fs) + nino * (sizeof(struct tarfs_inode) + sizeof(struct tarfs_inode *)));
    if (nino < 1)
      return -1;

    // PASS2: guess TARFS root (will be the mountpoint)
    log("PASS2, analyzing..\n");
    log("filesystem prefix '%s' \n", base_dir);

    struct tarfs_inode **index = inode_alloc( nino );
    struct tarfs_inode *inodes = (struct tarfs_inode *)(index + nino);

    if (index != NULL) {

      // PASS3: populate inodes
      log("PASS3, populating inodes..\n");
      size_t dsize = inode_populate(inodes, nino, buf, size, rebase_link, base_dir);


      /* Sort inode index table (pointers to inodes are sorted by inode's hash value)
       * so inode_lookup() can be used
       */
      log("building binary search index..\n");
      inode_sort(index, nino);

      /*Resolve symlinks and hardlinks; For inodes which can not be resolved to a valid type5 or type0
      * tar entries, the corresponding ->in_dvaddr is set to NULL, indicating that this inode has no valid data.
      * Resolve other dpendencies; Free tmp memory used by linkpath strings. Link path pointer is placed to the in_next
      * by the code above and is freed by the inode_resolve().
      * 
      */
      log("symlinks and hardlinks resolution..\n");
      inode_resolve(index, nino);

      /* 
       * Perform alphasorting by the in_path; index is not changed, only ->in_next is manipulated
       * to build an alphasorted list of entries.
       * root
       */
      log("lexigraphical sorting..\n");
      struct tarfs_inode *root = inode_alphasort(inodes, nino);

      /* PUBLISH */

      fs->fs_ino   = (struct tarfs_inode const * const *)index;
      fs->fs_nino  = nino;
      fs->fs_root  = (struct tarfs_inode const * )root;
      fs->fs_dsize = dsize;

      if (root != NULL) {
      
        log("root inode is exp=<2a0c975e>, real=<%08x> \"%s\"\r\n",root->in_hash, (const char *)root->in_path);
        inode_dumppath_sorted(root);
        return 0;
      }

      log("WARNING: no root inode after alphasort, opendir() is disabled\r\n");
      inode_dumphash_sorted((struct tarfs_inode const * const * )index, nino);
    }      

    return -1;
}


/**
 *
 *
 */
void inode_dumphash_sorted(struct tarfs_inode const * const * index, size_t count) {

  log("-- HASH SORTED INODES --\r\n");
#if CONFIG_TARFS_LOG

  for (size_t i = 0; i < count; i++) {
    struct tarfs_inode const *inode = (struct tarfs_inode const *)index[i];

    printf("<%08x> %c %s path=", inode->in_hash,
      inode_getinfo(index, i, NULL, NULL) ,
      inode->in_vaddr != inode->in_dvaddr ? "*" : " ");

    tar_print((char const *)inode->in_path, NULL);

    puts("");
  }
#endif
}

/**
 *
 *
 */
void inode_dumppath_sorted(struct tarfs_inode const * root) {

  log("-- ALPHA SORTED INODES --\r\n");

#if CONFIG_TARFS_LOG

  for (size_t i = 0; root != NULL; i++) {

    printf("<%08x> %s%s path=", 
            root->in_hash,
            root->in_vaddr != root->in_dvaddr ? "*" : " ",
            root->in_dvaddr == 0 ? "X" : " ");

    tar_print((char const *)root->in_path, NULL);

    puts("");

    root = root->in_next;
  }
#endif
}
