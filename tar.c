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
#include "hash.h"

/* Checked Tar String == a byte sequence, within TAR image, which is terminated with \0, \r, or \n
 * Unchecked Tar String == a byte sequence, POSIIBLY unterminated
 * C String == a byte sequence, uterminated with \0
 *
 * Strings in TAR image may end with a terminator or may not. For this reason we have bunch of string helpers
 * which emulate string.h behaviour. In case of Unchecked Tar String (UTS) user must provide pointer to the
 * string's explicit end (e.g. tar_strcmp(),  tar_strlen()..).
 */

/*
 * Compare an Unchecked TAR string with a Checked TAR string.
 * s1 -     UTS
 * s1_end - UTS limit
 * s2     - CTS
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
 * strncmp() for two Checked TAR Strings
 *
 */
int tar_strncmp(const char *s1, const char *s2, size_t len) {

    while (len > 0) {

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
        --len;
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
 * Return the length of an Unchecked TAR string.
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
 * Copy CTS to a C string
 *
 */
void tar_strcpy(char *dst, const char *src) {

  while ( true ) {
    if (*src == 0 || *src == '\r' || *src == '\n')
      break;
    *dst++ = *src++;
  }
  *dst = '\0';
}


/*
 * Duplicate an CTS or UTS as a special NUL-NUL-terminated C string.
 *
 * The returned buffer contains one additional byte after the terminating
 * NUL, allowing a single character (typically '/') to be appended without
 * reallocating. This is used during link resolution.
 */
char *tar_strdup1(const char *s1, const char *s1_end) {

  size_t len = tar_strlen(s1, s1_end);

  char *buf = tarfs_os_malloc(len + 1 + 1);

  if (buf != NULL) {
    memcpy(buf,s1,len);
    buf[len+0] = 0;
    buf[len+1] = 0;
  }
  
  return buf;
}



/* Parse an octal number from a TAR field. This is the only function which accepts UTS+len
 * instead of UTS+UTS_end
 * p - UTS
 * max_len - UTS limit.
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

    if (expc != calc)
      return true;
#if CONFIG_TARFS_INTEGRITY
    /* Verify embedded CRC64 sums if they are present */
    if (hdr->md[0] == 'C' && hdr->md[1] == '6' && hdr->md[2] == '4') {
      //log("CRC64\r\n");
    }
#endif

    return false;
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
 * Detect the archive root directory. This function will choose wrong directory name
 * if filesystem is damaged
 *
 * The detected directory is used both as the filesystem mount point and
 * as the path prefix to strip from every inode. 
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

/**
 * Insert CRC64 file checksums into tar archive. This function is used by `tarsum.c` utility
 * only
 */
int tar_addsum(uint8_t *tar_start, size_t tar_length) {

    int inserted = 0;
    uint32_t size;
    size_t off = 0;
    bool bad = 0;
    uintptr_t tar_end = (uintptr_t )(tar_start + tar_length);

    while (off + sizeof(tarhdr_t) <= tar_length) {

        tarhdr_t *hdr = (tarhdr_t *)(tar_start + off);

        if (tar_badhdr(hdr)) {

bad_header:
          if (!bad)
            bad = true;

          off += sizeof(tarhdr_t);
          continue;
        }

        bad = 0;
        size = tar_octal(hdr->size, sizeof(hdr->size));

        /* Check if size is sane: current pointer + 512 bytes + size must be < tar_end */
        if (((uintptr_t)(hdr + 1)) + size >= tar_end)
          goto bad_header;
        

        /* For entries having data (including PAX headers) we calculate CRC64 and inject it
         * into header->padding[] field.
         *
         */
        if (size > 0) {
          uint32_t    new_sum;
          uint8_t     octet;
          char        tmp[9] = { ' ',' ',' ',' ',' ',' ',' ',' ',' '};
          void const *data   = (void *)(hdr + 1);
          uint64_t icv;

          /* calculate integrity check value */
          icv = hash64(0, data, size);

          /* inject it byte by byte in Little Endian byte order */
          for (int i = 0; i < 8; i++) {
            octet = icv & 0xff;
            icv >>= 8;
            hdr->digest[i] = octet;
          }

          /* insert CRC type signature (CRC64)*/
          hdr->md[0] = 'C';
          hdr->md[1] = '6';
          hdr->md[2] = '4';

          hdr->zero = 0;

          /* calculate new header checksum */
          new_sum = tar_hdrsum(hdr);
          
          /* inject it into header in octal ASCII form. */
          snprintf(tmp, sizeof(tmp), "%-8o", new_sum);
          memcpy(hdr->checksum, tmp, 8);
          inserted++;
        }
  
        /* Real size is 512 bytes aligned */
        off += sizeof(tarhdr_t) + (((size_t)size + 511) & ~511u);
    }

    return inserted;
}


/**
 * Verify tarfile CRC64 sums if they are present.
 * @param has_crc true if we are sure that there are CRC64 hashes, or false if we are not sure about it
 */
int tar_verify_crc(uint8_t const *tar_start, size_t tar_length, bool has_crc) {

    uint32_t size;
    size_t off = 0;
    int bad = 0, total_bad = 0;
    uintptr_t tar_end = (uintptr_t )(tar_start + tar_length);

    while (off + sizeof(tarhdr_t) <= tar_length) {

        tarhdr_t const *hdr = (tarhdr_t const *)(tar_start + off);

        if (tar_badhdr(hdr)) {

bad_header:
          //log("CRC64 fail for <offset=%08x>, header and data are damaged\r\n",(uint32_t)off);
          bad++;

          off += sizeof(tarhdr_t);
          continue;
        }

        total_bad += bad;
        bad = 0;

        size = tar_octal(hdr->size, sizeof(hdr->size));

        /* Check if size is sane: current pointer + 512 bytes + size must be < tar_end */
        if (((uintptr_t)(hdr + 1)) + size >= tar_end)
          goto bad_header;
        

        /* For entries having data (including PAX headers) we calculate CRC64 and inject it
         * into header->padding[] field.
         *
         */
        if (size > 0 && (has_crc || (hdr->md[0] == 'C' && hdr->md[1] == '6' && hdr->md[2] == '4'))) {
          
          if (!has_crc) {
            log("A CRC64/ECMA182 record detected, rescanning from the beginning..\n");
            return tar_verify_crc(tar_start, tar_length, true);
          }

          void const *data   = (void *)(hdr + 1);
          uint64_t icv_calc, icv_hdr;

          /* calculate integrity check value */
          icv_calc = hash64(0, data, size);
          memcpy(&icv_hdr, hdr->digest, 8);
          /* TODO: swap the value on big-endian archs */
          if (icv_hdr != icv_calc) {
            //log("CRC64 fail for %s, data is damaged\r\n", hdr->name);
            total_bad++;
          }
        }
  
        /* Real size is 512 bytes aligned */
        off += sizeof(tarhdr_t) + (((size_t)size + 511) & ~511u);
    }

    return total_bad;
}

