
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
    inc dword ptr [rbp - 4]
    dec dword ptr [rbp - 8]
    inc dword ptr [rbp - 8]
    mov rax, 60
    mov rdi, 0
    syscall

.data

