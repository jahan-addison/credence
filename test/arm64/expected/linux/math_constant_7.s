
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    neg w9, w9
    mov w10, w8
    mov w8, #-100
    mov w11, w8
    mov w9, w8
    add w9, w9, #1
    mov w8, w9
    mov w10, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #93
    svc #0

.data

