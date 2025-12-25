#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase, TEST_CASE

#include <credence/target/x86_64/generator.h> // for emit
#include <credence/target/x86_64/runtime.h>   // for library
#include <easyjson.h>                         // for JSON
#include <filesystem>                         // for path
#include <fmt/format.h>                       // for format
#include <sstream>                            // for char_traits, basic_ost...
#include <string>                             // for basic_string, allocator

using namespace credence;
using namespace credence::target::x86_64;

namespace fs = std::filesystem;

#define ROOT_PATH STRINGIFY(ROOT_TEST_PATH)

#define EMIT(os, inst) credence::ir::ITA::emit_to(os, inst)
#define LOAD_JSON_FROM_STRING(str) credence::util::AST_Node::load(str)

#define SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST(ast_path, expected)           \
    do {                                                                     \
        using namespace credence::target::x86_64;                            \
        auto test = std::ostringstream{};                                    \
        auto fixture_path = fs::path(ROOT_PATH);                             \
        fixture_path.append("test/fixtures/x86_64/ast");                     \
        auto file_path =                                                     \
            fs::path(fixture_path).append(fmt::format("{}.json", ast_path)); \
        auto fixture_content =                                               \
            easyjson::JSON::load_file(file_path.string()).to_deque();        \
        credence::target::x86_64::emit(                                      \
            test, fixture_content[0], fixture_content[1], true);             \
        REQUIRE(test.str() == expected);                                     \
    } while (0)

#define SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(                    \
    ast_path, expected, syscall)                                               \
    do {                                                                       \
        using namespace credence::target::x86_64;                              \
        auto test = std::ostringstream{};                                      \
        auto fixture_path = fs::path(ROOT_PATH);                               \
        fixture_path.append("test/fixtures/x86_64/ast");                       \
        auto file_path =                                                       \
            fs::path(fixture_path).append(fmt::format("{}.json", ast_path));   \
        auto fixture_content =                                                 \
            easyjson::JSON::load_file(file_path.string()).to_deque();          \
        runtime::add_stdlib_functions_to_symbols(fixture_content[0], syscall); \
        credence::target::x86_64::emit(                                        \
            test, fixture_content[0], fixture_content[1], true);               \
        REQUIRE(test.str() == expected);                                       \
    } while (0)

#define SETUP_X86_64_FIXTURE_SHOULD_THROW_FROM_AST(ast_path)                 \
    do {                                                                     \
        using namespace credence::target::x86_64;                            \
        auto test = std::ostringstream{};                                    \
        auto fixture_path = fs::path(ROOT_PATH);                             \
        fixture_path.append("test/fixtures/x86_64/ast");                     \
        auto file_path =                                                     \
            fs::path(fixture_path).append(fmt::format("{}.json", ast_path)); \
        auto fixture_content =                                               \
            easyjson::JSON::load_file(file_path.string()).to_deque();        \
        REQUIRE_THROWS(credence::target::x86_64::emit(                       \
            test, fixture_content[0], fixture_content[1], true));            \
    } while (0)

TEST_CASE("target/x86_64: fixture: math_constant.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 4], 1
    mov dword ptr [rbp - 8], 5
    mov eax, dword ptr [rbp - 8]
    sub eax, 0
    add eax, dword ptr [rbp - 4]
    imul eax, 10
    mov dword ptr [rbp - 4], eax
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("math_constant", expected);
}

TEST_CASE("target/x86_64: fixture: math_constant_2.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov eax, 4
    add eax, 1
    mov dword ptr [rbp - 4], eax
    mov eax, 2
    sub eax, dword ptr [rbp - 4]
    mov dword ptr [rbp - 8], eax
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("math_constant_2", expected);
}

TEST_CASE("target/x86_64: fixture: math_constant_4.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 4], 20
    mov dword ptr [rbp - 8], 10
    mov eax, dword ptr [rbp - 4]
    cdq
    mov edi, dword ptr [rbp - 8]
    idiv edi
    mov dword ptr [rbp - 12], eax
    mov eax, dword ptr [rbp - 4]
    add eax, dword ptr [rbp - 8]
    mov dword ptr [rbp - 12], eax
    mov eax, dword ptr [rbp - 4]
    sub eax, dword ptr [rbp - 8]
    mov dword ptr [rbp - 12], eax
    mov eax, dword ptr [rbp - 4]
    imul eax, dword ptr [rbp - 8]
    mov dword ptr [rbp - 12], eax
    mov eax, dword ptr [rbp - 4]
    cdq
    mov r8d, dword ptr [rbp - 8]
    idiv r8d
    mov dword ptr [rbp - 12], edx
    mov eax, 10
    mov dword ptr [rbp - 12], eax
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("math_constant_4", expected);
}

