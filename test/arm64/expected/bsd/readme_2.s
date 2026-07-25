
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-48]!
    mov x29, sp
    str w0, [sp, #20]
    str x1, [sp, #28]
    ldr x10, [sp, #36]
    adrp x6, ._L_str4__@PAGE
    add x6, x6, ._L_str4__@PAGEOFF
    mov x10, x6
    str x10, [sp, #36]
._L2__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #1
    b.gt ._L4__main
._L3__main:
    b ._L1__main
._L4__main:
    ldr x0, [sp, #36]
    bl identity
    mov x0, x0
    bl identity
    mov x0, x0
    bl identity
    ldr x0, [sp, #36]
    ldr x10, [sp, #28]
    ldr x1, [x10, #8]
    bl _printf
    adrp x6, strings@PAGE
    add x6, x6, strings@PAGEOFF
    ldr x0, [x6]
    mov w1, #14
    bl _print
    b ._L3__main
._L1__main:
    ldp x29, x30, [sp], #48
    mov w0, #0
    mov x16, #1
    svc #0x80


identity:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov x0, x0
    ldp x29, x30, [sp], #16
    ret

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "good afternoon"

._L_str2__:
    .asciz "good evening"

._L_str3__:
    .asciz "good morning"

._L_str4__:
    .asciz "hello, how are you, %s\n"

.section __DATA,__data

.p2align 3

strings:
    .xword ._L_str1__

    .xword ._L_str3__

    .xword ._L_str2__
