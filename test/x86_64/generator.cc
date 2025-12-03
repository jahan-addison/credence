#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase, TEST_CASE

#include <credence/target/x86_64/generator.h> // for emit
#include <filesystem>                         // for path
#include <fmt/format.h>                       // for format
#include <simplejson.h>                       // for JSON
#include <sstream>                            // for char_traits, basic_ost...
#include <string>                             // for basic_string, allocator

using namespace credence;
using namespace credence::target::x86_64;

namespace fs = std::filesystem;

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

#define ROOT_PATH STRINGIFY(ROOT_TEST_PATH)

#define EMIT(os, inst) credence::ir::ITA::emit_to(os, inst)
#define LOAD_JSON_FROM_STRING(str) credence::util::AST_Node::load(str)

#define SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST(ast_path, expected)                  \
 do {                                                                               \
    using namespace credence::target::x86_64;                                       \
    auto test = std::ostringstream{};                                               \
    auto fixture_path = fs::path(ROOT_PATH);                                        \
    fixture_path.append("test/fixtures/x86_64/ast");                                \
    auto file_path = fs::path(fixture_path)                                         \
        .append(fmt::format("{}.json", ast_path));                                  \
    auto fixture_content = json::JSON::load_file(file_path.string()).to_deque();    \
    credence::target::x86_64::emit(                                                 \
        test, fixture_content[0], fixture_content[1]);                              \
    REQUIRE(test.str() == expected);                                                \
 } while(0)

#define SETUP_X86_64_FIXTURE_SHOULD_THROW_FROM_AST(ast_path)                       \
 do {                                                                               \
    using namespace credence::target::x86_64;                                       \
    auto test = std::ostringstream{};                                               \
    auto fixture_path = fs::path(ROOT_PATH);                                        \
    fixture_path.append("test/fixtures/x86_64/ast");                                \
    auto file_path = fs::path(fixture_path)                                         \
        .append(fmt::format("{}.json", ast_path));                                  \
    auto fixture_content = json::JSON::load_file(file_path.string()).to_deque();    \
    REQUIRE_THROWS(credence::target::x86_64::emit(                                  \
        test, fixture_content[0], fixture_content[1]));                             \
 } while(0)

TEST_CASE("target/x86_64: fixture: math_constant.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global main

main:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 4], 1
    mov dword ptr [rbp - 8], 5
    mov eax, dword ptr [rbp - 8]
    sub eax, 0
    add eax, dword ptr [rbp - 4]
    imul eax, 10
    mov dword ptr [rbp - 4], eax
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("math_constant", expected);
}

TEST_CASE("target/x86_64: fixture: math_constant_2.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global main

main:
    push rbp
    mov rbp, rsp
    mov eax, 4
    add eax, 1
    mov dword ptr [rbp - 4], eax
    mov eax, 2
    sub eax, dword ptr [rbp - 4]
    mov dword ptr [rbp - 8], eax
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("math_constant_2", expected);
}

TEST_CASE("target/x86_64: fixture: math_constant_4.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global main

main:
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
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("math_constant_4", expected);
}

TEST_CASE("target/x86_64: fixture: math_constant_5.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global main

main:
    push rbp
    mov rbp, rsp
    mov eax, 10
    not eax
    mov dword ptr [rbp - 4], eax
    mov dword ptr [rbp - 8], 5
    inc dword ptr [rbp - 4]
    dec dword ptr [rbp - 8]
    inc dword ptr [rbp - 8]
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("math_constant_5", expected);
}

TEST_CASE("target/x86_64: fixture: math_constant_6.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global main

main:
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
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("math_constant_6", expected);
}

TEST_CASE("target/x86_64: fixture: math_constant_7.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global main

main:
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
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("math_constant_7", expected);
}

TEST_CASE("target/x86_64: fixture: relation_constant.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global main

main:
    push rbp
    mov rbp, rsp
    mov al, 1
    mov byte ptr [rbp - 1], al
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("relation_constant", expected);
}

TEST_CASE("target/x86_64: fixture: bitwise_constant_1.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global main

main:
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
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("bitwise_constant_1", expected);
}

TEST_CASE("target/x86_64: fixture: bitwise_2.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global main

main:
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
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("bitwise_2", expected);
}

TEST_CASE("target/x86_64: fixture: bitwise_3.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global main

main:
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
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("bitwise_3", expected);
}

