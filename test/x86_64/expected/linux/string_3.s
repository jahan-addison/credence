
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "hello world"

.text
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
    mov rax, 60
    mov rdi, 0
    syscall

