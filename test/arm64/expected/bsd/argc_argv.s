
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    str w0, [sp, #20]
    str x1, [sp, #28]
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    ldr w1, [sp, #20]
    bl _printf
    adrp x0, ._L_str2__@PAGE
    add x0, x0, ._L_str2__@PAGEOFF
    ldr x10, [sp, #28]
    ldr x1, [x10, #8]
    bl _printf
    adrp x0, ._L_str3__@PAGE
    add x0, x0, ._L_str3__@PAGEOFF
    ldr x10, [sp, #28]
    ldr x1, [x10, #16]
    bl _printf
    adrp x0, ._L_str4__@PAGE
    add x0, x0, ._L_str4__@PAGEOFF
    ldr x10, [sp, #28]
    ldr x1, [x10, #24]
    bl _printf
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "argc count: %d\n"

._L_str2__:
    .asciz "argv 1: %s\n"

._L_str3__:
    .asciz "argv 2: %s\n"

._L_str4__:
    .asciz "argv 3: %s\n"
