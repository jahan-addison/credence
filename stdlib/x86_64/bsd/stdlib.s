###############################################################################
## Copyright (c) Jahan Addison
##
## This software is dual-licensed under the Apache License, Version 2.0
## or the GNU General Public License, Version 3.0 or later.
##
## You may use this work, in part or in whole, under the terms of either
## license.
##
## See the LICENSE.Apache-v2 and LICENSE.GPL-v3 files in the project root
## for the full text of these licenses.
###############################################################################

.intel_syntax noprefix

.data
    .align 16
.L_mask:
    .quad 0x7FFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF
.L_ten_mil:
    .double 1000000.0

.text

    .global printf
    .global print
    .global putchar
    .global getchar

####################################################################
## @brief printf(9)
## The first argument in %rdi is the format string
## Float and double arguments are in xmm0-xmm7
##   Format Specifiers:
## "int=%d, float=%f, double=%g, string=%s, bool=%b, char=%c"
####################################################################
printf:
    push    rbp
    mov     rbp, rsp
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 1208

    mov     [rbp - 48], rsi
    mov     [rbp - 56], rdx
    mov     [rbp - 64], rcx
    mov     [rbp - 72], r8
    mov     [rbp - 80], r9

    movups  [rbp - 112], xmm0
    movups  [rbp - 128], xmm1
    movups  [rbp - 144], xmm2
    movups  [rbp - 160], xmm3
    movups  [rbp - 176], xmm4
    movups  [rbp - 192], xmm5
    movups  [rbp - 208], xmm6
    movups  [rbp - 224], xmm7

    mov     r12, rdi
    xor     r13, r13
    xor     r14, r14
    xor     r15, r15

.loop:
    mov     al, [r12]
    test    al, al
    jz      .flush
    cmp     al, '%'
    je      .handle_specifier
    lea     rbx, [rbp - 1208]
    mov     [rbx + r13], al
    inc     r13
    inc     r12
    jmp     .loop

.handle_specifier:
    inc     r12
    mov     al, [r12]
    inc     r12
    cmp     al, 'd'
    je      .get_gp
    cmp     al, 's'
    je      .get_gp
    cmp     al, 'c'
    je      .get_gp
    cmp     al, 'b'
    je      .get_gp
    cmp     al, 'f'
    je      .get_xmm
    cmp     al, 'g'
    je      .get_xmm
    jmp     .loop

.get_gp:
    cmp     r14, 5
    jl      .gp_from_reg
    mov     rax, r14
    sub     rax, 5
    mov     rax, [rbp + 16 + rax * 8]
    jmp     .dispatch_gp
.gp_from_reg:
    mov     rax, r14
    neg     rax
    mov     rax, [rbp - 48 + rax * 8]
.dispatch_gp:
    inc     r14
    mov     bl, [r12 - 1]
    cmp     bl, 'd'
    je      .do_int
    cmp     bl, 's'
    je      .do_str
    cmp     bl, 'c'
    je      .do_char
    jmp     .do_bool

.get_xmm:
    cmp     r15, 8
    jl      .xmm_from_reg
    mov     rax, r15
    sub     rax, 8
    movq    xmm0, [rbp + 16 + rax * 8]
    jmp     .do_float
.xmm_from_reg:
    mov     rax, r15
    shl     rax, 4
    neg     rax
    movups  xmm0, [rbp - 112 + rax]

.do_float:
    sub     rsp, 16
    movsd   [rsp], xmm0

    cvttsd2si rbx, xmm0
    cvtsi2sd  xmm1, rbx
    subsd     xmm0, xmm1
    andpd     xmm0, [rip + .L_mask]
    mulsd     xmm0, [rip + .L_ten_mil]
    cvttsd2si r10, xmm0

    mov     rax, rbx
    call    .itoa

    lea     rbx, [rbp - 1208]
    mov     byte ptr [rbx + r13], '.'
    inc     r13

    mov     rax, r10
    mov     rdi, 6
    mov     rbx, 10
    sub     rsp, 16
    mov     rcx, 6
