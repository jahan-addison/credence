
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov al, 1
    mov byte ptr [rbp - 1], al
    mov dword ptr [rbp - 5], 1
    mov dword ptr [rbp - 9], 0
    mov rax, 33554433
    mov rdi, 0
    syscall

