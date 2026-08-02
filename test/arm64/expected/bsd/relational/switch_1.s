
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    mov w10, #10
    str w10, [sp, #20]
._L2__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #5
    b.ge ._L4__main
._L3__main:
    ldr w10, [sp, #24]
    mov w10, #10
    str w10, [sp, #24]
    b ._L1__main
._L4__main:
    mov w8, w10
    cmp w8, #10
    b.eq ._L8__main
    mov w8, w10
    cmp w8, #6
    b.eq ._L16__main
    mov w8, w10
    cmp w8, #7
    b.eq ._L18__main
._L17__main:
._L15__main:
._L7__main:
    b ._L3__main
._L8__main:
._L9__main:
._L11__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #1
    b.gt ._L10__main
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    ldr w1, [sp, #20]
    bl _printf
    b ._L7__main
._L10__main:
    ldr w10, [sp, #20]
    sub w10, w10, #1
    str w10, [sp, #20]
    b ._L9__main
._L16__main:
    ldr w10, [sp, #24]
    mov w10, #2
    str w10, [sp, #24]
    b ._L3__main
._L18__main:
    ldr w10, [sp, #20]
    mov w10, #5
    str w10, [sp, #20]
    b ._L17__main
._L1__main:
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__const

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "should say 1: %d\n"
