
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w9, #1
    mov w10, #5
    mov w8, w9
    mov w7, #10
    mul w8, w8, w7
    add w8, w8, w10
    sub w8, w8, #0
    mov w9, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

