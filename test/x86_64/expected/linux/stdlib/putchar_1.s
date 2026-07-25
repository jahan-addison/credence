
.intel_syntax noprefix

.text

    .p2align 4

    .global _start
    .extern getchar
    .extern print
    .extern printf
    .extern putchar

_start:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov edi, 108
    call putchar
    mov edi, 111
    call putchar
    mov edi, 108
    call putchar
    add rsp, 16
    mov rax, 60
    mov rdi, 0
    syscall

.data

