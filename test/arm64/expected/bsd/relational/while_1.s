
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    mov w10, #100
    str w10, [sp, #20]
    ldr w10, [sp, #24]
    mov w10, #4
    str w10, [sp, #24]
._L2__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #50
    b.gt ._L4__main
._L3__main:
._L11__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #48
    b.eq ._L13__main
    b ._L16__main
._L12__main:
    adrp x0, ._L_str2__@PAGE
    add x0, x0, ._L_str2__@PAGEOFF
    ldr w1, [sp, #20]
    ldr w2, [sp, #24]
    bl _printf
    b ._L1__main
._L4__main:
._L6__main:
._L8__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #50
    b.ge ._L7__main
    b ._L3__main
._L7__main:
    ldr w10, [sp, #20]
    sub w10, w10, #1
    str w10, [sp, #20]
    ldr w10, [sp, #20]
    mov w8, w10
    sub w8, w8, #1
    ldr w10, [sp, #24]
    mov w10, w8
    str w10, [sp, #24]
    b ._L6__main
._L13__main:
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    mov w1, #4
    bl _print
    b ._L12__main
._L16__main:
    adrp x0, ._L_str3__@PAGE
    add x0, x0, ._L_str3__@PAGEOFF
    mov w1, #6
    bl _print
    b ._L12__main
._L1__main:
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__const

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "no\n"

._L_str2__:
    .asciz "x, y: %d %d\n"

._L_str3__:
    .asciz "yes!\n"
