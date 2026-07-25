
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    str w0, [sp, #20]
    str x1, [sp, #28]
    adrp x0, ._L_str1__
    add x0, x0, :lo12:._L_str1__
    ldr w1, [sp, #20]
    bl printf
    adrp x0, ._L_str2__
    add x0, x0, :lo12:._L_str2__
    ldr x10, [sp, #28]
    ldr x1, [x10, #8]
    bl printf
    adrp x0, ._L_str3__
    add x0, x0, :lo12:._L_str3__
    ldr x10, [sp, #28]
    ldr x1, [x10, #16]
    bl printf
    adrp x0, ._L_str4__
    add x0, x0, :lo12:._L_str4__
    ldr x10, [sp, #28]
    ldr x1, [x10, #24]
    bl printf
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #93
    svc #0

.data

._L_str1__:
    .asciz "argc count: %d\n"

._L_str2__:
    .asciz "argv 1: %s\n"

._L_str3__:
    .asciz "argv 2: %s\n"

._L_str4__:
    .asciz "argv 3: %s\n"
