
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    adrp x0, ._L_str1__
    add x0, x0, :lo12:._L_str1__
    bl test
    mov x0, x0
    ldr x10, [sp, #24]
    mov x10, x0
    str x10, [sp, #24]
    ldr x0, [sp, #24]
    bl test
    ldr x0, [sp, #24]
    mov w1, #11
    bl print
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #93
    svc #0


test:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov x0, x0
    ldp x29, x30, [sp], #16
    ret

.data

._L_str1__:
    .asciz "hello world"
