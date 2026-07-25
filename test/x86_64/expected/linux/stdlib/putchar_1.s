
.intel_syntax noprefix

.data

.text
    .global _start

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

