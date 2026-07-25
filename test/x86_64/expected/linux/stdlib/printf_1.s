
.intel_syntax noprefix

.text

    .p2align 4

    .global _start
    .extern getchar
    .extern print
    .extern printf
    .extern putchar

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
    lea rdi, [rip + ._L_str4__]
    mov esi, 14
    call print
    jmp ._L3__main
._L7__main:
    lea rdi, [rip + ._L_str3__]
    lea rsi, [rip + ._L_str5__]
    mov edx, 5
    movsd xmm0, [rip + ._L_double2__]
    movss xmm1, [rip + ._L_float1__]
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
    .p2align 2

._L_float1__:
    .float 5.33

    .p2align 3

._L_double2__:
    .double 5.2

._L_str3__:
    .asciz "%s %d %g %f %c %b"

._L_str4__:
    .asciz "greater than 5"

._L_str5__:
    .asciz "hello"

