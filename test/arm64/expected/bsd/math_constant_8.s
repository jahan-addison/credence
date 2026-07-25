
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

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
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    ldr w1, [sp, #20]
    bl _printf
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "m is %d\n"
