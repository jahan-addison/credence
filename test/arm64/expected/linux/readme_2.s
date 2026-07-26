
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
    ldr x10, [sp, #32]
    adrp x6, ._L_str4__
    add x6, x6, :lo12:._L_str4__
    mov x10, x6
    str x10, [sp, #32]
._L2__main:
    mov x8, x19
    cmp x8, #1
    b.gt ._L4__main
._L3__main:
    b ._L1__main
._L4__main:
    ldr x0, [sp, #32]
    bl identity
    mov x0, x0
    bl identity
    mov x0, x0
    bl identity
    ldr x0, [sp, #32]
    ldr x1, [x19, #8]
    bl printf
    adrp x6, strings
    add x6, x6, :lo12:strings
    ldr x0, [x6]
    mov w1, #14
    bl print
    b ._L3__main
._L1__main:
    ldr x19, [sp, #16]
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
    .asciz "good afternoon"

._L_str2__:
    .asciz "good evening"

._L_str3__:
    .asciz "good morning"

._L_str4__:
    .asciz "hello, how are you, %s\n"

    .p2align 3


strings:
    .xword ._L_str1__

    .xword ._L_str3__

    .xword ._L_str2__
