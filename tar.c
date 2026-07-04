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
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "os.h"
#include "fs.h"
#include "tar.h"


/*
 * Compare a TAR string with a regular C string.
 *
 */
int tar_strcmp(const char *s1, const char *s1_end, const char *s2) {

    while (1) {


        if (s1_end != NULL && s1 >= s1_end)
          break;

        unsigned char c1 = (unsigned char)*s1;
        unsigned char c2 = (unsigned char)*s2;

        if (c1 == '\0' || c1 == '\r' || c1 == '\n')
            c1 = '\0';

        if (c2 == '\0' || c2 == '\r' || c2 == '\n')
            c2 = '\0';

        if (c1 != c2)
            return (int)c1 - (int)c2;

        if (c1 == '\0')
            return 0;

        ++s1;
        ++s2;
    }

   /* Reached the explicit end of the TAR field.
    * Treat it as an implicit end-of-string. 
    */
    unsigned char c2 = (unsigned char)*s2;

    if (c2 == '\r' || c2 == '\n')
        c2 = '\0';

    return -(int)c2;
}

/*
 * Return the length of a TAR string.
 *
 */
int tar_strlen(const char *s1, const char *s1_end) {

  const char *c = s1;

  while ( true ) {
    if (s1_end != NULL && c >= s1_end)
      break;
    if (*c == 0 || *c == '\r' || *c == '\n')
      break;
    c++;
  }

  s1_end = c;

  return s1_end - s1;
 
}

/*
 * Duplicate a TAR string as a special NUL-NUL-terminated C string.
 *
 * The returned buffer contains one additional byte after the terminating
 * NUL, allowing a single character (typically '/') to be appended without
 * reallocating. This is used during link resolution.
 */
char *tar_strdup1(const char *s1, const char *s1_end) {

  size_t len = tar_strlen(s1, s1_end);

  char *buf = malloc(len + 1 + 1);

  if (buf != NULL) {
    memcpy(buf,s1,len);
    buf[len+0] = 0;
    buf[len+1] = 0;
  }
  
  return buf;
}



/* Parse an octal number from a TAR field.
 *
 */
uint32_t tar_octal(const char *p, size_t max_len) {

  uint32_t value = 0;

  /* Skip leading spaces */
  while(max_len > 0 && *p == ' ') {
    p++;
    max_len--;
  }

  /* shift-add approach */
  while (max_len > 0 && *p >= '0' && *p <= '7' ) {

    value = (value << 3) | (*p - '0');
    p++;
    max_len--;
  }
  return value;
}


/* Calculate and verify the header checksum.
 *
 * TAR checksum rules require the checksum field itself to be treated
 * as eight ASCII spaces while the checksum is being calculated.
 *
 * The checksum field occupies bytes [148..156).
 */
uint32_t tar_hdrsum(tarhdr_t const * hdr) {

    uint8_t const *p = (uint8_t const *)hdr;
    uint32_t calc = 0;                         /* checksum calculated from header contents */

    for (size_t i = 0; i < sizeof(tarhdr_t); i++)
      if (i >= 148 && i < (148 + 8))
        calc += ' ';
      else
        calc += p[i];

    return calc;
}



/** Validate a TAR header.
 *
 * Checks:
 *   - entry type is supported/recognized;
 *   - reserved padding byte is zero;
 *   - checksum field contains only valid TAR characters;
 *   - header checksum matches the calculated value.
 *
 * @param hdr Pointer to a TAR header.
 * @return true if the header is invalid, corrupted, or unsupported.
 *         In this case no inode should be created for this entry.
 */
bool tar_badhdr(tarhdr_t const * hdr) {

    uint8_t const *p = (uint8_t const *)hdr;
    uint32_t calc = 0,                         /* checksum calculated from header contents */
             expc;                             /* checksum stored in the TAR header */

    /* Quick sanity check.
     *
     * If the entry type is unknown, the header is either corrupted or
     * describes a TAR feature we do not support. In either case, treat
     * it as an unreadable entry and skip it.
     */
    switch (hdr->type) {
      case TART_AFILE:
      case TART_PAX:
      case TART_PAX_G:
      case TART_FILE ... TART_CONT:
        /* Supported TAR entry type. */
        break;
      default:
        return true;
    };

    /* Verify the reserved padding byte.
     *
     * POSIX TAR requires this byte to be zero. We also rely on this
     * guarantee because it ensures that the preceding 'prefix' field
     * is NUL-terminated.
     */
    if (hdr->zero != 0)
      return true;

    calc = tar_hdrsum(hdr);
    expc = tar_octal(hdr->checksum, sizeof(hdr->checksum));

    return expc != calc;
}

