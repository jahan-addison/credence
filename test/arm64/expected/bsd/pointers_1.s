
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w10, #5
    str w10, [sp, #8]
    add x6, sp, #8
    mov x9, x6
    mov w11, #10
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

