
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
    mov eax, 10
    xor eax, dword ptr [rbp - 4]
    mov dword ptr [rbp - 16], eax
    mov eax, 1
    mov r11d, eax
    mov eax, dword ptr [rbp - 16]
    or eax, r11d
    mov dword ptr [rbp - 8], eax
    mov eax, dword ptr [rbp - 4]
    not eax
    mov dword ptr [rbp - 20], eax
    mov eax, dword ptr [rbp - 8]
    not eax
    mov r11d, eax
    mov eax, dword ptr [rbp - 20]
    and eax, r11d
    mov dword ptr [rbp - 12], eax
    mov rax, 60
    mov rdi, 0
    syscall

.data

