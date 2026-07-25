
.intel_syntax noprefix

.text

    .p2align 4

    .global _start

_start:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    lea rcx, [rip + ._L_str1__]
    mov qword ptr [rbp - 8], rcx
    mov rdi, qword ptr [rbp - 8]
    call identity
    mov rdi, rax
    call identity
    mov rdi, rax
    call identity
    mov rdi, rax
    mov esi, 18
    call print
    add rsp, 16
    mov rax, 60
    mov rdi, 0
    syscall


identity:
    push rbp
    mov rbp, rsp
    mov rax, rdi
    pop rbp
    ret

.data
._L_str1__:
    .asciz "hello, how are you"

