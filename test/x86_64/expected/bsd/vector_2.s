
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 32], 0
    mov dword ptr [rbp - 28], 1
    mov dword ptr [rbp - 24], 2
    mov dword ptr [rbp - 20], 3
    mov dword ptr [rbp - 16], 4
    mov dword ptr [rbp - 36], 10
    mov rax, 33554433
    mov rdi, 0
    syscall

