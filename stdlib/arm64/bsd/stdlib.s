###############################################################################
## Copyright (c) Jahan Addison
##
## This software is dual-licensed under the Apache License, Version 2.0
## or the GNU General Public License, Version 3.0 or later.
###############################################################################

.data
    .p2align 3
.L_ten_mil:
    .double 1000000.0

.text
    .p2align 2

.globl _printf
_printf:
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp

    sub     sp, sp, #128
    stp     x1, x2, [x29, #-128]
    stp     x3, x4, [x29, #-112]
    stp     x5, x6, [x29, #-96]
    str     x7,     [x29, #-80]
    stp     d0, d1, [x29, #-72]
    stp     d2, d3, [x29, #-56]
    stp     d4, d5, [x29, #-40]
    stp     d6, d7, [x29, #-24]

    stp     x19, x20, [sp, #-16]!
    stp     x21, x22, [sp, #-16]!
    stp     x23, x24, [sp, #-16]!
    stp     x25, x26, [sp, #-16]!

    sub     sp, sp, #1024
    mov     x19, x0
    mov     x20, #0
    mov     x21, #0
    mov     x22, #0
    mov     x23, sp
    sub     x26, x29, #128
    sub     x25, x29, #72

.loop:
    ldrb    w0, [x19], #1
    cbz     w0, .flush
    cmp     w0, #'%'
    b.eq    .handle_specifier
    strb    w0, [x23, x22]
    add     x22, x22, #1
    b       .loop

.handle_specifier:
    ldrb    w0, [x19], #1
    cmp     w0, #'d'
    b.eq    .do_int
    cmp     w0, #'s'
    b.eq    .do_str
    cmp     w0, #'c'
    b.eq    .do_char
    cmp     w0, #'b'
    b.eq    .do_bool
    cmp     w0, #'f'
    b.eq    .do_float
    cmp     w0, #'g'
    b.eq    .do_float
    b       .loop

.do_int:
    ldr     x0, [x26, x20]
    add     x20, x20, #8
    bl      ._itoa
    b       .loop

.do_str:
    ldr     x1, [x26, x20]
    add     x20, x20, #8
    cbz     x1, .loop
.s_copy:
    ldrb    w0, [x1], #1
    cbz     w0, .loop
    strb    w0, [x23, x22]
    add     x22, x22, #1
    b       .s_copy

.do_char:
    ldr     x24, [x26, x20]
    add     x20, x20, #8
    strb    w24, [x23, x22]
    add     x22, x22, #1
    b       .loop

.do_bool:
    ldr     x24, [x26, x20]
    add     x20, x20, #8
    cmp     x24, #0
    cset    w0, ne
    add     w0, w0, #'0'
    strb    w0, [x23, x22]
    add     x22, x22, #1
    b       .loop

.do_float:
    ldr     d0, [x25, x21]
    add     x21, x21, #8
    adrp    x0, .L_ten_mil@PAGE
    ldr     d2, [x0, .L_ten_mil@PAGEOFF]

    fcvtzs  x0, d0
    bl      ._itoa

    mov     w0, #'.'
    strb    w0, [x23, x22]
    add     x22, x22, #1

    fcvtzs  x24, d0
    scvtf   d1, x24
    fsub    d0, d0, d1
    fabs    d0, d0
    fmul    d0, d0, d2
    fcvtzs  x0, d0

    ldr x1, =1000000
    mov     x2, #6
.frac_loop:
    mov     x3, #10
    udiv    x1, x1, x3
    udiv    x4, x0, x1
    add     w5, w4, #'0'
    strb    w5, [x23, x22]
    add     x22, x22, #1
    msub    x0, x4, x1, x0
    subs    x2, x2, #1
    b.ne    .frac_loop
    b       .loop

.flush:
    mov     x0, #1
    mov     x1, x23
    mov     x2, x22
    mov     x16, #4
    svc     #0x80

    add     sp, sp, #1024
    ldp     x25, x26, [sp], #16
    ldp     x23, x24, [sp], #16
    ldp     x21, x22, [sp], #16
    ldp     x19, x20, [sp], #16
    mov     sp, x29
    ldp     x29, x30, [sp], #16
    ret

._itoa:
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp
    stp     x19, x20, [sp, #-16]!

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
    orr     x1, xzr, x0
.div_loop_itoa:
    udiv    x2, x1, x19
    msub    x3, x2, x19, x1
    add     w3, w3, #'0'
    strb    w3, [sp, x20]
    add     x20, x20, #1
    mov     x1, x2
    cbnz    x1, .div_loop_itoa
.rev_loop_itoa:
    subs    x20, x20, #1
    ldrb    w0, [sp, x20]
    strb    w0, [x23, x22]
    add     x22, x22, #1
    cbnz    x20, .rev_loop_itoa

    add     sp, sp, #32
    ldp     x19, x20, [sp], #16
    ldp     x29, x30, [sp], #16
    ret

.globl _print
_print:
    mov     x2, x1
    mov     x1, x0
    mov     x0, #1
    mov     x16, #4
    svc     #0x80
    ret

.globl _putchar
_putchar:
    strb    w0, [sp, #-16]!
    mov     x0, #1
    mov     x1, sp
    mov     x2, #1
    mov     x16, #4
    svc     #0x80
    add     sp, sp, #16
    ret

.globl _getchar
_getchar:
    sub     sp, sp, #16
    mov     x0, #0
    mov     x1, sp
    mov     x2, #1
    mov     x16, #3
    svc     #0x80
    cmp     x0, #1
    b.lt    .eof
    ldrb    w0, [sp]
    add     sp, sp, #16
    ret
.eof:
    mov     x0, #-1
    add     sp, sp, #16
    ret