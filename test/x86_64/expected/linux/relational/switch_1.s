
.intel_syntax noprefix

.text

    .p2align 4

    .global _start

_start:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov dword ptr [rbp - 4], 10
._L2__main:
    mov eax, dword ptr [rbp - 4]
    cmp eax, 5
    jge ._L4__main
._L3__main:
    mov dword ptr [rbp - 8], 10
    jmp ._L1__main
._L4__main:
    mov eax, dword ptr [rbp - 4]
    cmp eax, 10
    je ._L8__main
    mov eax, dword ptr [rbp - 4]
    cmp eax, 6
    je ._L16__main
    mov eax, dword ptr [rbp - 4]
    cmp eax, 7
    je ._L18__main
._L17__main:
._L15__main:
._L7__main:
    jmp ._L3__main
._L8__main:
._L9__main:
._L11__main:
    mov eax, dword ptr [rbp - 4]
    cmp eax, 1
    jg ._L10__main
    lea rdi, [rip + ._L_str1__]
    mov esi, dword ptr [rbp - 4]
    call printf
    jmp ._L7__main
._L10__main:
    dec dword ptr [rbp - 4]
    jmp ._L9__main
._L16__main:
    mov dword ptr [rbp - 8], 2
    jmp ._L3__main
._L18__main:
    mov dword ptr [rbp - 4], 5
    jmp ._L17__main
._L1__main:
    add rsp, 16
    mov rax, 60
    mov rdi, 0
    syscall

.data
._L_str1__:
    .asciz "should say 1: %d, %b\n"

