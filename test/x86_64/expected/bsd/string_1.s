
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "hello"

._L_str2__:
    .asciz "world"

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    lea rcx, [rip + ._L_str1__]
    mov qword ptr [rbp - 8], rcx
    lea rcx, [rip + ._L_str2__]
    mov qword ptr [rbp - 16], rcx
    lea rcx, [rip + ._L_str1__]
    mov qword ptr [rbp - 24], rcx
    mov rax, 33554433
    mov rdi, 0
    syscall

