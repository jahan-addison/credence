
.intel_syntax noprefix

.text

    .p2align 4

    .global _start
    .extern getchar
    .extern print
    .extern printf
    .extern putchar

_start:
    lea r15, [rsp]
    push rbp
    mov rbp, rsp
    sub rsp, 16
    lea rdi, [rip + ._L_str1__]
    mov rsi, [r15]
    call printf
    lea rdi, [rip + ._L_str2__]
    mov rsi, [r15 + 8 * 2]
    call printf
    lea rdi, [rip + ._L_str3__]
    mov rsi, [r15 + 8 * 3]
    call printf
    lea rdi, [rip + ._L_str4__]
    mov rsi, [r15 + 8 * 4]
    call printf
    add rsp, 16
    mov rax, 60
    mov rdi, 0
    syscall

.data
._L_str1__:
    .asciz "argc count: %d\n"

._L_str2__:
    .asciz "argv 1: %s\n"

._L_str3__:
    .asciz "argv 2: %s\n"

._L_str4__:
    .asciz "argv 3: %s\n"

