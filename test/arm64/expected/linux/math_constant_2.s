
.text

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w8, #4
    add w8, w8, #1
    mov w9, w8
    mov w8, w9
    sub w8, w8, #1
    add w8, w8, #1
    mov w10, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #93
    svc #0

.data

