
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
    mov qword ptr [rbp - 16], rax
    mov rdi, qword ptr [rbp - 16]
    mov esi, 11
    call print
    add rsp, 16
    mov rax, 33554433
    mov rdi, 0
    syscall


test:
    push rbp
    mov rbp, rsp
    mov rax, rdi
    leave
    ret

.data
._L_str1__:
    .asciz "hello world"

