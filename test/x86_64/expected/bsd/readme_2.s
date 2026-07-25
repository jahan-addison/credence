
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "good afternoon"

._L_str2__:
    .asciz "good evening"

._L_str3__:
    .asciz "good morning"

._L_str4__:
    .asciz "hello, how are you, %s\n"

strings:
    .quad ._L_str1__

    .quad ._L_str3__

    .quad ._L_str2__

.text
    .global _start

_start:
    lea r15, [rsp]
    push rbp
    mov rbp, rsp
    sub rsp, 16
    lea rcx, [rip + ._L_str4__]
    mov qword ptr [rbp - 8], rcx
._L2__main:
    mov rax, [r15]
    cmp rax, 1
    jg ._L4__main
._L3__main:
    jmp ._L1__main
._L4__main:
    mov rdi, qword ptr [rbp - 8]
    call identity
    mov rdi, rax
    call identity
    mov rdi, rax
    call identity
    mov rdi, rax
    mov rsi, [r15 + 8 * 2]
    call printf
    mov rdi, qword ptr [rip + strings]
    mov esi, 14
    call print
    jmp ._L3__main
._L1__main:
    add rsp, 16
    mov rax, 33554433
    mov rdi, 0
    syscall


identity:
    push rbp
    mov rbp, rsp
    mov rax, rdi
    pop rbp
    ret