TEST_CASE("target/x86_64: fixture: math_constant_5.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov eax, 10
    not eax
    mov dword ptr [rbp - 4], eax
    mov dword ptr [rbp - 8], 5
    inc dword ptr [rbp - 4]
    dec dword ptr [rbp - 8]
    inc dword ptr [rbp - 8]
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("math_constant_5", expected);
}

TEST_CASE("target/x86_64: fixture: math_constant_6.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov eax, 10
    not eax
    mov dword ptr [rbp - 4], eax
    mov eax, dword ptr [rbp - 4]
    not eax
    mov dword ptr [rbp - 8], eax
    inc dword ptr [rbp - 4]
    dec dword ptr [rbp - 8]
    inc dword ptr [rbp - 8]
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("math_constant_6", expected);
}

TEST_CASE("target/x86_64: fixture: math_constant_7.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov eax, 10
    not eax
    mov dword ptr [rbp - 4], eax
    mov eax, dword ptr [rbp - 4]
    neg eax
    mov dword ptr [rbp - 8], eax
    mov eax, 100
    neg eax
    mov dword ptr [rbp - 12], eax
    mov eax, dword ptr [rbp - 8]
    mov dword ptr [rbp - 4], eax
    inc dword ptr [rbp - 4]
    mov eax, dword ptr [rbp - 4]
    mov dword ptr [rbp - 8], eax
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("math_constant_7", expected);
}

TEST_CASE("target/x86_64: fixture: relation_constant.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov al, 1
    mov byte ptr [rbp - 1], al
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("relation_constant", expected);
}

TEST_CASE("target/x86_64: fixture: bitwise_constant_1.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov eax, 10
    not eax
    mov dword ptr [rbp - 4], eax
    mov eax, dword ptr [rbp - 4]
    xor eax, 10
    or eax, 1
    mov dword ptr [rbp - 8], eax
    mov eax, dword ptr [rbp - 4]
    not eax
    mov edi, dword ptr [rbp - 8]
    not edi
    and eax, edi
    mov dword ptr [rbp - 12], eax
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("bitwise_constant_1", expected);
}

TEST_CASE("target/x86_64: fixture: bitwise_2.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov eax, 10
    not eax
    mov dword ptr [rbp - 4], eax
    mov dword ptr [rbp - 8], 5
    mov eax, dword ptr [rbp - 4]
    xor eax, dword ptr [rbp - 8]
    mov edi, dword ptr [rbp - 8]
    shr edi, 5
    or eax, edi
    mov dword ptr [rbp - 12], eax
    mov eax, dword ptr [rbp - 4]
    not eax
    mov edi, dword ptr [rbp - 8]
    not edi
    and eax, edi
    mov dword ptr [rbp - 12], eax
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("bitwise_2", expected);
}

TEST_CASE("target/x86_64: fixture: bitwise_3.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov eax, 10
    not eax
    mov dword ptr [rbp - 4], eax
    mov dword ptr [rbp - 8], 5
    mov eax, dword ptr [rbp - 4]
    xor eax, dword ptr [rbp - 8]
    mov edi, dword ptr [rbp - 8]
    shr edi, dword ptr [rbp - 4]
    or eax, edi
    mov dword ptr [rbp - 12], eax
    mov eax, dword ptr [rbp - 4]
    not eax
    mov edi, dword ptr [rbp - 8]
    not edi
    and eax, edi
    mov dword ptr [rbp - 12], eax
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("bitwise_3", expected);
}

TEST_CASE("target/x86_64: fixture: bitwise_4.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov eax, 10
    not eax
    mov dword ptr [rbp - 4], eax
    mov dword ptr [rbp - 8], 5
    mov eax, 30
    or eax, 15
    mov dword ptr [rbp - 12], eax
    mov eax, dword ptr [rbp - 4]
    not eax
    mov edi, dword ptr [rbp - 8]
    not edi
    and eax, edi
    mov dword ptr [rbp - 12], eax
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("bitwise_4", expected);
}

