
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
    mov w0, #1
    ldr x1, [sp, #28]
    mov w2, #6
    mov x8, #64
    svc #0
    adrp x6, mess
    add x6, x6, :lo12:mess
    mov w0, #1
    ldr x1, [x6, #8]
    mov w2, #6
    mov x8, #64
    svc #0
    mov w0, #1
    adrp x1, ._L_str2__
    add x1, x1, :lo12:._L_str2__
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
    .asciz "how cool is this man\n"

._L_str3__:
    .asciz "world\n"

.align 3

mess:
    .xword ._L_str1__

    .xword ._L_str3__

.align 2

unit:
    .long 0
