
.intel_syntax noprefix

.text

    .p2align 4

    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov eax, dword ptr [rip + unit]
    mov dword ptr [rbp - 4], eax
    mov rax, qword ptr [rip + mess+8]
    mov qword ptr [rbp - 12], rax
    mov rax, 33554433
    mov rdi, 0
    syscall

.data
._L_str1__:
    .asciz "that sucks"

._L_str2__:
    .asciz "too bad"

._L_str3__:
    .asciz "tough luck"

    .p2align 3

mess:
    .quad ._L_str2__

    .quad ._L_str3__

    .quad ._L_str1__

    .p2align 2

unit:
    .long 1

