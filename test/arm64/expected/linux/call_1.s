
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr x10, [sp, #24]
    adrp x6, ._L_str1__
    add x6, x6, :lo12:._L_str1__
    mov x10, x6
    str x10, [sp, #24]
    ldr x0, [sp, #24]
    bl identity
    mov x0, x0
    bl identity
    mov x0, x0
    bl identity
    ldr x0, [sp, #24]
    mov w1, #18
    bl print
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #93
    svc #0


identity:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov x0, x0
    ldp x29, x30, [sp], #16
    ret

.data

._L_str1__:
    .asciz "hello, how are you"
