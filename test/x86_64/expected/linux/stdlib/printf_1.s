
.intel_syntax noprefix

.text

    .p2align 4

    .global _start

_start:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov dword ptr [rbp - 4], 5
._L2__main:
    mov eax, dword ptr [rbp - 4]
    cmp eax, 5
    jg ._L4__main
    jmp ._L7__main
._L3__main:
    jmp ._L1__main
._L4__main:
    lea rdi, [rip + ._L_str3__]
    mov esi, 14
    call print
    jmp ._L3__main
._L7__main:
    lea rdi, [rip + ._L_str2__]
    lea rsi, [rip + ._L_str4__]
    mov edx, 5
    movsd xmm0, [rip + ._L_double1__]
    mov ecx, 120
    mov r8d, 1
    call printf
    jmp ._L3__main
._L1__main:
    add rsp, 16
    mov rax, 60
    mov rdi, 0
    syscall

.data
    .p2align 3

._L_double1__:
    .double 5.2

._L_str2__:
    .asciz "%s %d %g %c %b"

._L_str3__:
    .asciz "greater than 5"

._L_str4__:
    .asciz "hello"

