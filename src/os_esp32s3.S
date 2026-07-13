#include "config.h"  

#if CONFIG_TARFS_HAVE_OPTIMIZED_MEMCPY

    .align 4
    .text
    .global tarfs_os_memcpy
    .type tarfs_os_memcpy, @function
    .section .iram1

tarfs_os_memcpy:
    .align      4
    entry       sp,     32

    # void memcpy(store_ptr, load_ptr, length)
    #                A2        A3        A4

    EE.LD.128.USAR.IP q0, a3, 0
    RUR.SAR_BYTE a13
    movi a11, 16
    sub a11, a11, a13  # head unaligned bytes 1  (A11, load_ptr)

    EE.LD.128.USAR.IP q1, a2, 0
    RUR.SAR_BYTE a9
    movi a12, 16
    sub a12, a12, a9  # head unaligned bytes 2 (A12, store_ptr)

# is store buffer 16-byte aligned?
    beqi a12, 16, 11f 

# unaligned. calculate reminder and copy via l32/s32
    min a12, a12, a4
    srli a13, a12, 2
    slli a14, a13, 2

# A14 is the reminder of the store_ptr
    sub a14, a12, a14 

# ESP32 zero-overhead hardware loop to copy the unaligned remainder
# This loop only takes place if remainder is >=4 bytes, so we can do 32 bit load/stores.

    loopgtz a13, 9f
    l32i a5, a3, 0
    addi a3, a3, 4
    s32i a5, a2, 0
    addi a2, a2, 4
9:

# Slow path: byte-by-byte copy

    loopgtz a14, 10f
    l8ui a5, a3, 0
    addi a3, a3, 1
    s8i a5, a2, 0
    addi a2, a2, 1

10:

# Main machinery

    sub a4, a4, a12
    EE.LD.128.USAR.IP q0, a3, 0
    RUR.SAR_BYTE a13
11:
    beqz a13, 1f
    srli a5, a4, 4  # len // 16
    slli a6, a5, 4
    sub a6, a4, a6  # remainder

    srli a7, a5, 1 # len // 32
    slli a8, a7, 1
    sub a8, a5, a8 # odd_flag

    srli a9, a6, 2  #remainder_4b
    slli a10, a9, 2
    sub a10, a6, a10 #remainder_1b

    loopgtz a7, 12f
    EE.LD.128.USAR.IP q0, a3, 16
    EE.LD.128.USAR.IP q1, a3, 16
    EE.LD.128.USAR.IP q2, a3, 0
    EE.SRC.Q q0, q0, q1
    EE.SRC.Q q1, q1, q2
    EE.VST.128.IP q0, a2, 16
    EE.VST.128.IP q1, a2, 16
12:
    beqz a8, 3f
    EE.LD.128.USAR.IP q0, a3, 16
    EE.LD.128.USAR.IP q1, a3, 0
    EE.SRC.Q q0, q0, q1
    EE.VST.128.IP q0, a2, 16
    bnez a8, 3f

1:
    srli a5, a4, 4  # len // 16
    slli a6, a5, 4
    sub a6, a4, a6  # remainder

    srli a7, a5, 1 # len // 32
    slli a8, a7, 1
    sub a8, a5, a8 # odd_flag

    srli a9, a6, 2  #remainder_4b
    slli a10, a9, 2
    sub a10, a6, a10 #remainder_1b

    loopgtz a7, 2f
    EE.VLD.128.IP q0, a3, 16
    EE.VLD.128.IP q1, a3, 16
    EE.VST.128.IP q0, a2, 16
    EE.VST.128.IP q1, a2, 16
2:
    beqz a8, 3f
    EE.VLD.128.IP q0, a3, 16
    EE.VST.128.IP q0, a2, 16
3:
    loopgtz a9, 4f
    l32i a5, a3, 0
    addi a3, a3, 4
    s32i a5, a2, 0
    addi a2, a2, 4
4:
    loopgtz a10, 5f
    l8ui a5, a3, 0
    addi a3, a3, 1
    s8i a5, a2, 0
    addi a2, a2, 1
5:
    retw
#endif
