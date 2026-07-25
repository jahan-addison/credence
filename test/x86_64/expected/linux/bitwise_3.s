
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
    mov dword ptr [rbp - 8], 5
    mov eax, dword ptr [rbp - 4]
    xor eax, dword ptr [rbp - 8]
    mov edi, dword ptr [rbp - 8]
    shr edi, dword ptr [rbp - 4]
    or eax, edi
    mov dword ptr [rbp - 12], eax
    mov eax, dword ptr [rbp - 4]
    not eax
    mov edi, dword ptr [rbp - 8]
    not edi
    and eax, edi
    mov dword ptr [rbp - 12], eax
    mov rax, 60
    mov rdi, 0
    syscall

.data

