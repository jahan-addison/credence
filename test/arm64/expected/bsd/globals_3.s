
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

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
    ldr x10, [x6, #8]
    str x10, [sp, #28]
    ldr x0, [sp, #28]
    mov w1, #10
    bl _print
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__const

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "that sucks"

._L_str2__:
    .asciz "too bad"

._L_str3__:
    .asciz "tough luck"

.section	__DATA,__data

    .p2align 3


mess:
    .xword ._L_str2__

    .xword ._L_str3__

    .xword ._L_str1__

    .p2align 2


unit:
    .long 1
