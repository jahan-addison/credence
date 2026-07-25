
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mov w10, #5
    eor w8, w9, w10
    lsr w6, w10, w9
    orr w8, w8, w6
    mov w11, w8
    mvn w8, w9
    mvn w6, w10
    and w8, w8, w6
    mov w11, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

