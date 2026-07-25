
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

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
    adrp x0, ._L_str2__@PAGE
    add x0, x0, ._L_str2__@PAGEOFF
    mov w1, #14
    bl _print
    b ._L3__main
._L7__main:
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    adrp x1, ._L_str3__@PAGE
    add x1, x1, ._L_str3__@PAGEOFF
    mov w2, #5
    adrp x8, ._L_double4__@PAGE
    ldr d3, ._L_double4__@PAGEOFF
    mov w4, 120
    mov w5, #1
    bl _printf
    b ._L3__main
._L1__main:
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "%s %d %g %c %b"

._L_str2__:
    .asciz "greater than 5"

._L_str3__:
    .asciz "hello"

.section __DATA,__data

._L_double4__:
    .double 5.2
