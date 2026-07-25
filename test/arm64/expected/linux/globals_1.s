
.text

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    adrp x6, unit
    add x6, x6, :lo12:unit
    ldr w9, [x6]
    adrp x6, mess
    add x6, x6, :lo12:mess
    ldr x10, [x6, #8]
    ldp x29, x30, [sp], #16
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

    .p2align 3


mess:
    .xword ._L_str2__

    .xword ._L_str3__

    .xword ._L_str1__

    .p2align 2


unit:
    .long 1
