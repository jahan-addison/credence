
.text

    .align 3

    .global _start

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
    adrp x0, ._L_str2__
    add x0, x0, :lo12:._L_str2__
    mov w1, #14
    bl print
    b ._L3__main
._L7__main:
    adrp x0, ._L_str1__
    add x0, x0, :lo12:._L_str1__
    adrp x1, ._L_str3__
    add x1, x1, :lo12:._L_str3__
    mov w2, #5
    adrp x8, ._L_double4__
    ldr d3, :lo12:._L_double4__
    mov w4, 120
    mov w5, #1
    bl printf
    b ._L3__main
._L1__main:
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #93
    svc #0

.data

._L_str1__:
    .asciz "%s %d %g %c %b"

._L_str2__:
    .asciz "greater than 5"

._L_str3__:
    .asciz "hello"

._L_double4__:
    .double 5.2
