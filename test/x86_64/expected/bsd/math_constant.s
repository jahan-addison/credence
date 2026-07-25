
.intel_syntax noprefix

.text

    .p2align 4

    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 4], 1
    mov dword ptr [rbp - 8], 5
    mov eax, dword ptr [rbp - 4]
    imul eax, 10
    add eax, dword ptr [rbp - 8]
    sub eax, 0
    mov dword ptr [rbp - 4], eax
    mov rax, 33554433
    mov rdi, 0
    syscall

.data

