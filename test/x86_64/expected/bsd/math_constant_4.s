
.intel_syntax noprefix

.text

    .p2align 4

    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 4], 20
    mov dword ptr [rbp - 8], 10
    mov eax, dword ptr [rbp - 4]
    cdq
    idiv dword ptr [rbp - 8]
    mov dword ptr [rbp - 12], eax
    mov eax, dword ptr [rbp - 4]
    add eax, dword ptr [rbp - 8]
    mov dword ptr [rbp - 12], eax
    mov eax, dword ptr [rbp - 4]
    sub eax, dword ptr [rbp - 8]
    mov dword ptr [rbp - 12], eax
    mov eax, dword ptr [rbp - 4]
    imul eax, dword ptr [rbp - 8]
    mov dword ptr [rbp - 12], eax
    mov eax, dword ptr [rbp - 4]
    cdq
    idiv dword ptr [rbp - 8]
    mov eax, edx
    mov dword ptr [rbp - 12], eax
    mov eax, 10
    mov dword ptr [rbp - 12], eax
    mov rax, 33554433
    mov rdi, 0
    syscall

.data

