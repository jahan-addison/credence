
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w8, 1
    mov w9, w8
    mov w10, #1
    mov w11, #0
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__const

