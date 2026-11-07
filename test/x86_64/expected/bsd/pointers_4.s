
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
    mov rcx, qword ptr [rbp - 8]
    mov qword ptr [rbp - 24], rcx
    mov eax, 20
    add eax, 10
    add eax, 10
    mov r10, qword ptr [rbp - 24]
    mov [r10], eax
    mov r10, qword ptr [rbp - 24]
    mov r10d, [r10]
    mov eax, r10d
    mov r10, qword ptr [rbp - 8]
    mov [r10], eax
    mov dword ptr [rbp - 12], 5
    mov rax, 33554433
    mov rdi, 0
    syscall

.data

