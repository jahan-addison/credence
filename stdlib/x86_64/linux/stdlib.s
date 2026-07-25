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
    .p2align 3
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
    push    rbx
    sub     rsp, 1232

    mov     [rbp - 48], rsi
    mov     [rbp - 56], rdx
    mov     [rbp - 64], rcx
    mov     [rbp - 72], r8
    mov     [rbp - 80], r9

    # Save Float Args
    movups  [rbp - 112], xmm0
    movups  [rbp - 128], xmm1
    movups  [rbp - 144], xmm2
    movups  [rbp - 160], xmm3
    movups  [rbp - 176], xmm4
    movups  [rbp - 192], xmm5
    movups  [rbp - 208], xmm6
    movups  [rbp - 224], xmm7

    mov     r12, rdi         # r12 = Format string pointer
    xor     r13, r13         # r13 = Buffer index
    mov     r14, -48         # r14 = Integer offset (starts at -48, steps by -8)
    mov     r15, -112        # r15 = Float offset (starts at -112, steps by -16)
    lea     rbx, [rbp - 1232]# rbx = Output buffer base pointer

.loop:
    mov     al, [r12]
    inc     r12
    test    al, al
    jz      .flush
    cmp     al, '%'
    je      .handle_specifier
    mov     [rbx + r13], al
    inc     r13
    jmp     .loop

.handle_specifier:
    mov     al, [r12]
    inc     r12
    cmp     al, 'd'
    je      .do_int
    cmp     al, 's'
    je      .do_str
    cmp     al, 'c'
    je      .do_char
    cmp     al, 'b'
    je      .do_bool
    cmp     al, 'f'
    je      .do_float32
    cmp     al, 'g'
    je      .do_float64
    jmp     .loop

.do_int:
    mov     rdi, [rbp + r14]
    sub     r14, 8
    call    itoa
    jmp     .loop

.do_str:
    mov     rsi, [rbp + r14]
    sub     r14, 8
    test    rsi, rsi
    jz      .loop
.s_copy:
    mov     al, [rsi]
    inc     rsi
    test    al, al
    jz      .loop
    mov     [rbx + r13], al
    inc     r13
    jmp     .s_copy

.do_char:
    mov     rax, [rbp + r14]
    sub     r14, 8
    mov     [rbx + r13], al
    inc     r13
    jmp     .loop

.do_bool:
    mov     rax, [rbp + r14]
    sub     r14, 8
    test    rax, rax
    setne   al
    add     al, '0'
    mov     [rbx + r13], al
    inc     r13
    jmp     .loop

.do_float32:
    movss   xmm0, dword ptr [rbp + r15]
    sub     r15, 16
    cvtss2sd xmm0, xmm0
    jmp     .L_float_common

.do_float64:
    movsd   xmm0, qword ptr [rbp + r15]
    sub     r15, 16
    jmp     .L_float_common

.L_float_common:
    movq    rax, xmm0
    test    rax, rax
    jns     .L_float_pos
    mov     byte ptr [rbx + r13], '-'
    inc     r13
    btr     rax, 63             # Clear sign bit (fabs)
    movq    xmm0, rax

.L_float_pos:
    cvttsd2si rdi, xmm0
    call    itoa
    mov     byte ptr [rbx + r13], '.'
    inc     r13

    cvttsd2si rax, xmm0
    cvtsi2sd xmm1, rax
    subsd   xmm0, xmm1
    movsd   xmm2, qword ptr [rip + .L_ten_mil]
    mulsd   xmm0, xmm2

    cvttsd2si r10, xmm0         # r10 = integer fraction payload
    mov     r11, 1000000        # r11 = divisor
    mov     rcx, 6              # loop counter

.frac_loop:
    mov     rax, r11
    xor     rdx, rdx
    mov     r8, 10
    div     r8
    mov     r11, rax            # r11 = r11 / 10

    mov     rax, r10
    xor     rdx, rdx
    div     r11                 # rax = digit, rdx = remainder
    add     al, '0'
    mov     [rbx + r13], al
    inc     r13

    mov     r10, rdx            # update payload to remainder
    dec     rcx
    jnz     .frac_loop
    jmp     .loop

.flush:
    mov     rax, 1              # Syscall 1 (sys_write)
    mov     rdi, 1              # stdout
    mov     rsi, rbx            # buffer pointer
    mov     rdx, r13            # buffer length
    syscall

    add     rsp, 1232
    pop     rbx
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    mov     rsp, rbp
    pop     rbp
    ret

itoa:
    test    rdi, rdi
    jns     .pos_itoa
    neg     rdi
    mov     byte ptr [rbx + r13], '-'
    inc     r13
.pos_itoa:
    mov     rax, rdi
    mov     r8, 10
    sub     rsp, 32
    mov     rcx, 0
.div_loop_itoa:
    xor     rdx, rdx
    div     r8                  # rax = rax / 10, rdx = rax % 10
    add     dl, '0'
    mov     byte ptr [rsp + rcx], dl
    inc     rcx
    test    rax, rax
    jnz     .div_loop_itoa
.rev_loop_itoa:
    dec     rcx
    mov     dl, byte ptr [rsp + rcx]
    mov     [rbx + r13], dl
    inc     r13
    test    rcx, rcx
    jnz     .rev_loop_itoa
    add     rsp, 32
    ret

####################################################
## @brief print(1)
## Buffer size is handled by credence
## The first argument should hold the buffer address
## The second argument should hold the buffer length
####################################################
print:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 16
    mov     rdx, rsi
    mov     rsi, rdi
    mov     rax, 1
    mov     rdi, 1
    syscall
    add     rsp, 16
    pop     rbp
    ret

####################################################
## @brief putchar(1)
####################################################
putchar:
    push    rbp
    mov     rbp, rsp
    push    rdi
    mov     rax, 1
    mov     rdi, 1
    mov     rsi, rsp
    mov     rdx, 1
    syscall
    add     rsp, 8
    pop     rbp
    ret

####################################################
## @brief getchar
####################################################
getchar:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 16
    mov     rax, 0
    mov     rdi, 0
    lea     rsi, [rbp - 1]
    mov     rdx, 1
    syscall
    cmp     rax, 1
    jl      .error_or_eof
    movzx   rax, byte ptr [rbp - 1]
    jmp     .done
.error_or_eof:
    mov     rax, -1
.done:
    leave
    ret