TEST_CASE("target/x86_64: fixture: pointers_1.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 12], 5
    lea rcx, [rbp - 12]
    mov qword ptr [rbp - 8], rcx
    mov dword ptr [rbp - 16], 10
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("pointers_1", expected);
}

TEST_CASE("target/x86_64: fixture: pointers_2.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 4], 10
    mov dword ptr [rbp - 8], 100
    mov dword ptr [rbp - 12], 6
    mov eax, dword ptr [rbp - 4]
    mov dword ptr [rbp - 16], eax
    mov eax, dword ptr [rbp - 8]
    mov dword ptr [rbp - 20], eax
    lea rcx, [rbp - 16]
    mov qword ptr [rbp - 32], rcx
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("pointers_2", expected);
}

TEST_CASE("target/x86_64: fixture: pointers_3.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 12], 100
    lea rcx, [rbp - 12]
    mov qword ptr [rbp - 8], rcx
    mov rax, qword ptr [rbp - 8]
    mov dword ptr [rax], 10
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("pointers_3", expected);
}

TEST_CASE("target/x86_64: fixture: pointers_4.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 12], 100
    lea rcx, [rbp - 12]
    mov qword ptr [rbp - 8], rcx
    mov rcx, qword ptr [rbp - 8]
    mov qword ptr [rbp - 24], rcx
    mov eax, 20
    add eax, 10
    add eax, 10
    mov rax, qword ptr [rbp - 24]
    mov dword ptr [rax], eax
    mov rax, qword ptr [rbp - 24]
    mov edi, dword ptr [rax]
    mov rax, qword ptr [rbp - 8]
    mov dword ptr [rax], edi
    mov dword ptr [rbp - 12], 5
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("pointers_4", expected);
}

TEST_CASE("target/x86_64: fixture: strings")
{
    SETUP_X86_64_FIXTURE_SHOULD_THROW_FROM_AST("string_2");
    std::string expected = R"x86(
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "hello"

._L_str2__:
    .asciz "world"

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    lea rcx, [rip + ._L_str1__]
    mov qword ptr [rbp - 8], rcx
    lea rcx, [rip + ._L_str2__]
    mov qword ptr [rbp - 16], rcx
    lea rcx, [rip + ._L_str1__]
    mov qword ptr [rbp - 24], rcx
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("string_1", expected);
}

TEST_CASE("target/x86_64: fixture: vector_1.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 12], 0
    mov dword ptr [rbp - 8], 1
    mov dword ptr [rbp - 4], 2
    mov dword ptr [rbp - 16], 10
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("vector_1", expected);
}

TEST_CASE("target/x86_64: fixture: vector_2.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 32], 0
    mov dword ptr [rbp - 28], 1
    mov dword ptr [rbp - 24], 2
    mov dword ptr [rbp - 20], 3
    mov dword ptr [rbp - 16], 4
    mov dword ptr [rbp - 36], 10
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("vector_2", expected);
}

TEST_CASE("target/x86_64: fixture: vector_3.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "good afternoon"

._L_str2__:
    .asciz "good morning"

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 32], 0
    mov dword ptr [rbp - 28], 1
    mov dword ptr [rbp - 24], 2
    lea rcx, [rip + ._L_str1__]
    mov qword ptr [rbp - 20], rcx
    lea rcx, [rip + ._L_str2__]
    mov qword ptr [rbp - 12], rcx
    mov dword ptr [rbp - 36], 10
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("vector_3", expected);
}

TEST_CASE("target/x86_64: fixture: globals")
{
    SETUP_X86_64_FIXTURE_SHOULD_THROW_FROM_AST("globals_2");

    std::string expected = R"x86(
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "that sucks"

._L_str2__:
    .asciz "too bad"

._L_str3__:
    .asciz "tough luck"

mess:
    .quad ._L_str2__

    .quad ._L_str3__

    .quad ._L_str1__

unit:
    .long 1

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    mov eax, dword ptr [rip + unit]
    mov dword ptr [rbp - 4], eax
    mov rax, qword ptr [rip + mess+8]
    mov qword ptr [rbp - 12], rax
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("globals_1", expected);
}

