
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    mov w10, #10
    str w10, [sp, #20]
._L2__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #5
    b.le ._L4__main
._L3__main:
._L8__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #10
    b.eq ._L10__main
._L9__main:
._L14__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #5
    b.ge ._L16__main
._L15__main:
._L20__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #5
    b.ne ._L22__main
._L21__main:
._L26__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #8
    b.gt ._L28__main
._L27__main:
._L32__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #20
    b.lt ._L34__main
._L33__main:
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    mov w1, #5
    bl _print
    mov w8, 1
    ldr w10, [sp, #20]
    mov w10, w8
    str w10, [sp, #20]
    b ._L1__main
._L4__main:
    ldr w10, [sp, #20]
    mov w10, #1
    str w10, [sp, #20]
    adrp x0, ._L_str6__@PAGE
    add x0, x0, ._L_str6__@PAGEOFF
    mov w1, #5
    bl _printf
    b ._L3__main
._L10__main:
    adrp x0, ._L_str2__@PAGE
    add x0, x0, ._L_str2__@PAGEOFF
    mov w1, #10
    bl _printf
    b ._L9__main
._L16__main:
    adrp x0, ._L_str4__@PAGE
    add x0, x0, ._L_str4__@PAGEOFF
    mov w1, #5
    bl _printf
    b ._L15__main
._L22__main:
    adrp x0, ._L_str7__@PAGE
    add x0, x0, ._L_str7__@PAGEOFF
    mov w1, #5
    bl _printf
    b ._L21__main
._L28__main:
    adrp x0, ._L_str3__@PAGE
    add x0, x0, ._L_str3__@PAGEOFF
    mov w1, #8
    bl _printf
    b ._L27__main
._L34__main:
    adrp x0, ._L_str5__@PAGE
    add x0, x0, ._L_str5__@PAGEOFF
    mov w1, #20
    bl _printf
    b ._L33__main
._L1__main:
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__const

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "done!"

._L_str2__:
    .asciz "equal to %d\n"

._L_str3__:
    .asciz "greater than %d\n"

._L_str4__:
    .asciz "greater than or equal to %d\n"

._L_str5__:
    .asciz "less than %d\n"

._L_str6__:
    .asciz "less than or equal to %d\n"

._L_str7__:
    .asciz "not equal to %d\n"
