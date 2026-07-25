
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr x10, [sp, #28]
    adrp x6, ._L_str1__
    add x6, x6, :lo12:._L_str1__
    mov x10, x6
    str x10, [sp, #28]
    ldr w10, [sp, #20]
    mov w10, #5
    str w10, [sp, #20]
._L2__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #5
    b.gt ._L4__main
    b ._L6__main
._L3__main:
    ldr x0, [sp, #28]
    mov w1, #6
    bl print
    b ._L1__main
._L4__main:
    ldr x10, [sp, #28]
    adrp x6, ._L_str2__
    add x6, x6, :lo12:._L_str2__
    mov x10, x6
    str x10, [sp, #28]
    b ._L3__main
._L6__main:
    ldr x10, [sp, #28]
    adrp x6, ._L_str3__
    add x6, x6, :lo12:._L_str3__
    mov x10, x6
    str x10, [sp, #28]
    b ._L3__main
._L1__main:
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #93
    svc #0

.data

._L_str1__:
    .asciz "no"

._L_str2__:
    .asciz "yes"

._L_str3__:
    .asciz "yes!!!"