TEST_CASE("target/x86_64: fixture: syscall kernel write")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "hello "

._L_str2__:
    .asciz "how cool is this man\n"

._L_str3__:
    .asciz "world\n"

mess:
    .quad ._L_str1__

    .quad ._L_str3__

unit:
    .long 0

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov eax, dword ptr [rip + unit]
    mov dword ptr [rbp - 4], eax
    mov rax, qword ptr [rip + mess]
    mov qword ptr [rbp - 12], rax
    mov rax, 1
    mov edi, 1
    mov rsi, qword ptr [rbp - 12]
    mov edx, 6
    syscall
    mov rax, 1
    mov edi, 1
    mov rsi, qword ptr [rip + mess+8]
    mov edx, 6
    syscall
    mov rax, 1
    mov edi, 1
    lea rsi, [rip + ._L_str2__]
    mov edx, 21
    syscall
    add rsp, 16
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "stdlib/write", expected, true);
}

TEST_CASE("target/x86_64: fixture: stdlib print")
{
    SETUP_X86_64_FIXTURE_SHOULD_THROW_FROM_AST("stdlib/print_2");
    std::string expected = R"x86(
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "hello "

._L_str2__:
    .asciz "hello world\n"

._L_str3__:
    .asciz "how cool is this man\n"

._L_str4__:
    .asciz "world\n"

mess:
    .quad ._L_str1__

    .quad ._L_str4__

unit:
    .long 0

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov eax, dword ptr [rip + unit]
    mov dword ptr [rbp - 4], eax
    mov rax, qword ptr [rip + mess]
    mov qword ptr [rbp - 12], rax
    lea rdi, [rip + ._L_str2__]
    mov esi, 13
    call print
    mov rdi, qword ptr [rbp - 12]
    mov esi, 6
    call print
    mov rdi, qword ptr [rip + mess+8]
    mov esi, 7
    call print
    mov rax, 1
    mov edi, 1
    lea rsi, [rip + ._L_str3__]
    mov edx, 21
    syscall
    add rsp, 16
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "stdlib/print", expected, true);
}

TEST_CASE("target/x86_64: fixture: call_1.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "hello, how are you"

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    lea rcx, [rip + ._L_str1__]
    mov qword ptr [rbp - 8], rcx
    mov rdi, qword ptr [rbp - 8]
    call identity
    mov rdi, rax
    call identity
    mov rdi, rax
    call identity
    mov rdi, rax
    mov esi, 18
    call print
    add rsp, 16
    mov rax, 60
    mov rdi, 0
    syscall


identity:
    push rbp
    mov rbp, rsp
    mov rax, rdi
    pop rbp
    ret

)x86";
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "call_1", expected, false);
}

TEST_CASE("target/x86_64: fixture: call_2.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "hello world"

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    lea rdi, [rip + ._L_str1__]
    call test
    mov qword ptr [rbp - 8], rax
    mov rdi, qword ptr [rbp - 8]
    call test
    mov rdi, rax
    mov esi, 11
    call print
    add rsp, 16
    mov rax, 60
    mov rdi, 0
    syscall


test:
    push rbp
    mov rbp, rsp
    mov rax, rdi
    pop rbp
    ret

)x86";
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "call_2", expected, false);
}

TEST_CASE("target/x86_64: fixture: readme_2.b")
{
    std::string expected = R"x86(
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
    mov rax, 60
    mov rdi, 0
    syscall


identity:
    push rbp
    mov rbp, rsp
    mov rax, rdi
    pop rbp
    ret

)x86";
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "readme_2", expected, false);
}

TEST_CASE("target/x86_64: fixture: address_of_1.b")
{
    SETUP_X86_64_FIXTURE_SHOULD_THROW_FROM_AST("address_of_1");
    std::string expected = R"x86(
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "one"

._L_str2__:
    .asciz "three"

._L_str3__:
    .asciz "two"

strings:
    .quad ._L_str1__

    .quad ._L_str3__

    .quad ._L_str2__

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    sub rsp, 24
    mov dword ptr [rbp - 20], 5
    lea rcx, [rbp - 20]
    mov qword ptr [rbp - 8], rcx
    mov rcx, qword ptr [rip + strings+8]
    mov qword ptr [rbp - 16], rcx
    mov rdi, qword ptr [rbp - 16]
    mov esi, 3
    call print
    add rsp, 24
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "address_of_2", expected, false);
}

