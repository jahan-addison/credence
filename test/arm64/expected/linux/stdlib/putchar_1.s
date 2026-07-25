
.text

    .p2align 3

    .global _start
    .global getchar
    .global print
    .global printf
    .global putchar

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w0, 108
    bl putchar
    mov w0, 111
    bl putchar
    mov w0, 108
    bl putchar
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #93
    svc #0

.data

