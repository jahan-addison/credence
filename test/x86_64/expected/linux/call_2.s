
.intel_syntax noprefix

.text

    .p2align 4

    .global _start

_start:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    lea rdi, [rip + ._L_str1__]
    call test
    mov qword ptr [rbp - 8], rax
    mov rdi, qword ptr [rbp - 8]
    call test
    mov rdi, rax
    mov esi, 11
    call print
    add rsp, 16
    mov rax, 60
    mov rdi, 0
    syscall


test:
    push rbp
    mov rbp, rsp
    mov rax, rdi
    pop rbp
    ret

.data
._L_str1__:
    .asciz "hello world"

