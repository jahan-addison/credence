
.intel_syntax noprefix

.text

    .p2align 4

    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov eax, 4
    add eax, 1
    mov dword ptr [rbp - 4], eax
    mov eax, dword ptr [rbp - 4]
    sub eax, 1
    add eax, 1
    mov dword ptr [rbp - 8], eax
    mov rax, 33554433
    mov rdi, 0
    syscall

.data

