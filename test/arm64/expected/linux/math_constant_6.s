
.text

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w8, #10
    mvn w8, w8
    mov w9, w8
    mov w8, w9
    mvn w8, w8
    mov w11, w8
    add w9, w9, #1
    sub w11, w11, #1
    add w11, w11, #1
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #93
    svc #0

.data

