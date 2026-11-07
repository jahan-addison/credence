
.text

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #16]
    mov w10, #100
    str w10, [sp, #16]
    add x8, sp, #16
    mov x9, x8
    mov x8, x9
    mov x11, x8
    mov w8, #20
    add w8, w8, #10
    add w8, w8, #10
    str w8, [x11]
    ldr w10, [x11]
    mov w8, w10
    str w8, [x9]
    ldr w10, [sp, #16]
    mov w10, #5
    str w10, [sp, #16]
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #93
    svc #0

.data

