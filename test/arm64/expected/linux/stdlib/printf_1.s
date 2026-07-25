
.text

    .p2align 3

    .global _start
    .global getchar
    .global print
    .global printf
    .global putchar

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    mov w10, #5
    str w10, [sp, #20]
._L2__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #5
    b.gt ._L4__main
    b ._L7__main
._L3__main:
    b ._L1__main
._L4__main:
    adrp x0, ._L_str4__
    add x0, x0, :lo12:._L_str4__
    mov w1, #14
    bl print
    b ._L3__main
._L7__main:
    adrp x0, ._L_str3__
    add x0, x0, :lo12:._L_str3__
    adrp x1, ._L_str5__
    add x1, x1, :lo12:._L_str5__
    mov w2, #5
    adrp x8, ._L_double2__
    ldr d3, [x8, #:lo12:._L_double2__]
    adrp x8, ._L_float1__
    ldr s4, [x8, #:lo12:._L_float1__]
    mov w5, 120
    mov w6, #1
    bl printf
    b ._L3__main
._L1__main:
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #93
    svc #0

.data

    .p2align 2


._L_float1__:
    .float 5.33

    .p2align 3


._L_double2__:
    .double 5.2

._L_str3__:
    .asciz "%s %d %g %f %c %b"

._L_str4__:
    .asciz "greater than 5"

._L_str5__:
    .asciz "hello"
