
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "good afternoon"

._L_str2__:
    .asciz "good morning"

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 32], 0
    mov dword ptr [rbp - 28], 1
    mov dword ptr [rbp - 24], 2
    lea rcx, [rip + ._L_str1__]
    mov qword ptr [rbp - 20], rcx
    lea rcx, [rip + ._L_str2__]
    mov qword ptr [rbp - 12], rcx
    mov dword ptr [rbp - 36], 10
    mov rax, 33554433
    mov rdi, 0
    syscall

