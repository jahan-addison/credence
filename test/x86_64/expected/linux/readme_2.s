
.intel_syntax noprefix

.text

    .p2align 4

    .global _start
    .extern getchar
    .extern print
    .extern printf
    .extern putchar

_start:
    lea r15, [rsp]
    push rbp
    mov rbp, rsp
    sub rsp, 40
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
    mov qword ptr [rbp - 16], rax
    mov rdi, qword ptr [rbp - 16]
    call identity
    mov qword ptr [rbp - 24], rax
    mov rdi, qword ptr [rbp - 24]
    call identity
    mov qword ptr [rbp - 32], rax
    mov rdi, qword ptr [rbp - 32]
    mov rsi, [r15 + 8 * 2]
    call printf
    mov r11, qword ptr [rip + strings]
    mov qword ptr [rbp - 40], r11
    mov rdi, qword ptr [rbp - 40]
    mov esi, 14
    call print
    jmp ._L3__main
._L1__main:
    add rsp, 16
    mov rax, 60
    mov rdi, 0
    syscall


identity:
    push rbp
    mov rbp, rsp
    mov rax, rdi
    leave
    ret

.data
._L_str1__:
    .asciz "good afternoon"

._L_str2__:
    .asciz "good evening"

._L_str3__:
    .asciz "good morning"

._L_str4__:
    .asciz "hello, how are you, %s\n"

    .p2align 3

strings:
    .quad ._L_str1__

    .quad ._L_str3__

    .quad ._L_str2__

