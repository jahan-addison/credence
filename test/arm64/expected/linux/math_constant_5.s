
.text

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mov w10, #5
    add w9, w9, #1
    sub w10, w10, #1
    add w10, w10, #1
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #93
    svc #0

.data

