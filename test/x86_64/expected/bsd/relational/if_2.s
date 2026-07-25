
.intel_syntax noprefix

.text

    .p2align 4

    .global _start

_start:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    lea rcx, [rip + ._L_str1__]
    mov qword ptr [rbp - 16], rcx
    mov dword ptr [rbp - 4], 5
._L2__main:
    mov eax, dword ptr [rbp - 4]
    cmp eax, 5
    jg ._L4__main
    jmp ._L6__main
._L3__main:
    mov rdi, qword ptr [rbp - 16]
    mov esi, 6
    call print
    jmp ._L1__main
._L4__main:
    lea rcx, [rip + ._L_str2__]
    mov qword ptr [rbp - 16], rcx
    jmp ._L3__main
._L6__main:
    lea rcx, [rip + ._L_str3__]
    mov qword ptr [rbp - 16], rcx
    jmp ._L3__main
._L1__main:
    add rsp, 16
    mov rax, 33554433
    mov rdi, 0
    syscall

.data
._L_str1__:
    .asciz "no"

._L_str2__:
    .asciz "yes"

._L_str3__:
    .asciz "yes!!!"

