
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-64]!
    mov x29, sp
    add x15, sp, #56
    mov w8, #0
    str w8, [x15]
    add x15, sp, #48
    mov w8, #1
    str w8, [x15]
    add x15, sp, #40
    mov w8, #2
    str w8, [x15]
    add x15, sp, #32
    mov w8, #3
    str w8, [x15]
    add x15, sp, #24
    mov w8, #4
    str w8, [x15]
    mov w9, #10
    ldp x29, x30, [sp], #64
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

