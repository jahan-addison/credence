###############################################################################
## Copyright (c) Jahan Addison
##
## This software is dual-licensed under the Apache License, Version 2.0
## or the GNU General Public License, Version 3.0 or later.
##
## You may use this work, in part or in whole, under the terms of either
## license.
##
## See the LICENSE.Apache-v2 and LICENSE.GPL-v3 files in the project root
## for the full text of these licenses.
###############################################################################

.data
    .align 3
.L_ten_mil:
    .double 1000000.0

.text
    .align 2
    .global _printf
    .global _print
    .global _putchar
    .global _getchar

####################################################################
## @brief printf(3) - ARM64 macOS/Darwin version
## The first argument in x0 is the format string
## Integer arguments are in x1-x7
## Float and double arguments are in d0-d7
##   Format Specifiers:
## "int=%d, float=%f, double=%g, string=%s, bool=%b, char=%c"
####################################################################
.globl _printf
_printf:
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp
    stp     x19, x20, [sp, #-16]!
    stp     x21, x22, [sp, #-16]!
    stp     x23, x24, [sp, #-16]!
    stp     x25, x26, [sp, #-16]!

    sub     sp, sp, #1024

    mov     x19, x0                    ; format string
    mov     x20, #0                    ; gp_arg_count
    mov     x21, #0                    ; float_arg_count
    mov     x22, #0                    ; buffer_pos
    mov     x23, sp                    ; buffer start

    add     x26, x29, #16

.loop:
    ldrb    w0, [x19]
    cbz     w0, .flush
    cmp     w0, #'%'
    b.eq    .handle_specifier
    strb    w0, [x23, x22]
    add     x22, x22, #1
    add     x19, x19, #1
    b       .loop

.handle_specifier:
    add     x19, x19, #1
    ldrb    w0, [x19]
    add     x19, x19, #1
    cmp     w0, #'d'
    b.eq    .get_gp
    cmp     w0, #'s'
    b.eq    .get_gp
    cmp     w0, #'c'
    b.eq    .get_gp
    cmp     w0, #'b'
    b.eq    .get_gp
    cmp     w0, #'f'
    b.eq    .get_xmm
    cmp     w0, #'g'
    b.eq    .get_xmm
    b       .loop

.get_gp:
    ldr     x24, [x26, x20]
    add     x20, x20, #8
    ldrb    w1, [x19, #-1]
    cmp     w1, #'d'
    b.eq    .do_int
    cmp     w1, #'s'
    b.eq    .do_str
    cmp     w1, #'c'
    b.eq    .do_char
    b       .do_bool

.get_xmm:
    ldr     x24, [x26, x20]
    add     x20, x20, #8
    fmov    d0, x24

.do_float:
    adrp    x0, .L_ten_mil@PAGE
    ldr     d2, [x0, .L_ten_mil@PAGEOFF]

    fcvtzs  x24, d0
    scvtf   d1, x24
    fsub    d0, d0, d1
    fabs    d0, d0
    fmul    d0, d0, d2
    fcvtzs  x25, d0

    mov     x0, x24
    bl      ._itoa

    mov     w0, #'.'
    strb    w0, [x23, x22]
    add     x22, x22, #1

    mov     x0, x25
    mov     x1, #10
    mov     x2, #6
.frac_extract_loop:
    udiv    x3, x0, x1
    msub    x0, x3, x1, x0
    add     w0, w0, #'0'
    sub     x2, x2, #1
    strb    w0, [x23, x22]
    add     x22, x22, #1
    mov     x0, x3
    cmp     x2, #0
    b.ne    .frac_extract_loop

    add     x21, x21, #1
    b       .loop

.do_int:
    mov     x0, x24
    bl      ._itoa
    b       .loop

.do_str:
    mov     x1, x24
.s_copy:
    ldrb    w0, [x1], #1
    cbz     w0, .loop
    strb    w0, [x23, x22]
    add     x22, x22, #1
    b       .s_copy

.do_char:
    and     w0, w24, #0xFF
    strb    w0, [x23, x22]
    add     x22, x22, #1
    b       .loop

.do_bool:
    cmp     x24, #0
    cset    w0, ne
    add     w0, w0, #'0'
    strb    w0, [x23, x22]
    add     x22, x22, #1
    b       .loop

.flush:
    mov     x0, #1
    mov     x1, x23
    mov     x2, x22
    mov     x16, #4                    ; write syscall (Darwin: 4, not 64)
    svc     #0x80

    add     sp, sp, #1024
    ldp     x25, x26, [sp], #16
    ldp     x23, x24, [sp], #16
    ldp     x21, x22, [sp], #16
    ldp     x19, x20, [sp], #16
    ldp     x29, x30, [sp], #16
    ret

._itoa:
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp
    stp     x19, x20, [sp, #-16]!
    stp     x21, x22, [sp, #-16]!

    cmp     x0, #0
    b.ge    .pos_itoa
    neg     x0, x0
    mov     w1, #'-'
    strb    w1, [x23, x22]
    add     x22, x22, #1
.pos_itoa:
    mov     x19, #10
    mov     x20, #0
    sub     sp, sp, #32
    cmp     x0, #0
    b.ne    .div_loop_itoa
    mov     w1, #'0'
    strb    w1, [x23, x22]
    add     x22, x22, #1
    b       .done_itoa
.div_loop_itoa:
    udiv    x21, x0, x19
    msub    x0, x21, x19, x0
    add     w0, w0, #'0'
    strb    w0, [sp, x20]
    add     x20, x20, #1
    mov     x0, x21
    cmp     x0, #0
    b.ne    .div_loop_itoa
.rev_loop_itoa:
    cmp     x20, #0
    b.le    .done_itoa
    sub     x20, x20, #1
    ldrb    w0, [sp, x20]
    strb    w0, [x23, x22]
    add     x22, x22, #1
    b       .rev_loop_itoa
.done_itoa:
    add     sp, sp, #32
    ldp     x21, x22, [sp], #16
    ldp     x19, x20, [sp], #16
    ldp     x29, x30, [sp], #16
    ret

####################################################
## @brief print(1) - ARM64 macOS/Darwin version
## The first argument should hold the buffer address
## The second argument should hold the buffer length
####################################################
.globl _print
_print:
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp
    mov     x2, x1
    mov     x1, x0
    mov     x0, #1
    mov     x16, #4                    ; write syscall (Darwin)
    svc     #0x80
    ldp     x29, x30, [sp], #16
    ret

####################################################
## @brief putchar(1) - ARM64 macOS/Darwin version
####################################################
.globl _putchar
_putchar:
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp
    sub     sp, sp, #16
    strb    w0, [sp]
    mov     x0, #1
    mov     x1, sp
    mov     x2, #1
    mov     x16, #4                    ; write syscall (Darwin)
    svc     #0x80
    add     sp, sp, #16
    ldp     x29, x30, [sp], #16
    ret

####################################################
## @brief getchar - ARM64 macOS/Darwin version
####################################################
.globl _getchar
_getchar:
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp
    sub     sp, sp, #16
    mov     x0, #0
    mov     x1, sp
    mov     x2, #1
    mov     x16, #3                    ; read syscall (Darwin)
    svc     #0x80
    cmp     x0, #1
    b.lt    .eof_getchar
    ldrb    w0, [sp]
    and     w0, w0, #0xFF
    b       .done_getchar
.eof_getchar:
    mov     x0, #-1
.done_getchar:
    add     sp, sp, #16
    ldp     x29, x30, [sp], #16
    ret