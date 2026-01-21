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

    stp     x19, x20, [sp, #-16]!
    stp     x21, x22, [sp, #-16]!
    stp     x23, x24, [sp, #-16]!
    stp     x25, x26, [sp, #-16]!

    sub     sp, sp, #1152

    add     x9, sp, #1024
    stp     x0, x1, [x9, #0]
    stp     x2, x3, [x9, #16]
    stp     x4, x5, [x9, #32]
    stp     x6, x7, [x9, #48]

    add     x9, sp, #1088
    stp     d0, d1, [x9, #0]
    stp     d2, d3, [x9, #16]
    stp     d4, d5, [x9, #32]
    stp     d6, d7, [x9, #48]

    mov     x19, x0
    mov     x20, #1
    add     x26, sp, #1024
    add     x25, sp, #1088

    mov     x22, #0
    mov     x23, sp

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
    ldr     x0, [x26, x20, lsl #3]
    add     x20, x20, #1
    bl      ._itoa
    b       .loop

.do_str:
    ldr     x1, [x26, x20, lsl #3]
    add     x20, x20, #1
    cbz     x1, .loop
.s_copy:
    ldrb    w0, [x1], #1
    cbz     w0, .loop
    strb    w0, [x23, x22]
    add     x22, x22, #1
    b       .s_copy

.do_char:
    ldr     x0, [x26, x20, lsl #3]
    add     x20, x20, #1
    strb    w0, [x23, x22]
    add     x22, x22, #1
    b       .loop

.do_bool:
    ldr     x0, [x26, x20, lsl #3]
    add     x20, x20, #1
    cmp     x0, #0
    cset    w0, ne
    add     w0, w0, #'0'
    strb    w0, [x23, x22]
    add     x22, x22, #1
    b       .loop

.do_float:
    ldr     d0, [x25, x20, lsl #3]
    add     x20, x20, #1

    fcvtzs  x0, d0
    bl      ._itoa

    mov     w0, #'.'
    strb    w0, [x23, x22]
    add     x22, x22, #1

    frintz  d1, d0
    fsub    d0, d0, d1
    fabs    d0, d0

    adrp    x0, .L_ten_mil@PAGE
    ldr     d2, [x0, .L_ten_mil@PAGEOFF]
    fmul    d0, d0, d2

    fcvtzs  x0, d0

    movz    x1, #0x4240
    movk    x1, #0x000f, lsl #16
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

    add     sp, sp, #1152
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