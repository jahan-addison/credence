
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 12], 0
    mov dword ptr [rbp - 8], 1
    mov dword ptr [rbp - 4], 2
    mov dword ptr [rbp - 16], 10
    mov rax, 33554433
    mov rdi, 0
    syscall

