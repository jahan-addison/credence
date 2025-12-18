//
//  Copyright (c) Jahan Addison
//
//  Licensed under the Apache License, Version 2.0 (the "License")//
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
.intel_syntax noprefix

.text

    .global print
    .global putchar


////////////////////////////////////////////////////
// @brief print
// Arguments are handled by credence
// %rdi should hold the buffer address
// mov    rdi, qword ptr [rbp - 8],
// %rsx should hold the buffer length
// mov    rsi, qword ptr [rbp - 12]
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

////////////////////////////////////////////////////
// @brief putchar
// Arguments are handled by credence
// %rdi should hold the character immediate
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

