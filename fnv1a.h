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
 *
 *  This implementation is an optimized variation of the FNV-1a hash algorithm.
 *  This is NOT a strict reference implementation of FNV, but preserves
 *  byte-equivalence of FNV-1a while improving memory throughput.
 */
#pragma once

#include <stdint.h>
#include <stddef.h> /* size_t */


#define HASH32_IV     (uint32_t)(2166136261u)   /* Init vector */

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
uint32_t hash32(uint32_t prev_hash, const uint8_t *data, size_t len);