/**
 * Quick run through the tarfile to count number of inodes we have to create
 * Bad blocks are skipped; inodes_count = number_of_links + number_files + number_of_directoris
 *
 */
int tar_getnino(const uint8_t *tar_start, size_t tar_length) {

    uint32_t size;
    size_t off = 0, hdr_no = 0;
    int bad = 0, files = 0, dirs = 0, links = 0, bad_total = 0;
    uintptr_t tar_end = (uintptr_t )((const uint8_t *)tar_start + tar_length);

    while (off + sizeof(tarhdr_t) <= tar_length) {

        const tarhdr_t *hdr = (const tarhdr_t *)(tar_start + off);

        if (tar_badhdr(hdr)) {

          if (!bad) {
bad_header:
            bad++;
            
          }

          off += sizeof(tarhdr_t);
          continue;
        }

        if (bad) {
          bad_total += bad;
          bad = 0;
        }

        size = tar_octal(hdr->size, sizeof(hdr->size));

        /* Check if size is sane: current pointer + 512 bytes + size must be < tar_end */
        if (((uintptr_t)(hdr + 1)) + size >= tar_end) {
          goto bad_header;
        }

        switch(hdr->type) {
          case TART_AFILE:
          case TART_CONT:
          case TART_FILE: files++; break;

          case TART_SYMLINK:
          case TART_HARDLINK: links++; break;

          case TART_DIR: dirs++; break;

          default:
        };

  
        off += sizeof(tarhdr_t);


        /* Real size is 512 bytes aligned */
        off += ((size_t)size + 511) & ~511u;

        hdr_no++;
    }

  return files + links + dirs;

}

/**
 * Detect the archive root directory.
 *
 * The detected directory is used both as the filesystem mount point and
 * as the path prefix to strip from every inode. For example, if the archive
 * contains:
 */
bool tar_rootdir(const uint8_t *tar_start, size_t tar_length, char *base_dir, size_t out_len) {

  uint32_t size;
  size_t off = 0;
  uintptr_t tar_end = (uintptr_t )((const uint8_t *)tar_start + tar_length);

  while (off + sizeof(tarhdr_t) <= tar_length) {

    const tarhdr_t *hdr = (const tarhdr_t *)(tar_start + off);

    size = 0;
    if (!tar_badhdr(hdr)) {

        size = tar_octal(hdr->size, sizeof(hdr->size));

        if (((uintptr_t)(hdr + 1)) + size < tar_end) {


          if (hdr->type == TART_DIR) {
            if (hdr->name[0]) {

              //printf("A candidate found at %p\r\n", hdr);
              /* Check if guessed root is a valid mountpoint name (latin1, <17 bytes)*/
              int l = tar_strlen(hdr->name, &hdr->name[0] + sizeof(hdr->name));
              if (l < sizeof(hdr->name) && l <= tarfs_os_mp_maxlen() && l < out_len) {
                memcpy(base_dir, hdr->name, l - 1 );
                base_dir[l-1] = 0;
                return true;
              }
            }
          }

        } else
            size = 0;
    }

    off += (((size_t)size + 511) & ~511u) + sizeof(tarhdr_t);
  }
  return false;
}


/* Displays string `buf`, which may or may not end with NUL: the line end
 * markers are \r, \n and \0.
 *
 * We dont go past the `end` pointer even if we didnt find any line 
 * terminator from the list above
 */
void tar_print(const char *buf, const char *end) {

  int len = 0;
  const char *text = buf;

  if (end == NULL)
    end = (const char *)(-1);

  /* Calculate remaining string length */
  for (; (text + len) < end; len++) {
    if (text[len] == '\0' || text[len] == '\r' || text[len] == '\n' )
      break;
  }

  /* XXX: get rid of stack buffer here */
  char b[len + 1];
  memcpy(b,text,len);
  b[len] = 0;

  printf("%s",b);
}

