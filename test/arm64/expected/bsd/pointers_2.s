
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    mov w9, #10
    mov w11, #100
    mov w12, #6
    ldr w10, [sp, #16]
    mov w8, w9
    mov w10, w8
    str w10, [sp, #16]
    mov w8, w11
    mov w13, w8
    add x8, sp, #16
    mov x14, x8
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__const

