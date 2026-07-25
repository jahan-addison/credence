
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
    mov edi, dword ptr [rbp - 8]
    idiv edi
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
    mov r8d, dword ptr [rbp - 8]
    idiv r8d
    mov dword ptr [rbp - 12], edx
    mov eax, 10
    mov dword ptr [rbp - 12], eax
    mov rax, 33554433
    mov rdi, 0
    syscall

.data

