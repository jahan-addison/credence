
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start
    .global _getchar
    .global _print
    .global _printf
    .global _putchar

_start:
    stp x29, x30, [sp, #-32]!
    str x19, [sp, #16]
    mov x29, sp
    add x19, sp, #32
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    ldr w1, [x19]
    bl _printf
    adrp x0, ._L_str2__@PAGE
    add x0, x0, ._L_str2__@PAGEOFF
    ldr x1, [x19, #8]
    bl _printf
    adrp x0, ._L_str3__@PAGE
    add x0, x0, ._L_str3__@PAGEOFF
    ldr x1, [x19, #16]
    bl _printf
    adrp x0, ._L_str4__@PAGE
    add x0, x0, ._L_str4__@PAGEOFF
    ldr x1, [x19, #24]
    bl _printf
    ldr x19, [sp, #16]
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__const

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "argc count: %d\n"

._L_str2__:
    .asciz "argv 1: %s\n"

._L_str3__:
    .asciz "argv 2: %s\n"

._L_str4__:
    .asciz "argv 3: %s\n"
