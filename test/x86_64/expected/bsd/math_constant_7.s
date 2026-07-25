
.intel_syntax noprefix

.text

    .p2align 4

    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov eax, 10
    not eax
    mov dword ptr [rbp - 4], eax
    mov eax, dword ptr [rbp - 4]
    neg eax
    mov dword ptr [rbp - 8], eax
    mov eax, 100
    neg eax
    mov dword ptr [rbp - 12], eax
    mov eax, dword ptr [rbp - 8]
    mov dword ptr [rbp - 4], eax
    inc dword ptr [rbp - 4]
    mov eax, dword ptr [rbp - 4]
    mov dword ptr [rbp - 8], eax
    mov rax, 33554433
    mov rdi, 0
    syscall

.data

