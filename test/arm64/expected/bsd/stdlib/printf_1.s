
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
    mov w10, #5
    str w10, [sp, #20]
._L2__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #5
    b.gt ._L4__main
    b ._L7__main
._L3__main:
    b ._L1__main
._L4__main:
    adrp x0, ._L_str4__@PAGE
    add x0, x0, ._L_str4__@PAGEOFF
    mov w1, #14
    bl _print
    b ._L3__main
._L7__main:
    adrp x0, ._L_str3__@PAGE
    add x0, x0, ._L_str3__@PAGEOFF
    adrp x1, ._L_str5__@PAGE
    add x1, x1, ._L_str5__@PAGEOFF
    mov w2, #5
    adrp x8, ._L_double2__@PAGE
    ldr d3, [x8, ._L_double2__@PAGEOFF]
    adrp x8, ._L_float1__@PAGE
    ldr s4, [x8, ._L_float1__@PAGEOFF]
    mov w5, 120
    mov w6, #1
    bl _printf
    b ._L3__main
._L1__main:
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

    .p2align 2


._L_float1__:
    .float 5.33

    .p2align 3


._L_double2__:
    .double 5.2

._L_str3__:
    .asciz "%s %d %g %f %c %b"

._L_str4__:
    .asciz "greater than 5"

._L_str5__:
    .asciz "hello"
