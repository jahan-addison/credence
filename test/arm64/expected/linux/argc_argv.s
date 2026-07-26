
.text

    .p2align 3

    .global _start
    .global getchar
    .global print
    .global printf
    .global putchar

_start:
    stp x29, x30, [sp, #-32]!
    str x19, [sp, #16]
    mov x29, sp
    add x19, sp, #32
    adrp x0, ._L_str1__
    add x0, x0, :lo12:._L_str1__
    ldr w1, [x19]
    bl printf
    adrp x0, ._L_str2__
    add x0, x0, :lo12:._L_str2__
    ldr x1, [x19, #8]
    bl printf
    adrp x0, ._L_str3__
    add x0, x0, :lo12:._L_str3__
    ldr x1, [x19, #16]
    bl printf
    adrp x0, ._L_str4__
    add x0, x0, :lo12:._L_str4__
    ldr x1, [x19, #24]
    bl printf
    ldr x19, [sp, #16]
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
