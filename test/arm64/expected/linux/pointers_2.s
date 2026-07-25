
.text

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w9, #10
    mov w10, #100
    mov w11, #6
    mov w8, w9
    mov w12, w8
    mov w8, w10
    mov w13, w8
    str w12, [sp, #8]
    add x6, sp, #8
    mov x14, x6
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #93
    svc #0

.data

