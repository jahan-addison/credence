
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "no\n"

._L_str2__:
    .asciz "x, y: %d %d\n"

._L_str3__:
    .asciz "yes!\n"

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov dword ptr [rbp - 4], 100
    mov dword ptr [rbp - 8], 4
._L2__main:
    mov eax, dword ptr [rbp - 4]
    cmp eax, 50
    jg ._L4__main
._L3__main:
._L11__main:
    mov eax, dword ptr [rbp - 4]
    cmp eax, 48
    je ._L13__main
    jmp ._L16__main
._L12__main:
    lea rdi, [rip + ._L_str2__]
    mov esi, dword ptr [rbp - 4]
    mov edx, dword ptr [rbp - 8]
    call printf
    jmp ._L1__main
._L4__main:
._L6__main:
._L8__main:
    mov eax, dword ptr [rbp - 4]
    cmp eax, 50
    jge ._L7__main
    jmp ._L3__main
._L7__main:
    dec dword ptr [rbp - 4]
    mov eax, dword ptr [rbp - 4]
    sub eax, 1
    mov dword ptr [rbp - 8], eax
    jmp ._L6__main
._L13__main:
    lea rdi, [rip + ._L_str1__]
    mov esi, 4
    call print
    jmp ._L12__main
._L16__main:
    lea rdi, [rip + ._L_str3__]
    mov esi, 6
    call print
    jmp ._L12__main
._L1__main:
    add rsp, 16
    mov rax, 60
    mov rdi, 0
    syscall

