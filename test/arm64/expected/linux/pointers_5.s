
.text

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    mov w10, #100
    mov w12, #50
    str w10, [sp, #24]
    add x6, sp, #24
    mov x9, x6
    str w12, [sp, #16]
    add x6, sp, #16
    mov x11, x6
    mov w8, #10
    str w8, [x11]
    ldr w8, [x11]
    str w8, [x9]
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #93
    svc #0

.data

