
.text

    .p2align 3

    .global _start
    .global getchar
    .global print
    .global printf
    .global putchar

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    mov w10, #1
    str w10, [sp, #20]
    ldr w10, [sp, #24]
    mov w10, #5
    str w10, [sp, #24]
    ldr w10, [sp, #20]
    mov w8, w10
    mov w7, #10
    mul w8, w8, w7
    ldr w10, [sp, #24]
    add w8, w8, w10
    sub w8, w8, #0
    ldr w10, [sp, #20]
    mov w10, w8
    str w10, [sp, #20]
    adrp x0, ._L_str1__
    add x0, x0, :lo12:._L_str1__
    ldr w1, [sp, #20]
    bl printf
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #93
    svc #0

.data

._L_str1__:
    .asciz "m is %d\n"
