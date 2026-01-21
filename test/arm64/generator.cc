/**
 * target/arm64: code generator tests
 */
#include <doctest/doctest.h>

#include <credence/target/arm64/generator.h>
#include <credence/target/arm64/runtime.h>
#include <easyjson.h>
#include <filesystem>
#include <fmt/format.h>
#include <sstream>
#include <string>

using namespace credence;
using namespace credence::target::arm64;

namespace fs = std::filesystem;

#define ROOT_PATH STRINGIFY(ROOT_TEST_PATH)

#define EMIT(os, inst) credence::ir::ITA::emit_to(os, inst)
#define LOAD_JSON_FROM_STRING(str) credence::util::AST_Node::load(str)

#define SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST(ast_path, expected)            \
    do {                                                                     \
        using namespace credence::target::arm64;                             \
        auto test = std::ostringstream{};                                    \
        auto fixture_path = fs::path(ROOT_PATH);                             \
        fixture_path.append("test/fixtures/platform/ast");                   \
        auto file_path =                                                     \
            fs::path(fixture_path).append(fmt::format("{}.json", ast_path)); \
        auto fixture_content =                                               \
            easyjson::JSON::load_file(file_path.string()).to_deque();        \
        credence::target::arm64::emit(                                       \
            test, fixture_content[0], fixture_content[1], true);             \
        REQUIRE(test.str() == expected);                                     \
    } while (0)

#define SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(                   \
    ast_path, expected, syscall)                                             \
    do {                                                                     \
        using namespace credence::target::arm64;                             \
        auto test = std::ostringstream{};                                    \
        auto fixture_path = fs::path(ROOT_PATH);                             \
        fixture_path.append("test/fixtures/platform/ast");                   \
        auto file_path =                                                     \
            fs::path(fixture_path).append(fmt::format("{}.json", ast_path)); \
        auto fixture_content =                                               \
            easyjson::JSON::load_file(file_path.string()).to_deque();        \
        credence::target::common::runtime::add_stdlib_functions_to_symbols(  \
            fixture_content[0],                                              \
            credence::target::common::assembly::OS_Type::BSD,                \
            credence::target::common::assembly::Arch_Type::ARM64,            \
            syscall);                                                        \
        credence::target::arm64::emit(                                       \
            test, fixture_content[0], fixture_content[1], true);             \
        REQUIRE(test.str() == expected);                                     \
    } while (0)

#define SETUP_ARM64_FIXTURE_SHOULD_THROW_FROM_AST(ast_path)                  \
    do {                                                                     \
        using namespace credence::target::arm64;                             \
        auto test = std::ostringstream{};                                    \
        auto fixture_path = fs::path(ROOT_PATH);                             \
        fixture_path.append("test/fixtures/platform/ast");                   \
        auto file_path =                                                     \
            fs::path(fixture_path).append(fmt::format("{}.json", ast_path)); \
        auto fixture_content =                                               \
            easyjson::JSON::load_file(file_path.string()).to_deque();        \
        REQUIRE_THROWS(credence::target::arm64::emit(                        \
            test, fixture_content[0], fixture_content[1], true));            \
    } while (0)

std::string replace_last_lines_in_string(std::string_view src,
    std::string_view replacement,
    int amount = 3)
{
    size_t pos = src.length();
    int newlines = 0;
    while (pos > 0) {
        if (src[pos - 1] == '\n') {
            newlines++;
            if (newlines == amount) {
                break;
            }
        }
        pos--;
    }
    std::string result(src.substr(0, pos));
    result += replacement;

    return result;
}

TEST_CASE("target/arm64: fixture: math_constant.b")
{
#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w9, #1
    mov w10, #5
    mov w8, w10
    sub w8, w8, #0
    add w8, w8, w9
    mov w7, #10
    mul w8, w8, w7
    mov w9, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w9, #1
    mov w10, #5
    mov w8, w10
    sub w8, w8, #0
    add w8, w8, w9
    mov w7, #10
    mul w8, w8, w7
    mov w9, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("math_constant", expected);
};