.frac_extract_loop:
    xor     rdx, rdx
    div     rbx
    add     dl, '0'
    mov     [rsp + rcx - 1], dl
    loop    .frac_extract_loop

    mov     rcx, 6
    xor     rdi, rdi
.frac_copy_loop:
    mov     al, [rsp + rdi]
    lea     rdx, [rbp - 1208]
    mov     [rdx + r13], al
    inc     r13
    inc     rdi
    loop    .frac_copy_loop

    add     rsp, 16
    add     rsp, 16
    inc     r15
    jmp     .loop

.do_int:
    call    .itoa
    jmp     .loop

.do_str:
    mov     rsi, rax
.s_copy:
    mov     al, [rsi]
    test    al, al
    jz      .loop
    lea     rbx, [rbp - 1208]
    mov     [rbx + r13], al
    inc     r13
    inc     rsi
    jmp     .s_copy

.do_char:
    lea     rbx, [rbp - 1208]
    mov     [rbx + r13], al
    inc     r13
    jmp     .loop

.do_bool:
    test    rax, rax
    setnz   al
    add     al, '0'
    lea     rbx, [rbp - 1208]
    mov     [rbx + r13], al
    inc     r13
    jmp     .loop

.flush:
    mov     rax, 0x2000004
    mov     rdi, 1
    lea     rsi, [rbp - 1208]
    mov     rdx, r13
    syscall
    add     rsp, 1208
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    ret

.itoa:
    push    rbx
    push    rcx
    push    rdx
    push    rdi
    test    rax, rax
    jns     .pos
    neg     rax
    lea     rdi, [rbp - 1208]
    mov     byte ptr [rdi + r13], '-'
    inc     r13
.pos:
    mov     rbx, 10
    mov     rdi, rsp
    sub     rsp, 32
    xor     rcx, rcx
    test    rax, rax
    jnz     .div_loop
    lea     rdx, [rbp - 1208]
    mov     byte ptr [rdx + r13], '0'
    inc     r13
    jmp     .done_itoa
.div_loop:
    xor     rdx, rdx
    div     rbx
    add     dl, '0'
    mov     [rsp + rcx], dl
    inc     rcx
    test    rax, rax
    jnz     .div_loop
.rev_loop:
    dec     rcx
    mov     al, [rsp + rcx]
    lea     rdx, [rbp - 1208]
    mov     [rdx + r13], al
    inc     r13
    test    rcx, rcx
    jnz     .rev_loop
.done_itoa:
    add     rsp, 32
    pop     rdi
    pop     rdx
    pop     rcx
    pop     rbx
    ret

####################################################
## @brief print(1)
## Buffer size is handled by credence
## %rdi should hold the buffer address
## mov    rdi, qword ptr [rbp - 8],
## %rsx should hold the buffer length
## mov    rsi, qword ptr [rbp - 12]
####################################################
print:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 16
    mov     rax, 33554436
    mov     rdx, rsi
    mov     rsi, rdi
    mov     rdi, 1
    syscall
    add     rsp, 16
    pop     rbp
    ret

####################################################
## @brief putchar(1)
## %rdi should hold the character immediate
####################################################
putchar:
    push    rbp
    mov     rbp, rsp
    push    rdi
    mov     rax, 33554436

    mov     rdi, 1
    mov     rsi, rsp
    mov     rdx, 1
    syscall
    add     rsp, 8
    pop     rbp
    ret

####################################################
## @brief getchar
## The character from stdin is into a byte in %rax
####################################################
getchar:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 16
    mov     rax, 33554435
    mov     rdi, 0
    lea     rsi, [rbp - 1]
    mov     rdx, 1
    syscall
    jc      .error
    cmp     rax, 1
    jl      .eof
    movzx   rax, byte ptr [rbp - 1]
    jmp     .done_getchar
.error:
.eof:
    mov     rax, -1
.done_getchar:
    leave
    ret
