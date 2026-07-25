
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
    xor eax, 10
    or eax, 1
    mov dword ptr [rbp - 8], eax
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

