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
 *  1) CRC64/ECMA182 algorithm implementation (no tables version)
 *  2) Optimized variation of the FNV-1a hash algorithm.
 */

#pragma once

#include <stdint.h>
#include <stddef.h> /* size_t */


#define HASH32_IV     (uint32_t)(2166136261u)   /* Init vector for 32bit hash */
#define HASH64_IV     0ULL                      /* Init vector for 64bit hash */

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
 * @param prev_hash initial hash state (use HASH32_IV for fresh hash)
 * @param data      Input byte buffer
 * @param len       Length of input buffer
 *
 * @return Updated 32-bit FNV-1a hash
 *
 */
uint32_t hash32(uint32_t prev_hash, uint8_t const *data, size_t len);


/**
 * @brief Compute CRC64/ECMA182 hash over a byte buffer
 *
 * This function supports incremental hashing:
 *
 *     h = hash64(HASH64_IV, data1, len1);
 *     h = hash64(h,        data2, len2);
 *     h = hash64(h,        data3, len3);
 *     ....
 *
 * @param prev_hash initial hash state (use HASH32_IV for fresh hash)
 * @param data      Input byte buffer
 * @param len       Length of input buffer
 *
 * @return Updated 64-bit CRC64 sum
 *
 */
uint64_t hash64(uint64_t prev_hash, void const *data, size_t len);

/**
 * @brief Straight 32 and 64 bit sums over a byte buffer
 *
 * These functions supports incremental hashing just as hash32 and hash64
 *
 * @param prev_hash initial hash state (use 0 for fresh hash)
 * @param data      Input byte buffer
 * @param len       Length of input buffer
 *
 * @return Updated 32/64-bit sum
 *
 */
uint64_t sum64(uint64_t prev_sum, void const *buffer0, size_t buf_len);
uint32_t sum32(uint32_t prev_sum, void const *buffer0, size_t buf_len);
