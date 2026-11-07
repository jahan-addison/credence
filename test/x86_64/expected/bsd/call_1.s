
.intel_syntax noprefix

.text

    .p2align 4

    .global _start

_start:
    push rbp
    mov rbp, rsp
    sub rsp, 40
    lea rcx, [rip + ._L_str1__]
    mov qword ptr [rbp - 8], rcx
    mov rdi, qword ptr [rbp - 8]
    call identity
    mov qword ptr [rbp - 16], rax
    mov rdi, qword ptr [rbp - 16]
    call identity
    mov qword ptr [rbp - 24], rax
    mov rdi, qword ptr [rbp - 24]
    call identity
    mov qword ptr [rbp - 32], rax
    mov rdi, qword ptr [rbp - 32]
    mov esi, 18
    call print
    add rsp, 40
    mov rax, 33554433
    mov rdi, 0
    syscall


identity:
    push rbp
    mov rbp, rsp
    mov rax, rdi
    leave
    ret

.data
._L_str1__:
    .asciz "hello, how are you"

