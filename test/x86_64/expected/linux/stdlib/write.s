
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "hello "

._L_str2__:
    .asciz "how cool is this man\n"

._L_str3__:
    .asciz "world\n"

mess:
    .quad ._L_str1__

    .quad ._L_str3__

unit:
    .long 0

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov eax, dword ptr [rip + unit]
    mov dword ptr [rbp - 4], eax
    mov rax, qword ptr [rip + mess]
    mov qword ptr [rbp - 12], rax
    mov rax, 1
    mov edi, 1
    mov rsi, qword ptr [rbp - 12]
    mov edx, 6
    syscall
    mov rax, 1
    mov edi, 1
    mov rsi, qword ptr [rip + mess+8]
    mov edx, 6
    syscall
    mov rax, 1
    mov edi, 1
    lea rsi, [rip + ._L_str2__]
    mov edx, 21
    syscall
    add rsp, 16
    mov rax, 60
    mov rdi, 0
    syscall

