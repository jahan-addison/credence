
.intel_syntax noprefix

.text

    .p2align 4

    .global _start

_start:
    push rbp
    mov rbp, rsp
    sub rsp, 24
    mov dword ptr [rbp - 20], 5
    lea rcx, [rbp - 20]
    mov qword ptr [rbp - 8], rcx
    mov rcx, qword ptr [rip + strings+8]
    mov qword ptr [rbp - 16], rcx
    mov rdi, qword ptr [rbp - 16]
    mov esi, 3
    call print
    add rsp, 24
    mov rax, 33554433
    mov rdi, 0
    syscall

.data
._L_str1__:
    .asciz "one"

._L_str2__:
    .asciz "three"

._L_str3__:
    .asciz "two"

strings:
    .p2align 3

    .quad ._L_str1__

    .p2align 3

    .quad ._L_str3__

    .p2align 3

    .quad ._L_str2__

