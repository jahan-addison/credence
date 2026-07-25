
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-80]!
    mov x29, sp
    add x15, sp, #72
    mov w8, #0
    str w8, [x15]
    add x15, sp, #64
    mov w8, #1
    str w8, [x15]
    add x15, sp, #56
    mov w8, #2
    str w8, [x15]
    add x15, sp, #48
    adrp x6, ._L_str1__
    add x6, x6, :lo12:._L_str1__
    str x6, [x15]
    mov x15, x6
    add x15, sp, #40
    adrp x6, ._L_str2__
    add x6, x6, :lo12:._L_str2__
    str x6, [x15]
    mov x15, x6
    ldr w10, [sp, #76]
    mov w10, #10
    str w10, [sp, #76]
    add x15, sp, #48
    ldr x0, [sp, #48]
    mov w1, #14
    bl print
    ldp x29, x30, [sp], #80
    mov w0, #0
    mov x8, #93
    svc #0

.data

._L_str1__:
    .asciz "good afternoon"

._L_str2__:
    .asciz "good morning"
