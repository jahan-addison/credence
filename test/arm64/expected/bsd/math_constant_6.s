
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mvn w9, w9
    mov w10, w8
    add w9, w9, #1
    sub w10, w10, #1
    add w10, w10, #1
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

