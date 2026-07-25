
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-80]!
    mov x29, sp
    add x15, sp, #72
    mov w8, #0
    str w8, [x15]
    add x15, sp, #64
    mov w8, #1
    str w8, [x15]
    add x15, sp, #56
    mov w8, #2
    str w8, [x15]
    add x15, sp, #48
    adrp x6, ._L_str1__@PAGE
    add x6, x6, ._L_str1__@PAGEOFF
    str x6, [x15]
    mov x15, x6
    add x15, sp, #40
    adrp x6, ._L_str2__@PAGE
    add x6, x6, ._L_str2__@PAGEOFF
    str x6, [x15]
    mov x15, x6
    ldr w10, [sp, #76]
    mov w10, #10
    str w10, [sp, #76]
    add x15, sp, #48
    ldr x0, [sp, #48]
    mov w1, #14
    bl _print
    ldp x29, x30, [sp], #80
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "good afternoon"

._L_str2__:
    .asciz "good morning"
