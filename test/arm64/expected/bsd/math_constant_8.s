
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start
    .global _getchar
    .global _print
    .global _printf
    .global _putchar

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #16]
    mov w10, #1
    str w10, [sp, #16]
    ldr w10, [sp, #20]
    mov w10, #5
    str w10, [sp, #20]
    mov w8, #10
    ldr w10, [sp, #16]
    mul w8, w8, w10
    ldr w10, [sp, #20]
    add w8, w8, w10
    sub w8, w8, #0
    ldr w10, [sp, #16]
    mov w10, w8
    str w10, [sp, #16]
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    ldr w1, [sp, #16]
    bl _printf
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__const

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "m is %d\n"
