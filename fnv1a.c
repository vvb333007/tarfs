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


/**
 *  This implementation is an optimized variation of the FNV-1a hash algorithm.
 *  This is NOT a strict reference implementation of FNV, but preserves
 *  byte-equivalence of FNV-1a while improving memory throughput.
 *
 * const char *test_vector3 = "The_Prefix/The_Name.txt";
 * const uint32_t ref_value = 0x6f914293;
 *
 * @file hash.c
 * @brief Fowler–Noll–Vo (FNV-1a) 32-bit hash implementation (optimized stream version)
 *
 *
 */

#include <stdint.h>
#include <stddef.h>

#include "fnv1a.h"

// #define BIG_ENDIAN


#define HASH32_PRIME  (uint32_t)(16777619u)     /* Special prime number */

/**
 * @brief Process one byte from pointer (pointer is incremented)
 *
 * @param hash_ hash accumulator
 * @param pb_   pointer to byte pointer (will be incremented)
 */
#define HASH32_PSTEP(hash_, pb_) do { \
  (hash_) ^= *(const uint8_t *)(pb_++); \
  (hash_) *= HASH32_PRIME; \
} while(0)

/**
 * @brief Same as the above but second arg is the value, not pointer.
 * @note No pointer autoincrement
 */
#define HASH32_CSTEP(hash_, b_) do { \
  (hash_) ^= (uint8_t)(b_); \
  (hash_) *= HASH32_PRIME; \
} while( 0 )


// !! WARNING This macro contains a `return` statement !!
//
#define HASH32_RETURN_IF_DONE do { if (--len == 0) return hash; } while(0)




/**
 * @brief Compute FNV-1a hash over a byte buffer
 *
 * This function supports incremental hashing:
 *
 *     h = hash32(HASH32_IV, data1, len1);
 *     h = hash32(h,        data2, len2);
 *     h = hash32(h,        data3, len3);
 *     ....
 *
 * @param prev_hash previous hash value (use HASH32_IV for fresh hash)
 * @param data      Input byte buffer
 * @param len       Length of input buffer
 *
 * @return Updated 32-bit FNV-1a hash
 *
 */
uint32_t hash32(uint32_t prev_hash, const uint8_t *data, size_t len) {

  const uint32_t *wdata;     /* Pointer to an aligned 32-bit accessible portion of the data */
  size_t          nwords;    /* Number of 32-bit chunks */
  uint32_t        hash;      /* Resulting hash */
  uintptr_t       addr; 

  /* Empty input does not change our hash value */
  if (len == 0)
    return prev_hash;

  hash = prev_hash;
  addr = (uintptr_t )data; 

  if (addr & 1) {
    HASH32_PSTEP(hash, data);   // advance data pointer
    HASH32_RETURN_IF_DONE ;
  }

  if (addr & 2) {

    HASH32_PSTEP(hash, data); // advance data pointer
    HASH32_RETURN_IF_DONE ;

    HASH32_PSTEP(hash, data); // advance data pointer
    HASH32_RETURN_IF_DONE ;
  }

  /* At this point we still have data to hash and our data address is 4 bytes aligned
   * Unrolled version which accesses memory once, reading 4 bytes. Original version does byte-by-byte accesses
   *
   */
  wdata  = (const uint32_t *)data; /* do we break aliasing rules here? gcc is quiet */
  nwords  = len >> 2;              /* number of 32 bit words to hash */

  len &= 3;                        /* len now indicates trailing bytes count [0..3] */

  while (nwords > 0) {

    uint32_t v = *wdata++;

#ifdef BIG_ENDIAN
        HASH32_CSTEP(hash, v >> 24); // advance data pointer
        HASH32_CSTEP(hash, v >> 16);
        HASH32_CSTEP(hash, v >> 8);
        HASH32_CSTEP(hash, v);
#else
        HASH32_CSTEP(hash, v);      // advance data pointer
        HASH32_CSTEP(hash, v >> 8);
        HASH32_CSTEP(hash, v >> 16);
        HASH32_CSTEP(hash, v >> 24);
#endif

    nwords--;
  }

  /* There may be 1..3 bytes left after fast-path block. Use the same idea 
   * as used in source address alignment procedure, but inverse it
   *
   */
  data = (const uint8_t *)wdata;

  if (len & 2) {
    HASH32_PSTEP(hash, data); // advance data pointer
    HASH32_PSTEP(hash, data);
  }

  if (len & 1) {
    HASH32_PSTEP(hash, data); // advance data pointer
  }

  return hash;
}
