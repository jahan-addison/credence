
.intel_syntax noprefix

.text

    .p2align 4

    .global _start

_start:
    push rbp
    mov rbp, rsp
    sub rsp, 24
    mov dword ptr [rbp - 12], 2
    lea rcx, [rip + ._L_str1__]
    mov qword ptr [rbp - 8], rcx
    mov rdi, qword ptr [rbp - 8]
    mov esi, 11
    call print
    add rsp, 24
    mov rax, 33554433
    mov rdi, 0
    syscall

.data
._L_str1__:
    .asciz "hello world"

