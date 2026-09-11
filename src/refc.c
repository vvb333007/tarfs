/*
 * TARFS - Immutable (read-only) filesystem for embedded systems.
 *
 * Copyright (c) 2026 Viacheslav Logunov
 * SPDX-License-Identifier: MMIT
 *
 * Author:
 *   Viacheslav Logunov <vvb333007@gmail.com>
 *
 * Project:
 *   https://github.com/vvb333007/tarfs
 *
 * @file refc.c
 * @brief Public Atomic Reference Counter API implementation
 */


// if (addref(&str->ref) > 0) {
//   use(str);
//   unref(&str->ref);  // or unrefx(&str->ref, destructor_function);
//
// }
//
//


#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdbool.h>

#include "refc.h"

/**
 * Increase reference counter by specified value. (see refc.h for full comments)
 *
 * This function attempts to increase reference counter by `n` (user defined integer
 * type variable or integer literal, e.g. addrefn(&ref, 10)).
 */
refc_type_t addrefn(refc_t *ref, refc_type_t n) {

  refc_type_t r;
  refc_type_t lim;

  if ( ref == NULL )
    return 0;

  r = atomic_load_explicit(ref, memory_order_relaxed);

  if ( n > 0) {
    lim = ((refc_type_t )(-1)) - n;

    do {

      /* is refcounter dead or will it be overflowed by the addition of /n/? */
      if (r < 1 || r > lim)
        return 0;

      /* CAS, acquire */
    } while (!atomic_compare_exchange_weak_explicit( 
                ref,
               &r,
                r + n,
                memory_order_acquire,
                memory_order_relaxed));
  }
  return r + n;
}


/**
 * Release references and optionally destroy object. (see refc.h for full comments)
 *
 * Decrease reference counter by /n/. When reference counter reaches
 * zero, destructor function /dtor/ is called: in case of /n/ > 0, object is unreferenced one by one
 * so the destructor is called exactly once, even if /n/ is larger than current ref counter value
 */
refc_type_t unrefxn(refc_t *r, void *object, refc_type_t n, void (*dtor)(void *)) {

  refc_type_t cur, next;

  if (r == NULL)
    return 0;

  cur = atomic_load_explicit(r, memory_order_relaxed);

  if ( n > 0) {
    do {
      if (cur == 0)
        return 0;  /* dead already / being destroyed: no destructor only report a failure */

      next = (n >= cur) ? 0 : cur - n;

      /* CAS, release */
    } while (!atomic_compare_exchange_weak_explicit(
                r, &cur, next,
                memory_order_release,
                memory_order_relaxed));

    if (next == 0) {
      atomic_thread_fence(memory_order_acquire);
      if (dtor != NULL)
        dtor(object ? object : r);
    }
  }

  return cur;   /* value BEFORE this operation */
}
