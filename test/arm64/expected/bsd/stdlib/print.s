
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
    ldr w10, [sp, #20]
    adrp x6, unit@PAGE
    add x6, x6, unit@PAGEOFF
    ldr w10, [x6]
    str w10, [sp, #20]
    ldr x10, [sp, #28]
    adrp x6, mess@PAGE
    add x6, x6, mess@PAGEOFF
    ldr x10, [x6]
    str x10, [sp, #28]
    adrp x0, ._L_str2__@PAGE
    add x0, x0, ._L_str2__@PAGEOFF
    mov w1, #13
    bl _print
    ldr x0, [sp, #28]
    mov w1, #6
    bl _print
    adrp x6, mess@PAGE
    add x6, x6, mess@PAGEOFF
    ldr x0, [x6, #8]
    mov w1, #7
    bl _print
    mov w0, #1
    adrp x1, ._L_str3__@PAGE
    add x1, x1, ._L_str3__@PAGEOFF
    mov w2, #21
    mov x16, #4
    svc #0x80
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "hello "

._L_str2__:
    .asciz "hello world\n"

._L_str3__:
    .asciz "how cool is this man\n"

._L_str4__:
    .asciz "world\n"

.section __DATA,__data

    .p2align 3


mess:
    .xword ._L_str1__

    .xword ._L_str4__

    .p2align 2


unit:
    .long 0
