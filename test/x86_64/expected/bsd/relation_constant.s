
.intel_syntax noprefix

.text

    .p2align 4

    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov eax, 1
    mov dword ptr [rbp - 4], eax
    mov dword ptr [rbp - 8], 1
    mov dword ptr [rbp - 12], 0
    mov rax, 33554433
    mov rdi, 0
    syscall

.data

