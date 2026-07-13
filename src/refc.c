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
 */


// if (addref(&str->ref)) {
//   use(str);
//   unref(&str->ref);
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
bool addrefn(refc_t *ref, refc_type_t n) {

  refc_type_t r;
  refc_type_t lim;

  if ( ref == NULL )
    return false;

  r = atomic_load_explicit(ref, memory_order_relaxed);
  lim = ((refc_type_t )(-1)) - n;

  do {

    /* refcounter is dead or will be overflow by the addition of `n`? return `false` */
    if (r < 1 || r > lim)
      return false;

    /* CAS, acquire */
  } while (!atomic_compare_exchange_weak_explicit( 
              ref,
             &r,
              r + n,
              memory_order_acquire,
              memory_order_relaxed));

  return true;
}


/**
 * Release references and optionally destroy object. (see refc.h for full comments)
 *
 * Decrease reference counter by /n/. When reference counter reaches
 * zero, destructor function /dtor/ is called: in case of `n` > 0, object is unreferenced one by one
 * so the destructor is called exactly once, even if `n` is larger than current ref counter value
 */
refc_type_t unrefxn(refc_t *r, void *object, refc_type_t n, void (* dtor)(void *)) {

  if (r != NULL && n > 0) {

    int prev;

    do {
      
      if ((prev = atomic_fetch_sub_explicit(r, 1, memory_order_release)) == 1) { 
        atomic_thread_fence(memory_order_acquire);
        /* call eitehr dtor() or free() with eitehr /object/ or /r/ as its argument */
        if (dtor != NULL)
          dtor(object ? object : r);
        break;
      }
    } while(--n);
    /* Successfully decremented, return old value to  the user */
    return prev;
  }
  return 0;
}
