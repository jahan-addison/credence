
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start
    .global _getchar
    .global _print
    .global _printf
    .global _putchar

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w0, 108
    bl _putchar
    mov w0, 111
    bl _putchar
    mov w0, 108
    bl _putchar
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