TEST_CASE("target/x86_64: fixture: string_3.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "hello world"

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    sub rsp, 24
    mov dword ptr [rbp - 12], 2
    lea rcx, [rip + ._L_str1__]
    mov qword ptr [rbp - 8], rcx
    mov rdi, qword ptr [rbp - 8]
    mov esi, 11
    call print
    add rsp, 24
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "string_3", expected, false);
}

TEST_CASE("target/x86_64: fixture: stdlib putchar")
{
    SETUP_X86_64_FIXTURE_SHOULD_THROW_FROM_AST("stdlib/putchar_2");
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov edi, 108
    call putchar
    mov edi, 111
    call putchar
    mov edi, 108
    call putchar
    add rsp, 16
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "stdlib/putchar_1", expected, false);
}

TEST_CASE("target/x86_64: fixture: relational/if_1.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

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

.text
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
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "relational/if_1", expected, false);
}

TEST_CASE("target/x86_64: fixture: relational/while_1.b")
{
    std::string expected = R"x86(
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

)x86";
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "relational/while_1", expected, false);
}

TEST_CASE("target/x86_64: fixture: relational/switch_1.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "should say 1: %d, %b\n"

.text
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

)x86";
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "relational/switch_1", expected, false);
}

TEST_CASE("target/x86_64: fixture: relational/if_2.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "no"

._L_str2__:
    .asciz "yes"

._L_str3__:
    .asciz "yes!!!"

.text
    .global _start

_start:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    lea rcx, [rip + ._L_str1__]
    mov qword ptr [rbp - 16], rcx
    mov dword ptr [rbp - 4], 5
._L2__main:
    mov eax, dword ptr [rbp - 4]
    cmp eax, 5
    jg ._L4__main
    jmp ._L6__main
._L3__main:
    mov rdi, qword ptr [rbp - 16]
    mov esi, 6
    call print
    jmp ._L1__main
._L4__main:
    lea rcx, [rip + ._L_str2__]
    mov qword ptr [rbp - 16], rcx
    jmp ._L3__main
._L6__main:
    lea rcx, [rip + ._L_str3__]
    mov qword ptr [rbp - 16], rcx
    jmp ._L3__main
._L1__main:
    add rsp, 16
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "relational/if_2", expected, false);
}

TEST_CASE("target/x86_64: fixture: stdlib/printf_1.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "%s %d %g %c %b"

._L_str2__:
    .asciz "greater than 5"

._L_str3__:
    .asciz "hello"

._L_double4__:
    .double 5.2

.text
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
    lea rdi, [rip + ._L_str2__]
    mov esi, 14
    call print
    jmp ._L3__main
._L7__main:
    lea rdi, [rip + ._L_str1__]
    lea rsi, [rip + ._L_str3__]
    mov edx, 5
    movsd xmm0, [rip + ._L_double4__]
    mov ecx, 120
    mov r8d, 1
    call printf
    jmp ._L3__main
._L1__main:
    add rsp, 16
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "stdlib/printf_1", expected, false);
}

TEST_CASE("target/x86_64: fixture: argc_argv.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "argc count: %d\n"

._L_str2__:
    .asciz "argv 1: %s\n"

._L_str3__:
    .asciz "argv 2: %s\n"

._L_str4__:
    .asciz "argv 3: %s\n"

.text
    .global _start

_start:
    lea r15, [rsp]
    push rbp
    mov rbp, rsp
    sub rsp, 16
    lea rdi, [rip + ._L_str1__]
    mov rsi, [r15]
    call printf
    lea rdi, [rip + ._L_str2__]
    mov rsi, [r15 + 8 * 2]
    call printf
    lea rdi, [rip + ._L_str3__]
    mov rsi, [r15 + 8 * 3]
    call printf
    lea rdi, [rip + ._L_str4__]
    mov rsi, [r15 + 8 * 4]
    call printf
    add rsp, 16
    mov rax, 60
    mov rdi, 0
    syscall

)x86";
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "argc_argv", expected, false);
}