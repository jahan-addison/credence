
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    adrp x6, ._L_str1__@PAGE
    add x6, x6, ._L_str1__@PAGEOFF
    mov x9, x6
    adrp x6, ._L_str2__@PAGE
    add x6, x6, ._L_str2__@PAGEOFF
    mov x10, x6
    adrp x6, ._L_str1__@PAGE
    add x6, x6, ._L_str1__@PAGEOFF
    mov x11, x6
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__const

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "hello"

._L_str2__:
    .asciz "world"
