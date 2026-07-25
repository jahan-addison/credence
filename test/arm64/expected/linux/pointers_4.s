
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w10, #100
    str w10, [sp, #8]
    add x6, sp, #8
    mov x9, x6
    mov x8, x9
    mov x11, x8
    mov w8, #20
    add w8, w8, #10
    add w8, w8, #10
    str w8, [x11]
    ldr w8, [x11]
    str w8, [x9]
    ldr x10, [sp, #16]
    mov x10, #5
    str x10, [sp, #16]
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #93
    svc #0

.data

