
.text

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    mov w8, #10
    mvn w8, w8
    mov w9, w8
    mov w11, #5
    mov w8, w9
    eor w8, w8, w11
    str w8, [sp, #16]
    mov w8, w11
    lsr w8, w8, w9
    mov w7, w8
    ldr w10, [sp, #16]
    mov w8, w10
    orr w8, w8, w7
    mov w12, w8
    mov w8, w9
    mvn w8, w8
    str w8, [sp, #20]
    mov w8, w11
    mvn w8, w8
    mov w7, w8
    ldr w10, [sp, #20]
    mov w8, w10
    and w8, w8, w7
    mov w12, w8
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #93
    svc #0

.data