TEST_CASE("target/x86_64: fixture: bitwise_4.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global main

main:
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
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("bitwise_4", expected);
}

TEST_CASE("target/x86_64: fixture: pointers_1.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global main

main:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 12], 5
    lea rax, [rbp - 12]
    mov qword ptr [rbp - 8], rax
    mov dword ptr [rbp - 16], 10
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("pointers_1", expected);
}

TEST_CASE("target/x86_64: fixture: pointers_2.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global main

main:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 4], 10
    mov dword ptr [rbp - 8], 100
    mov dword ptr [rbp - 12], 6
    mov eax, dword ptr [rbp - 4]
    mov dword ptr [rbp - 16], eax
    mov eax, dword ptr [rbp - 8]
    mov dword ptr [rbp - 20], eax
    lea rax, [rbp - 16]
    mov qword ptr [rbp - 32], rax
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("pointers_2", expected);
}

TEST_CASE("target/x86_64: fixture: pointers_3.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global main

main:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 12], 100
    lea rax, [rbp - 12]
    mov qword ptr [rbp - 8], rax
    mov rax, qword ptr [rbp - 8]
    mov dword ptr [rax], 10
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("pointers_3", expected);
}

TEST_CASE("target/x86_64: fixture: pointers_3.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global main

main:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 12], 100
    lea rax, [rbp - 12]
    mov qword ptr [rbp - 8], rax
    mov rax, qword ptr [rbp - 8]
    mov qword ptr [rbp - 24], rax
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
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("pointers_4", expected);
}

TEST_CASE("target/x86_64: fixture: string_1.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "hello"

._L_str2__:
    .asciz "world"

.text
    .global main

main:
    push rbp
    mov rbp, rsp
    lea rax, [rip + ._L_str1__]
    mov qword ptr [rbp - 8], rax
    lea rax, [rip + ._L_str2__]
    mov qword ptr [rbp - 16], rax
    lea rax, [rip + ._L_str1__]
    mov qword ptr [rbp - 24], rax
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("string_1", expected);
}

TEST_CASE("target/x86_64: fixture: string_2.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

._L_str1__:
    .asciz "hello world"

.text
    .global main

main:
    push rbp
    mov rbp, rsp
    lea rax, [rip + ._L_str1__]
    mov qword ptr [rbp - 16], rax
    lea rax, [rbp - 16]
    mov qword ptr [rbp - 8], rax
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("string_2", expected);
}

TEST_CASE("target/x86_64: fixture: vector_1.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global main

main:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 12], 0
    mov dword ptr [rbp - 8], 1
    mov dword ptr [rbp - 4], 2
    mov dword ptr [rbp - 16], 10
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("vector_1", expected);
}

TEST_CASE("target/x86_64: fixture: vector_2.b")
{
    std::string expected = R"x86(
.intel_syntax noprefix

.data

.text
    .global main

main:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 32], 0
    mov dword ptr [rbp - 28], 1
    mov dword ptr [rbp - 24], 2
    mov dword ptr [rbp - 20], 3
    mov dword ptr [rbp - 16], 4
    mov dword ptr [rbp - 36], 10
_L1:
    xor eax, eax
    pop rbp
    ret

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
    .global main

main:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 32], 0
    mov dword ptr [rbp - 28], 1
    mov dword ptr [rbp - 24], 2
    lea rax, [rip + ._L_str1__]
    mov dword ptr [rbp - 20], rax
    lea rax, [rip + ._L_str2__]
    mov dword ptr [rbp - 12], rax
    mov dword ptr [rbp - 36], 10
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("vector_3", expected);
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
    .global main

main:
    push rbp
    mov rbp, rsp
    mov dword ptr [rbp - 32], 0
    mov dword ptr [rbp - 28], 1
    mov dword ptr [rbp - 24], 2
    lea rax, [rip + ._L_str1__]
    mov dword ptr [rbp - 20], rax
    lea rax, [rip + ._L_str2__]
    mov dword ptr [rbp - 12], rax
    mov dword ptr [rbp - 36], 10
_L1:
    xor eax, eax
    pop rbp
    ret

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
    .global main

main:
    push rbp
    mov rbp, rsp
    mov eax, dword ptr [rip + unit]
    mov dword ptr [rbp - 4], eax
    mov rax, qword ptr [rip + mess+8]
    mov qword ptr [rbp - 12], rax
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    SETUP_X86_64_FIXTURE_AND_TEST_FROM_AST("globals_1", expected);
}