TEST_CASE("target/arm64: fixture: math_constant_8.b")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    mov w10, #1
    str w10, [sp, #20]
    ldr w10, [sp, #24]
    mov w10, #5
    str w10, [sp, #24]
    ldr w10, [sp, #24]
    mov w8, w10
    sub w8, w8, #0
    ldr w10, [sp, #20]
    ldr w10, [sp, #20]
    add w8, w8, w10
    mov w7, #10
    mul w8, w8, w7
    ldr w10, [sp, #20]
    mov w10, w8
    str w10, [sp, #20]
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    ldr w1, [sp, #20]
    bl printf
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #1
    svc #0

.data

._L_str1__:
    .asciz "m is %d\n"
)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    mov w10, #1
    str w10, [sp, #20]
    ldr w10, [sp, #24]
    mov w10, #5
    str w10, [sp, #24]
    ldr w10, [sp, #24]
    mov w8, w10
    sub w8, w8, #0
    ldr w10, [sp, #20]
    ldr w10, [sp, #20]
    add w8, w8, w10
    mov w7, #10
    mul w8, w8, w7
    ldr w10, [sp, #20]
    mov w10, w8
    str w10, [sp, #20]
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    ldr w1, [sp, #20]
    bl _printf
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "m is %d\n"
)arm";
#endif

    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "math_constant_8", expected, true);
}

TEST_CASE("target/arm64: fixture: math_constant_2.b")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w8, #4
    add w8, w8, #1
    mov w9, w8
    mov w8, #2
    sub w8, w8, w9
    mov w10, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w8, #4
    add w8, w8, #1
    mov w9, w8
    mov w8, #2
    sub w8, w8, w9
    mov w10, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("math_constant_2", expected);
}

TEST_CASE("target/arm64: fixture: math_constant_4.b")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w9, #20
    mov w10, #10
    sdiv w8, w8, w10
    mov w11, w8
    add w8, w8, w10
    mov w11, w8
    sub w8, w8, w10
    mov w11, w8
    mul w8, w8, w10
    mov w11, w8
    sdiv w8, w8, w10
    msub w8, w8, w10, w8
    mov w11, w8
    mov w8, #10
    mov w11, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w9, #20
    mov w10, #10
    sdiv w8, w8, w10
    mov w11, w8
    add w8, w8, w10
    mov w11, w8
    sub w8, w8, w10
    mov w11, w8
    mul w8, w8, w10
    mov w11, w8
    sdiv w8, w8, w10
    msub w8, w8, w10, w8
    mov w11, w8
    mov w8, #10
    mov w11, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("math_constant_4", expected);
}

TEST_CASE("target/arm64: fixture: math_constant_5.b")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mov w10, #5
    add w9, w9, #1
    sub w10, w10, #1
    add w10, w10, #1
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mov w10, #5
    add w9, w9, #1
    sub w10, w10, #1
    add w10, w10, #1
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("math_constant_5", expected);
}

TEST_CASE("target/arm64: fixture: math_constant_6.b")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mvn w9, w9
    mov w10, w8
    add w9, w9, #1
    sub w10, w10, #1
    add w10, w10, #1
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mvn w9, w9
    mov w10, w8
    add w9, w9, #1
    sub w10, w10, #1
    add w10, w10, #1
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("math_constant_6", expected);
}

TEST_CASE("target/arm64: fixture: math_constant_7.b")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    neg w9, w9
    mov w10, w8
    mov w8, #-100
    mov w11, w8
    mov w9, w8
    add w9, w9, #1
    mov w8, w9
    mov w10, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    neg w9, w9
    mov w10, w8
    mov w8, #-100
    mov w11, w8
    mov w9, w8
    add w9, w9, #1
    mov w8, w9
    mov w10, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("math_constant_7", expected);
}

TEST_CASE("target/arm64: fixture: relation_constant.b")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w8, 1
    mov w9, w8
    mov w10, #1
    mov w11, #0
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w8, 1
    mov w9, w8
    mov w10, #1
    mov w11, #0
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("relation_constant", expected);
}

TEST_CASE("target/arm64: fixture: bitwise_constant_1.b")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mov w6, #10
    eor w8, w6, w9
    orr w8, w8, #1
    mov w10, w8
    mvn w8, w9
    mvn w6, w10
    and w8, w8, w6
    mov w11, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mov w6, #10
    eor w8, w6, w9
    orr w8, w8, #1
    mov w10, w8
    mvn w8, w9
    mvn w6, w10
    and w8, w8, w6
    mov w11, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("bitwise_constant_1", expected);
}

TEST_CASE("target/arm64: fixture: bitwise_2.b")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mov w10, #5
    eor w8, w9, w10
    lsr w6, w10, #5
    orr w8, w8, w6
    mov w11, w8
    mvn w8, w9
    mvn w6, w10
    and w8, w8, w6
    mov w11, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mov w10, #5
    eor w8, w9, w10
    lsr w6, w10, #5
    orr w8, w8, w6
    mov w11, w8
    mvn w8, w9
    mvn w6, w10
    and w8, w8, w6
    mov w11, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("bitwise_2", expected);
}

TEST_CASE("target/arm64: fixture: bitwise_3.b")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mov w10, #5
    eor w8, w9, w10
    lsr w6, w10, w9
    orr w8, w8, w6
    mov w11, w8
    mvn w8, w9
    mvn w6, w10
    and w8, w8, w6
    mov w11, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mov w10, #5
    eor w8, w9, w10
    lsr w6, w10, w9
    orr w8, w8, w6
    mov w11, w8
    mvn w8, w9
    mvn w6, w10
    and w8, w8, w6
    mov w11, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("bitwise_3", expected);
}

TEST_CASE("target/arm64: fixture: bitwise_4.b")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mov w10, #5
    mov w8, #30
    orr w8, w8, #15
    mov w11, w8
    mvn w8, w9
    mvn w6, w10
    and w8, w8, w6
    mov w11, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mov w10, #5
    mov w8, #30
    orr w8, w8, #15
    mov w11, w8
    mvn w8, w9
    mvn w6, w10
    and w8, w8, w6
    mov w11, w8
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("bitwise_4", expected);
}

TEST_CASE("target/arm64: fixture: pointers_1.b")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w10, #5
    str w10, [sp, #8]
    add x6, sp, #8
    mov x9, x6
    mov w11, #10
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w10, #5
    str w10, [sp, #8]
    add x6, sp, #8
    mov x9, x6
    mov w11, #10
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("pointers_1", expected);
}

TEST_CASE("target/arm64: fixture: pointers_2.b")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w9, #10
    mov w10, #100
    mov w11, #6
    mov w8, w9
    mov w12, w8
    mov w8, w10
    mov w13, w8
    str w12, [sp, #8]
    add x6, sp, #8
    mov x14, x6
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w9, #10
    mov w10, #100
    mov w11, #6
    mov w8, w9
    mov w12, w8
    mov w8, w10
    mov w13, w8
    str w12, [sp, #8]
    add x6, sp, #8
    mov x14, x6
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("pointers_2", expected);
}

TEST_CASE("target/arm64: fixture: pointers_3.b")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w10, #100
    str w10, [sp, #8]
    add x6, sp, #8
    mov x9, x6
    mov w8, #10
    str w8, [x9]
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w10, #100
    str w10, [sp, #8]
    add x6, sp, #8
    mov x9, x6
    mov w8, #10
    str w8, [x9]
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("pointers_3", expected);
}

TEST_CASE("target/arm64: fixture: pointers_4.b")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w10, #100
    str w10, [sp, #8]
    add x6, sp, #8
    mov x9, x6
    mov x8, x9
    mov x11, x8
    mov w8, #20
    add w8, w8, #10
    add w8, w8, #10
    str w8, [x11]
    ldr w8, [x11]
    str w8, [x9]
    ldr x10, [sp, #16]
    mov x10, #5
    str x10, [sp, #16]
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w10, #100
    str w10, [sp, #8]
    add x6, sp, #8
    mov x9, x6
    mov x8, x9
    mov x11, x8
    mov w8, #20
    add w8, w8, #10
    add w8, w8, #10
    str w8, [x11]
    ldr w8, [x11]
    str w8, [x9]
    ldr x10, [sp, #16]
    mov x10, #5
    str x10, [sp, #16]
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("pointers_4", expected);
}

TEST_CASE("target/arm64: fixture: pointers_5.b")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    mov w10, #100
    mov w12, #50
    str w10, [sp, #24]
    add x6, sp, #24
    mov x9, x6
    str w12, [sp, #16]
    add x6, sp, #16
    mov x11, x6
    mov w8, #10
    str w8, [x11]
    ldr w8, [x11]
    str w8, [x9]
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    mov w10, #100
    mov w12, #50
    str w10, [sp, #24]
    add x6, sp, #24
    mov x9, x6
    str w12, [sp, #16]
    add x6, sp, #16
    mov x11, x6
    mov w8, #10
    str w8, [x11]
    ldr w8, [x11]
    str w8, [x9]
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("pointers_5", expected);
}

TEST_CASE("target/arm64: fixture: string_1.b")
{
    SETUP_ARM64_FIXTURE_SHOULD_THROW_FROM_AST("string_2");

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    adrp x6, ._L_str1__@PAGE
    add x6, x6, ._L_str1__@PAGEOFF
    mov x9, x6
    adrp x6, ._L_str2__@PAGE
    add x6, x6, ._L_str2__@PAGEOFF
    mov x10, x6
    adrp x6, ._L_str1__@PAGE
    add x6, x6, ._L_str1__@PAGEOFF
    mov x11, x6
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #1
    svc #0

.data

._L_str1__:
    .asciz "hello"

._L_str2__:
    .asciz "world"
)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    adrp x6, ._L_str1__@PAGE
    add x6, x6, ._L_str1__@PAGEOFF
    mov x9, x6
    adrp x6, ._L_str2__@PAGE
    add x6, x6, ._L_str2__@PAGEOFF
    mov x10, x6
    adrp x6, ._L_str1__@PAGE
    add x6, x6, ._L_str1__@PAGEOFF
    mov x11, x6
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "hello"

._L_str2__:
    .asciz "world"
)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("string_1", expected);
}

TEST_CASE("target/arm64: fixture: vector_1.b")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-48]!
    mov x29, sp
    add x15, sp, #40
    mov w8, #0
    str w8, [x15]
    add x15, sp, #32
    mov w8, #1
    str w8, [x15]
    add x15, sp, #24
    mov w8, #2
    str w8, [x15]
    mov w9, #10
    ldp x29, x30, [sp], #48
    mov w0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-48]!
    mov x29, sp
    add x15, sp, #40
    mov w8, #0
    str w8, [x15]
    add x15, sp, #32
    mov w8, #1
    str w8, [x15]
    add x15, sp, #24
    mov w8, #2
    str w8, [x15]
    mov w9, #10
    ldp x29, x30, [sp], #48
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("vector_1", expected);
}

TEST_CASE("target/arm64: fixture: vector_2.b")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-64]!
    mov x29, sp
    add x15, sp, #56
    mov w8, #0
    str w8, [x15]
    add x15, sp, #48
    mov w8, #1
    str w8, [x15]
    add x15, sp, #40
    mov w8, #2
    str w8, [x15]
    add x15, sp, #32
    mov w8, #3
    str w8, [x15]
    add x15, sp, #24
    mov w8, #4
    str w8, [x15]
    mov w9, #10
    ldp x29, x30, [sp], #64
    mov w0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-64]!
    mov x29, sp
    add x15, sp, #56
    mov w8, #0
    str w8, [x15]
    add x15, sp, #48
    mov w8, #1
    str w8, [x15]
    add x15, sp, #40
    mov w8, #2
    str w8, [x15]
    add x15, sp, #32
    mov w8, #3
    str w8, [x15]
    add x15, sp, #24
    mov w8, #4
    str w8, [x15]
    mov w9, #10
    ldp x29, x30, [sp], #64
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("vector_2", expected);
}

TEST_CASE("target/arm64: fixture: vector_4.b")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-80]!
    mov x29, sp
    add x15, sp, #72
    mov w8, #0
    str w8, [x15]
    add x15, sp, #64
    mov w8, #1
    str w8, [x15]
    add x15, sp, #56
    mov w8, #2
    str w8, [x15]
    add x15, sp, #48
    adrp x6, ._L_str1__@PAGE
    add x6, x6, ._L_str1__@PAGEOFF
    str x6, [x15]
    mov x15, x6
    add x15, sp, #40
    adrp x6, ._L_str2__@PAGE
    add x6, x6, ._L_str2__@PAGEOFF
    str x6, [x15]
    mov x15, x6
    ldr w10, [sp, #76]
    mov w10, #10
    str w10, [sp, #76]
    add x15, sp, #48
    ldr x0, [sp, #48]
    mov w1, #14
    bl print
    ldp x29, x30, [sp], #80
    mov w0, #0
    mov x8, #1
    svc #0

.data

._L_str1__:
    .asciz "good afternoon"

._L_str2__:
    .asciz "good morning"
)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-80]!
    mov x29, sp
    add x15, sp, #72
    mov w8, #0
    str w8, [x15]
    add x15, sp, #64
    mov w8, #1
    str w8, [x15]
    add x15, sp, #56
    mov w8, #2
    str w8, [x15]
    add x15, sp, #48
    adrp x6, ._L_str1__@PAGE
    add x6, x6, ._L_str1__@PAGEOFF
    str x6, [x15]
    mov x15, x6
    add x15, sp, #40
    adrp x6, ._L_str2__@PAGE
    add x6, x6, ._L_str2__@PAGEOFF
    str x6, [x15]
    mov x15, x6
    ldr w10, [sp, #76]
    mov w10, #10
    str w10, [sp, #76]
    add x15, sp, #48
    ldr x0, [sp, #48]
    mov w1, #14
    bl _print
    ldp x29, x30, [sp], #80
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "good afternoon"

._L_str2__:
    .asciz "good morning"
)arm";
#endif
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "vector_4", expected, true);
}

TEST_CASE("target/arm64: fixture: globals 1, 2")
{
    SETUP_ARM64_FIXTURE_SHOULD_THROW_FROM_AST("globals_2");

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    adrp x6, unit@PAGE
    add x6, x6, unit@PAGEOFF
    ldr w9, [x6]
    adrp x6, mess@PAGE
    add x6, x6, mess@PAGEOFF
    ldr x10, [x6, #8]
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #1
    svc #0

.data

._L_str1__:
    .asciz "that sucks"

._L_str2__:
    .asciz "too bad"

._L_str3__:
    .asciz "tough luck"

.align 3

mess:
    .xword ._L_str2__

    .xword ._L_str3__

    .xword ._L_str1__

.align 2

unit:
    .long 1
)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    adrp x6, unit@PAGE
    add x6, x6, unit@PAGEOFF
    ldr w9, [x6]
    adrp x6, mess@PAGE
    add x6, x6, mess@PAGEOFF
    ldr x10, [x6, #8]
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "that sucks"

._L_str2__:
    .asciz "too bad"

._L_str3__:
    .asciz "tough luck"

.section __DATA,__data

.p2align 3

mess:
    .xword ._L_str2__

    .xword ._L_str3__

    .xword ._L_str1__

.p2align 2

unit:
    .long 1
)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("globals_1", expected);
}

TEST_CASE("target/arm64: fixture: globals 3")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    adrp x6, unit@PAGE
    add x6, x6, unit@PAGEOFF
    ldr w10, [x6]
    str w10, [sp, #20]
    ldr x10, [sp, #28]
    adrp x6, mess@PAGE
    add x6, x6, mess@PAGEOFF
    ldr x10, [x6, #8]
    str x10, [sp, #28]
    ldr x0, [sp, #28]
    mov w1, #10
    bl print
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #1
    svc #0

.data

._L_str1__:
    .asciz "that sucks"

._L_str2__:
    .asciz "too bad"

._L_str3__:
    .asciz "tough luck"

.align 3

mess:
    .xword ._L_str2__

    .xword ._L_str3__

    .xword ._L_str1__

.align 2

unit:
    .long 1
)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    adrp x6, unit@PAGE
    add x6, x6, unit@PAGEOFF
    ldr w10, [x6]
    str w10, [sp, #20]
    ldr x10, [sp, #28]
    adrp x6, mess@PAGE
    add x6, x6, mess@PAGEOFF
    ldr x10, [x6, #8]
    str x10, [sp, #28]
    ldr x0, [sp, #28]
    mov w1, #10
    bl _print
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "that sucks"

._L_str2__:
    .asciz "too bad"

._L_str3__:
    .asciz "tough luck"

.section __DATA,__data

.p2align 3

mess:
    .xword ._L_str2__

    .xword ._L_str3__

    .xword ._L_str1__

.p2align 2

unit:
    .long 1
)arm";
#endif
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "globals_3", expected, true);
}

TEST_CASE("target/arm64: fixture: syscall kernel write")
{

#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    adrp x6, unit@PAGE
    add x6, x6, unit@PAGEOFF
    ldr w10, [x6]
    str w10, [sp, #20]
    ldr x10, [sp, #28]
    adrp x6, mess@PAGE
    add x6, x6, mess@PAGEOFF
    ldr x10, [x6]
    str x10, [sp, #28]
    mov w0, #1
    ldr x1, [sp, #28]
    mov w2, #6
    mov x8, #4
    svc #0
    adrp x6, mess@PAGE
    add x6, x6, mess@PAGEOFF
    mov w0, #1
    ldr x1, [x6, #8]
    mov w2, #6
    mov x8, #4
    svc #0
    mov w0, #1
    adrp x1, ._L_str2__@PAGE
    add x1, x1, ._L_str2__@PAGEOFF
    mov w2, #21
    mov x8, #4
    svc #0
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #1
    svc #0

.data

._L_str1__:
    .asciz "hello "

._L_str2__:
    .asciz "how cool is this man\n"

._L_str3__:
    .asciz "world\n"

.align 3

mess:
    .xword ._L_str1__

    .xword ._L_str3__

.align 2

unit:
    .long 0
)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    adrp x6, unit@PAGE
    add x6, x6, unit@PAGEOFF
    ldr w10, [x6]
    str w10, [sp, #20]
    ldr x10, [sp, #28]
    adrp x6, mess@PAGE
    add x6, x6, mess@PAGEOFF
    ldr x10, [x6]
    str x10, [sp, #28]
    mov w0, #1
    ldr x1, [sp, #28]
    mov w2, #6
    mov x16, #4
    svc #0x80
    adrp x6, mess@PAGE
    add x6, x6, mess@PAGEOFF
    mov w0, #1
    ldr x1, [x6, #8]
    mov w2, #6
    mov x16, #4
    svc #0x80
    mov w0, #1
    adrp x1, ._L_str2__@PAGE
    add x1, x1, ._L_str2__@PAGEOFF
    mov w2, #21
    mov x16, #4
    svc #0x80
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "hello "

._L_str2__:
    .asciz "how cool is this man\n"

._L_str3__:
    .asciz "world\n"

.section __DATA,__data

.p2align 3

mess:
    .xword ._L_str1__

    .xword ._L_str3__

.p2align 2

unit:
    .long 0
)arm";
#endif
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "stdlib/write", expected, true);
}

TEST_CASE("target/arm64: fixture: stdlib print")
{
    SETUP_ARM64_FIXTURE_SHOULD_THROW_FROM_AST("stdlib/print_2");
#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    adrp x6, unit@PAGE
    add x6, x6, unit@PAGEOFF
    ldr w10, [x6]
    str w10, [sp, #20]
    ldr x10, [sp, #28]
    adrp x6, mess@PAGE
    add x6, x6, mess@PAGEOFF
    ldr x10, [x6]
    str x10, [sp, #28]
    adrp x0, ._L_str2__@PAGE
    add x0, x0, ._L_str2__@PAGEOFF
    mov w1, #13
    bl print
    ldr x0, [sp, #28]
    mov w1, #6
    bl print
    adrp x6, mess@PAGE
    add x6, x6, mess@PAGEOFF
    ldr x0, [x6, #8]
    mov w1, #7
    bl print
    mov w0, #1
    adrp x1, ._L_str3__@PAGE
    add x1, x1, ._L_str3__@PAGEOFF
    mov w2, #21
    mov x8, #4
    svc #0
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #1
    svc #0

.data

._L_str1__:
    .asciz "hello "

._L_str2__:
    .asciz "hello world\n"

._L_str3__:
    .asciz "how cool is this man\n"

._L_str4__:
    .asciz "world\n"

.align 3

mess:
    .xword ._L_str1__

    .xword ._L_str4__

.align 2

unit:
    .long 0
)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    adrp x6, unit@PAGE
    add x6, x6, unit@PAGEOFF
    ldr w10, [x6]
    str w10, [sp, #20]
    ldr x10, [sp, #28]
    adrp x6, mess@PAGE
    add x6, x6, mess@PAGEOFF
    ldr x10, [x6]
    str x10, [sp, #28]
    adrp x0, ._L_str2__@PAGE
    add x0, x0, ._L_str2__@PAGEOFF
    mov w1, #13
    bl _print
    ldr x0, [sp, #28]
    mov w1, #6
    bl _print
    adrp x6, mess@PAGE
    add x6, x6, mess@PAGEOFF
    ldr x0, [x6, #8]
    mov w1, #7
    bl _print
    mov w0, #1
    adrp x1, ._L_str3__@PAGE
    add x1, x1, ._L_str3__@PAGEOFF
    mov w2, #21
    mov x16, #4
    svc #0x80
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "hello "

._L_str2__:
    .asciz "hello world\n"

._L_str3__:
    .asciz "how cool is this man\n"

._L_str4__:
    .asciz "world\n"

.section __DATA,__data

.p2align 3

mess:
    .xword ._L_str1__

    .xword ._L_str4__

.p2align 2

unit:
    .long 0
)arm";
#endif
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "stdlib/print", expected, true);
}

TEST_CASE("target/arm64: fixture: call_1")
{
#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr x10, [sp, #24]
    adrp x6, ._L_str1__@PAGE
    add x6, x6, ._L_str1__@PAGEOFF
    mov x10, x6
    str x10, [sp, #24]
    ldr x0, [sp, #24]
    bl identity
    mov x0, x0
    bl identity
    mov x0, x0
    bl identity
    ldr x0, [sp, #24]
    mov w1, #18
    bl print
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #1
    svc #0


identity:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov x0, x0
    ldp x29, x30, [sp], #16
    ret

.data

._L_str1__:
    .asciz "hello, how are you"
)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr x10, [sp, #24]
    adrp x6, ._L_str1__@PAGE
    add x6, x6, ._L_str1__@PAGEOFF
    mov x10, x6
    str x10, [sp, #24]
    ldr x0, [sp, #24]
    bl identity
    mov x0, x0
    bl identity
    mov x0, x0
    bl identity
    ldr x0, [sp, #24]
    mov w1, #18
    bl _print
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80


identity:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov x0, x0
    ldp x29, x30, [sp], #16
    ret

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "hello, how are you"
)arm";
#endif
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "call_1", expected, false);
}

TEST_CASE("target/arm64: fixture: call_2")
{
#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    bl test
    mov x0, x0
    ldr x10, [sp, #24]
    mov x10, x0
    str x10, [sp, #24]
    ldr x0, [sp, #24]
    bl test
    ldr x0, [sp, #24]
    mov w1, #11
    bl print
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #1
    svc #0


test:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov x0, x0
    ldp x29, x30, [sp], #16
    ret

.data

._L_str1__:
    .asciz "hello world"
)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    bl test
    mov x0, x0
    ldr x10, [sp, #24]
    mov x10, x0
    str x10, [sp, #24]
    ldr x0, [sp, #24]
    bl test
    ldr x0, [sp, #24]
    mov w1, #11
    bl _print
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80


test:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov x0, x0
    ldp x29, x30, [sp], #16
    ret

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "hello world"
)arm";
#endif
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "call_2", expected, false);
}

TEST_CASE("target/arm64: fixture: readme 2")
{
#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-48]!
    mov x29, sp
    str w0, [sp, #20]
    str x1, [sp, #28]
    ldr x10, [sp, #36]
    adrp x6, ._L_str4__@PAGE
    add x6, x6, ._L_str4__@PAGEOFF
    mov x10, x6
    str x10, [sp, #36]
._L2__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #1
    b.gt ._L4__main
._L3__main:
    b ._L1__main
._L4__main:
    ldr x0, [sp, #36]
    bl identity
    mov x0, x0
    bl identity
    mov x0, x0
    bl identity
    ldr x0, [sp, #36]
    ldr x10, [sp, #28]
    ldr x1, [x10, #8]
    bl printf
    adrp x6, strings@PAGE
    add x6, x6, strings@PAGEOFF
    ldr x0, [x6]
    mov w1, #14
    bl print
    b ._L3__main
._L1__main:
    ldp x29, x30, [sp], #48
    mov w0, #0
    mov x8, #1
    svc #0


identity:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov x0, x0
    ldp x29, x30, [sp], #16
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

.align 3

strings:
    .xword ._L_str1__

    .xword ._L_str3__

    .xword ._L_str2__
)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-48]!
    mov x29, sp
    str w0, [sp, #20]
    str x1, [sp, #28]
    ldr x10, [sp, #36]
    adrp x6, ._L_str4__@PAGE
    add x6, x6, ._L_str4__@PAGEOFF
    mov x10, x6
    str x10, [sp, #36]
._L2__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #1
    b.gt ._L4__main
._L3__main:
    b ._L1__main
._L4__main:
    ldr x0, [sp, #36]
    bl identity
    mov x0, x0
    bl identity
    mov x0, x0
    bl identity
    ldr x0, [sp, #36]
    ldr x10, [sp, #28]
    ldr x1, [x10, #8]
    bl _printf
    adrp x6, strings@PAGE
    add x6, x6, strings@PAGEOFF
    ldr x0, [x6]
    mov w1, #14
    bl _print
    b ._L3__main
._L1__main:
    ldp x29, x30, [sp], #48
    mov w0, #0
    mov x16, #1
    svc #0x80


identity:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov x0, x0
    ldp x29, x30, [sp], #16
    ret

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "good afternoon"

._L_str2__:
    .asciz "good evening"

._L_str3__:
    .asciz "good morning"

._L_str4__:
    .asciz "hello, how are you, %s\n"

.section __DATA,__data

.p2align 3

strings:
    .xword ._L_str1__

    .xword ._L_str3__

    .xword ._L_str2__
)arm";
#endif
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "readme_2", expected, false);
}

TEST_CASE("target/arm64: fixture: stdlib putchar")
{
    SETUP_ARM64_FIXTURE_SHOULD_THROW_FROM_AST("stdlib/putchar_2");
#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w0, 108
    bl putchar
    mov w0, 111
    bl putchar
    mov w0, 108
    bl putchar
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w0, 108
    bl _putchar
    mov w0, 111
    bl _putchar
    mov w0, 108
    bl _putchar
    ldp x29, x30, [sp], #16
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "stdlib/putchar_1", expected, false);
}

TEST_CASE("target/arm64: fixture: relational/if_1.b")
{
#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    mov w10, #10
    str w10, [sp, #20]
._L2__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #5
    b.le ._L4__main
._L3__main:
._L8__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #10
    b.eq ._L10__main
._L9__main:
._L14__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #5
    b.ge ._L16__main
._L15__main:
._L20__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #5
    b.ne ._L22__main
._L21__main:
._L26__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #8
    b.gt ._L28__main
._L27__main:
._L32__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #20
    b.lt ._L34__main
._L33__main:
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    mov w1, #5
    bl print
    mov w8, 1
    ldr w10, [sp, #20]
    mov w10, w8
    str w10, [sp, #20]
    b ._L1__main
._L4__main:
    ldr w10, [sp, #20]
    mov w10, #1
    str w10, [sp, #20]
    adrp x0, ._L_str6__@PAGE
    add x0, x0, ._L_str6__@PAGEOFF
    mov w1, #5
    bl printf
    b ._L3__main
._L10__main:
    adrp x0, ._L_str2__@PAGE
    add x0, x0, ._L_str2__@PAGEOFF
    mov w1, #10
    bl printf
    b ._L9__main
._L16__main:
    adrp x0, ._L_str4__@PAGE
    add x0, x0, ._L_str4__@PAGEOFF
    mov w1, #5
    bl printf
    b ._L15__main
._L22__main:
    adrp x0, ._L_str7__@PAGE
    add x0, x0, ._L_str7__@PAGEOFF
    mov w1, #5
    bl printf
    b ._L21__main
._L28__main:
    adrp x0, ._L_str3__@PAGE
    add x0, x0, ._L_str3__@PAGEOFF
    mov w1, #8
    bl printf
    b ._L27__main
._L34__main:
    adrp x0, ._L_str5__@PAGE
    add x0, x0, ._L_str5__@PAGEOFF
    mov w1, #20
    bl printf
    b ._L33__main
._L1__main:
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #1
    svc #0

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
)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    mov w10, #10
    str w10, [sp, #20]
._L2__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #5
    b.le ._L4__main
._L3__main:
._L8__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #10
    b.eq ._L10__main
._L9__main:
._L14__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #5
    b.ge ._L16__main
._L15__main:
._L20__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #5
    b.ne ._L22__main
._L21__main:
._L26__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #8
    b.gt ._L28__main
._L27__main:
._L32__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #20
    b.lt ._L34__main
._L33__main:
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    mov w1, #5
    bl _print
    mov w8, 1
    ldr w10, [sp, #20]
    mov w10, w8
    str w10, [sp, #20]
    b ._L1__main
._L4__main:
    ldr w10, [sp, #20]
    mov w10, #1
    str w10, [sp, #20]
    adrp x0, ._L_str6__@PAGE
    add x0, x0, ._L_str6__@PAGEOFF
    mov w1, #5
    bl _printf
    b ._L3__main
._L10__main:
    adrp x0, ._L_str2__@PAGE
    add x0, x0, ._L_str2__@PAGEOFF
    mov w1, #10
    bl _printf
    b ._L9__main
._L16__main:
    adrp x0, ._L_str4__@PAGE
    add x0, x0, ._L_str4__@PAGEOFF
    mov w1, #5
    bl _printf
    b ._L15__main
._L22__main:
    adrp x0, ._L_str7__@PAGE
    add x0, x0, ._L_str7__@PAGEOFF
    mov w1, #5
    bl _printf
    b ._L21__main
._L28__main:
    adrp x0, ._L_str3__@PAGE
    add x0, x0, ._L_str3__@PAGEOFF
    mov w1, #8
    bl _printf
    b ._L27__main
._L34__main:
    adrp x0, ._L_str5__@PAGE
    add x0, x0, ._L_str5__@PAGEOFF
    mov w1, #20
    bl _printf
    b ._L33__main
._L1__main:
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

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
)arm";
#endif
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "relational/if_1", expected, false);
}

TEST_CASE("target/arm64: fixture: relational while")
{
#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    mov w10, #100
    str w10, [sp, #20]
    ldr w10, [sp, #24]
    mov w10, #4
    str w10, [sp, #24]
._L2__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #50
    b.gt ._L4__main
._L3__main:
._L11__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #48
    b.eq ._L13__main
    b ._L16__main
._L12__main:
    adrp x0, ._L_str2__@PAGE
    add x0, x0, ._L_str2__@PAGEOFF
    ldr w1, [sp, #20]
    ldr w2, [sp, #24]
    bl printf
    b ._L1__main
._L4__main:
._L6__main:
._L8__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #50
    b.ge ._L7__main
    b ._L3__main
._L7__main:
    ldr w10, [sp, #20]
    sub w10, w10, #1
    str w10, [sp, #20]
    ldr w10, [sp, #20]
    mov w8, w10
    sub w8, w8, #1
    ldr w10, [sp, #24]
    mov w10, w8
    str w10, [sp, #24]
    b ._L6__main
._L13__main:
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    mov w1, #4
    bl print
    b ._L12__main
._L16__main:
    adrp x0, ._L_str3__@PAGE
    add x0, x0, ._L_str3__@PAGEOFF
    mov w1, #6
    bl print
    b ._L12__main
._L1__main:
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #1
    svc #0

.data

._L_str1__:
    .asciz "no\n"

._L_str2__:
    .asciz "x, y: %d %d\n"

._L_str3__:
    .asciz "yes!\n"
)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    mov w10, #100
    str w10, [sp, #20]
    ldr w10, [sp, #24]
    mov w10, #4
    str w10, [sp, #24]
._L2__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #50
    b.gt ._L4__main
._L3__main:
._L11__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #48
    b.eq ._L13__main
    b ._L16__main
._L12__main:
    adrp x0, ._L_str2__@PAGE
    add x0, x0, ._L_str2__@PAGEOFF
    ldr w1, [sp, #20]
    ldr w2, [sp, #24]
    bl _printf
    b ._L1__main
._L4__main:
._L6__main:
._L8__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #50
    b.ge ._L7__main
    b ._L3__main
._L7__main:
    ldr w10, [sp, #20]
    sub w10, w10, #1
    str w10, [sp, #20]
    ldr w10, [sp, #20]
    mov w8, w10
    sub w8, w8, #1
    ldr w10, [sp, #24]
    mov w10, w8
    str w10, [sp, #24]
    b ._L6__main
._L13__main:
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    mov w1, #4
    bl _print
    b ._L12__main
._L16__main:
    adrp x0, ._L_str3__@PAGE
    add x0, x0, ._L_str3__@PAGEOFF
    mov w1, #6
    bl _print
    b ._L12__main
._L1__main:
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "no\n"

._L_str2__:
    .asciz "x, y: %d %d\n"

._L_str3__:
    .asciz "yes!\n"
)arm";
#endif
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "relational/while_1", expected, false);
}

TEST_CASE("target/arm64: fixture: relational switch")
{
#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    mov w10, #10
    str w10, [sp, #20]
._L2__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #5
    b.ge ._L4__main
._L3__main:
    ldr w10, [sp, #24]
    mov w10, #10
    str w10, [sp, #24]
    b ._L1__main
._L4__main:
    mov w8, w10
    cmp w8, #10
    b.eq ._L8__main
    mov w8, w10
    cmp w8, #6
    b.eq ._L16__main
    mov w8, w10
    cmp w8, #7
    b.eq ._L18__main
._L17__main:
._L15__main:
._L7__main:
    b ._L3__main
._L8__main:
._L9__main:
._L11__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #1
    b.gt ._L10__main
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    ldr w1, [sp, #20]
    bl printf
    b ._L7__main
._L10__main:
    ldr w10, [sp, #20]
    sub w10, w10, #1
    str w10, [sp, #20]
    b ._L9__main
._L16__main:
    ldr w10, [sp, #24]
    mov w10, #2
    str w10, [sp, #24]
    b ._L3__main
._L18__main:
    ldr w10, [sp, #20]
    mov w10, #5
    str w10, [sp, #20]
    b ._L17__main
._L1__main:
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #1
    svc #0

.data

._L_str1__:
    .asciz "should say 1: %d, %b\n"
)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    mov w10, #10
    str w10, [sp, #20]
._L2__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #5
    b.ge ._L4__main
._L3__main:
    ldr w10, [sp, #24]
    mov w10, #10
    str w10, [sp, #24]
    b ._L1__main
._L4__main:
    mov w8, w10
    cmp w8, #10
    b.eq ._L8__main
    mov w8, w10
    cmp w8, #6
    b.eq ._L16__main
    mov w8, w10
    cmp w8, #7
    b.eq ._L18__main
._L17__main:
._L15__main:
._L7__main:
    b ._L3__main
._L8__main:
._L9__main:
._L11__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #1
    b.gt ._L10__main
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    ldr w1, [sp, #20]
    bl _printf
    b ._L7__main
._L10__main:
    ldr w10, [sp, #20]
    sub w10, w10, #1
    str w10, [sp, #20]
    b ._L9__main
._L16__main:
    ldr w10, [sp, #24]
    mov w10, #2
    str w10, [sp, #24]
    b ._L3__main
._L18__main:
    ldr w10, [sp, #20]
    mov w10, #5
    str w10, [sp, #20]
    b ._L17__main
._L1__main:
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "should say 1: %d, %b\n"
)arm";
#endif
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "relational/switch_1", expected, false);
}

TEST_CASE("target/arm64: fixture: relational if 2")
{
#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr x10, [sp, #28]
    adrp x6, ._L_str1__@PAGE
    add x6, x6, ._L_str1__@PAGEOFF
    mov x10, x6
    str x10, [sp, #28]
    ldr w10, [sp, #20]
    mov w10, #5
    str w10, [sp, #20]
._L2__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #5
    b.gt ._L4__main
    b ._L6__main
._L3__main:
    ldr x0, [sp, #28]
    mov w1, #6
    bl print
    b ._L1__main
._L4__main:
    ldr x10, [sp, #28]
    adrp x6, ._L_str2__@PAGE
    add x6, x6, ._L_str2__@PAGEOFF
    mov x10, x6
    str x10, [sp, #28]
    b ._L3__main
._L6__main:
    ldr x10, [sp, #28]
    adrp x6, ._L_str3__@PAGE
    add x6, x6, ._L_str3__@PAGEOFF
    mov x10, x6
    str x10, [sp, #28]
    b ._L3__main
._L1__main:
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #1
    svc #0

.data

._L_str1__:
    .asciz "no"

._L_str2__:
    .asciz "yes"

._L_str3__:
    .asciz "yes!!!"
)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr x10, [sp, #28]
    adrp x6, ._L_str1__@PAGE
    add x6, x6, ._L_str1__@PAGEOFF
    mov x10, x6
    str x10, [sp, #28]
    ldr w10, [sp, #20]
    mov w10, #5
    str w10, [sp, #20]
._L2__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #5
    b.gt ._L4__main
    b ._L6__main
._L3__main:
    ldr x0, [sp, #28]
    mov w1, #6
    bl _print
    b ._L1__main
._L4__main:
    ldr x10, [sp, #28]
    adrp x6, ._L_str2__@PAGE
    add x6, x6, ._L_str2__@PAGEOFF
    mov x10, x6
    str x10, [sp, #28]
    b ._L3__main
._L6__main:
    ldr x10, [sp, #28]
    adrp x6, ._L_str3__@PAGE
    add x6, x6, ._L_str3__@PAGEOFF
    mov x10, x6
    str x10, [sp, #28]
    b ._L3__main
._L1__main:
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "no"

._L_str2__:
    .asciz "yes"

._L_str3__:
    .asciz "yes!!!"
)arm";
#endif
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "relational/if_2", expected, false);
}

TEST_CASE("target/arm64: fixture: stdlib printf")
{
#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    mov w10, #5
    str w10, [sp, #20]
._L2__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #5
    b.gt ._L4__main
    b ._L7__main
._L3__main:
    b ._L1__main
._L4__main:
    adrp x0, ._L_str2__@PAGE
    add x0, x0, ._L_str2__@PAGEOFF
    mov w1, #14
    bl print
    b ._L3__main
._L7__main:
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    adrp x1, ._L_str3__@PAGE
    add x1, x1, ._L_str3__@PAGEOFF
    mov w2, #5
    adrp x8, ._L_double4__@PAGE
    ldr d3, [x8, ._L_double4__@PAGEOFF]
    mov w4, 120
    mov w5, #1
    bl printf
    b ._L3__main
._L1__main:
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #1
    svc #0

.data

._L_str1__:
    .asciz "%s %d %g %c %b"

._L_str2__:
    .asciz "greater than 5"

._L_str3__:
    .asciz "hello"

._L_double4__:
    .double 5.2
)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    ldr w10, [sp, #20]
    mov w10, #5
    str w10, [sp, #20]
._L2__main:
    ldr w10, [sp, #20]
    mov w8, w10
    cmp w8, #5
    b.gt ._L4__main
    b ._L7__main
._L3__main:
    b ._L1__main
._L4__main:
    adrp x0, ._L_str2__@PAGE
    add x0, x0, ._L_str2__@PAGEOFF
    mov w1, #14
    bl _print
    b ._L3__main
._L7__main:
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    adrp x1, ._L_str3__@PAGE
    add x1, x1, ._L_str3__@PAGEOFF
    mov w2, #5
    adrp x8, ._L_double4__@PAGE
    ldr d3, [x8, ._L_double4__@PAGEOFF]
    mov w4, 120
    mov w5, #1
    bl _printf
    b ._L3__main
._L1__main:
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "%s %d %g %c %b"

._L_str2__:
    .asciz "greater than 5"

._L_str3__:
    .asciz "hello"

.section __DATA,__data

._L_double4__:
    .double 5.2
)arm";
#endif
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "stdlib/printf_1", expected, false);
}

TEST_CASE("target/arm64: fixture: argc_argv")
{
#if defined(__linux__)
    std::string expected = R"arm(
.text

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    str w0, [sp, #20]
    str x1, [sp, #28]
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    ldr w1, [sp, #20]
    bl printf
    adrp x0, ._L_str2__@PAGE
    add x0, x0, ._L_str2__@PAGEOFF
    ldr x10, [sp, #28]
    ldr x1, [x10, #8]
    bl printf
    adrp x0, ._L_str3__@PAGE
    add x0, x0, ._L_str3__@PAGEOFF
    ldr x10, [sp, #28]
    ldr x1, [x10, #16]
    bl printf
    adrp x0, ._L_str4__@PAGE
    add x0, x0, ._L_str4__@PAGEOFF
    ldr x10, [sp, #28]
    ldr x1, [x10, #24]
    bl printf
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x8, #1
    svc #0

.data

._L_str1__:
    .asciz "argc count: %d\n"

._L_str2__:
    .asciz "argv 1: %s\n"

._L_str3__:
    .asciz "argv 2: %s\n"

._L_str4__:
    .asciz "argv 3: %s\n"
)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .p2align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    str w0, [sp, #20]
    str x1, [sp, #28]
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    ldr w1, [sp, #20]
    bl _printf
    adrp x0, ._L_str2__@PAGE
    add x0, x0, ._L_str2__@PAGEOFF
    ldr x10, [sp, #28]
    ldr x1, [x10, #8]
    bl _printf
    adrp x0, ._L_str3__@PAGE
    add x0, x0, ._L_str3__@PAGEOFF
    ldr x10, [sp, #28]
    ldr x1, [x10, #16]
    bl _printf
    adrp x0, ._L_str4__@PAGE
    add x0, x0, ._L_str4__@PAGEOFF
    ldr x10, [sp, #28]
    ldr x1, [x10, #24]
    bl _printf
    ldp x29, x30, [sp], #32
    mov w0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

._L_str1__:
    .asciz "argc count: %d\n"

._L_str2__:
    .asciz "argv 1: %s\n"

._L_str3__:
    .asciz "argv 2: %s\n"

._L_str4__:
    .asciz "argv 3: %s\n"
)arm";
#endif
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(
        "argc_argv", expected, false);
}