
.intel_syntax noprefix

.text

    .p2align 4

    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 12], 100
    lea rcx, [rbp - 12]
    mov qword ptr [rbp - 8], rcx
    mov rax, qword ptr [rbp - 8]
    mov dword ptr [rax], 10
    mov rax, 33554433
    mov rdi, 0
    syscall

.data

