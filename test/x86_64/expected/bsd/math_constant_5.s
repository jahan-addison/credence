
.intel_syntax noprefix

.data

.text
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
    mov rax, 33554433
    mov rdi, 0
    syscall

