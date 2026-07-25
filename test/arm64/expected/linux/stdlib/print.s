
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    adrp x6, unit
    add x6, x6, :lo12:unit
    ldr w10, [x6]
    str w10, [sp, #20]
    ldr x10, [sp, #28]
    adrp x6, mess
    add x6, x6, :lo12:mess
    ldr x10, [x6]
    str x10, [sp, #28]
    adrp x0, ._L_str2__
    add x0, x0, :lo12:._L_str2__
    mov w1, #13
    bl print
    ldr x0, [sp, #28]
    mov w1, #6
    bl print
    adrp x6, mess
    add x6, x6, :lo12:mess
    ldr x0, [x6, #8]
    mov w1, #7
    bl print
    mov w0, #1
    adrp x1, ._L_str3__
    add x1, x1, :lo12:._L_str3__
    mov w2, #21
    mov x8, #64
    svc #0
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #93
    svc #0

.data

._L_str1__:
    .asciz "hello "

._L_str2__:
    .asciz "hello world\n"

._L_str3__:
    .asciz "how cool is this man\n"

._L_str4__:
    .asciz "world\n"

.align 3

mess:
    .xword ._L_str1__

    .xword ._L_str4__

.align 2

unit:
    .long 0
