
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w9, #20
    mov w10, #10
    sdiv w8, w8, w10
    mov w11, w8
    add w8, w8, w10
    mov w11, w8
    sub w8, w8, w10
    mov w11, w8
    mul w8, w8, w10
    mov w11, w8
    sdiv w8, w8, w10
    msub w8, w8, w10, w8
    mov w11, w8
    mov w8, #10
    mov w11, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__const

