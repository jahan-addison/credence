
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 4], 10
    mov dword ptr [rbp - 8], 100
    mov dword ptr [rbp - 12], 6
    mov eax, dword ptr [rbp - 4]
    mov dword ptr [rbp - 16], eax
    mov eax, dword ptr [rbp - 8]
    mov dword ptr [rbp - 20], eax
    lea rcx, [rbp - 16]
    mov qword ptr [rbp - 32], rcx
    mov rax, 60
    mov rdi, 0
    syscall

