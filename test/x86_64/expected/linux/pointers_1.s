
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 12], 5
    lea rcx, [rbp - 12]
    mov qword ptr [rbp - 8], rcx
    mov dword ptr [rbp - 16], 10
    mov rax, 60
    mov rdi, 0
    syscall

