
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
    ldr x10, [x6, #8]
    str x10, [sp, #28]
    ldr x0, [sp, #28]
    mov w1, #10
    bl print
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #93
    svc #0

.data

._L_str1__:
    .asciz "that sucks"

._L_str2__:
    .asciz "too bad"

._L_str3__:
    .asciz "tough luck"

.align 3

mess:
    .xword ._L_str2__

    .xword ._L_str3__

    .xword ._L_str1__

.align 2

unit:
    .long 1
