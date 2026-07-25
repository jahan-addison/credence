
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
    jle ._L4__main
._L3__main:
._L8__main:
    mov eax, dword ptr [rbp - 4]
    cmp eax, 10
    je ._L10__main
._L9__main:
._L14__main:
    mov eax, dword ptr [rbp - 4]
    cmp eax, 5
    jge ._L16__main
._L15__main:
._L20__main:
    mov eax, dword ptr [rbp - 4]
    cmp eax, 5
    jne ._L22__main
._L21__main:
._L26__main:
    mov eax, dword ptr [rbp - 4]
    cmp eax, 8
    jg ._L28__main
._L27__main:
._L32__main:
    mov eax, dword ptr [rbp - 4]
    cmp eax, 20
    jl ._L34__main
._L33__main:
    lea rdi, [rip + ._L_str1__]
    mov esi, 5
    call print
    mov al, 1
    mov byte ptr [rbp - 4], al
    jmp ._L1__main
._L4__main:
    mov dword ptr [rbp - 4], 1
    lea rdi, [rip + ._L_str6__]
    mov esi, 5
    call printf
    jmp ._L3__main
._L10__main:
    lea rdi, [rip + ._L_str2__]
    mov esi, 10
    call printf
    jmp ._L9__main
._L16__main:
    lea rdi, [rip + ._L_str4__]
    mov esi, 5
    call printf
    jmp ._L15__main
._L22__main:
    lea rdi, [rip + ._L_str7__]
    mov esi, 5
    call printf
    jmp ._L21__main
._L28__main:
    lea rdi, [rip + ._L_str3__]
    mov esi, 8
    call printf
    jmp ._L27__main
._L34__main:
    lea rdi, [rip + ._L_str5__]
    mov esi, 20
    call printf
    jmp ._L33__main
._L1__main:
    add rsp, 16
    mov rax, 33554433
    mov rdi, 0
    syscall

.data
._L_str1__:
    .asciz "done!"

._L_str2__:
    .asciz "equal to %d\n"

._L_str3__:
    .asciz "greater than %d\n"

._L_str4__:
    .asciz "greater than or equal to %d\n"

._L_str5__:
    .asciz "less than %d\n"

._L_str6__:
    .asciz "less than or equal to %d\n"

._L_str7__:
    .asciz "not equal to %d\n"

