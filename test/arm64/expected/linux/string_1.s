
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    adrp x6, ._L_str1__
    add x6, x6, :lo12:._L_str1__
    mov x9, x6
    adrp x6, ._L_str2__
    add x6, x6, :lo12:._L_str2__
    mov x10, x6
    adrp x6, ._L_str1__
    add x6, x6, :lo12:._L_str1__
    mov x11, x6
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #93
    svc #0

.data

._L_str1__:
    .asciz "hello"

._L_str2__:
    .asciz "world"
