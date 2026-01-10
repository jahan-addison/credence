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
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    str x23, [sp, #16]
    mov w9, #1
    mov w10, #5
    mov w8, w10
    sub w8, w8, #0
    add w8, w8, w9
    mov w23, #10
    mul w8, w8, w23
    mov w9, w8
    ldr x23, [sp, #16]
    ldp x29, x30, [sp], #32
    mov x0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    str x23, [sp, #16]
    mov w9, #1
    mov w10, #5
    mov w8, w10
    sub w8, w8, #0
    add w8, w8, w9
    mov w23, #10
    mul w8, w8, w23
    mov w9, w8
    ldr x23, [sp, #16]
    ldp x29, x30, [sp], #32
    mov x0, #0
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
    str x23, [sp, #16]
    mov w9, #1
    mov w10, #5
    mov w8, w10
    sub w8, w8, #0
    add w8, w8, w9
    mov w23, #10
    mul w8, w8, w23
    mov w9, w8
    sub sp, sp, #16
    str w9, [sp, #0]
    str w10, [sp, #4]
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    mov w1, w9
    bl printf
    str w9, [sp, #0]
    str w10, [sp, #4]
    add sp, sp, #16
    ldr x23, [sp, #16]
    ldp x29, x30, [sp], #32
    mov x0, #0
    mov x8, #1
    svc #0

.data

._L_str1__:
    .asciz "m is %d\n"
)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    str x23, [sp, #16]
    mov w9, #1
    mov w10, #5
    mov w8, w10
    sub w8, w8, #0
    add w8, w8, w9
    mov w23, #10
    mul w8, w8, w23
    mov w9, w8
    sub sp, sp, #16
    str w9, [sp, #0]
    str w10, [sp, #4]
    adrp x0, ._L_str1__@PAGE
    add x0, x0, ._L_str1__@PAGEOFF
    mov w1, w9
    bl _printf
    str w9, [sp, #0]
    str w10, [sp, #4]
    add sp, sp, #16
    ldr x23, [sp, #16]
    ldp x29, x30, [sp], #32
    mov x0, #0
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
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    mov w8, #4
    add w8, w8, #1
    mov w9, w8
    mov w8, #2
    sub w8, w8, w9
    mov w10, w8
    ldp x29, x30, [sp], #32
    mov x0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    mov w8, #4
    add w8, w8, #1
    mov w9, w8
    mov w8, #2
    sub w8, w8, w9
    mov w10, w8
    ldp x29, x30, [sp], #32
    mov x0, #0
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
    stp x29, x30, [sp, #-32]!
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
    ldp x29, x30, [sp], #32
    mov x0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
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
    ldp x29, x30, [sp], #32
    mov x0, #0
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
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mov w10, #5
    add w9, w9, #1
    sub w10, w10, #1
    add w10, w10, #1
    ldp x29, x30, [sp], #32
    mov x0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mov w10, #5
    add w9, w9, #1
    sub w10, w10, #1
    add w10, w10, #1
    ldp x29, x30, [sp], #32
    mov x0, #0
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
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mvn w9, w9
    mov w10, w8
    add w9, w9, #1
    sub w10, w10, #1
    add w10, w10, #1
    ldp x29, x30, [sp], #32
    mov x0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mvn w9, w9
    mov w10, w8
    add w9, w9, #1
    sub w10, w10, #1
    add w10, w10, #1
    ldp x29, x30, [sp], #32
    mov x0, #0
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
    stp x29, x30, [sp, #-32]!
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
    ldp x29, x30, [sp], #32
    mov x0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
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
    ldp x29, x30, [sp], #32
    mov x0, #0
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
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    mov w8, 1
    mov w9, w8
    mov w10, #1
    mov w11, #0
    ldp x29, x30, [sp], #32
    mov x0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    mov w8, 1
    mov w9, w8
    mov w10, #1
    mov w11, #0
    ldp x29, x30, [sp], #32
    mov x0, #0
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
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    str x23, [sp, #16]
    movn w8, #10
    mov w9, w8
    mov w23, #10
    eor w8, w23, w9
    orr w8, w8, #1
    mov w10, w8
    mvn w8, w9
    mvn w23, w10
    and w8, w8, w23
    mov w11, w8
    ldr x23, [sp, #16]
    ldp x29, x30, [sp], #32
    mov x0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    str x23, [sp, #16]
    movn w8, #10
    mov w9, w8
    mov w23, #10
    eor w8, w23, w9
    orr w8, w8, #1
    mov w10, w8
    mvn w8, w9
    mvn w23, w10
    and w8, w8, w23
    mov w11, w8
    ldr x23, [sp, #16]
    ldp x29, x30, [sp], #32
    mov x0, #0
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
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    str x23, [sp, #16]
    movn w8, #10
    mov w9, w8
    mov w10, #5
    eor w8, w9, w10
    lsr w23, w10, #5
    orr w8, w8, w23
    mov w11, w8
    mvn w8, w9
    mvn w23, w10
    and w8, w8, w23
    mov w11, w8
    ldr x23, [sp, #16]
    ldp x29, x30, [sp], #32
    mov x0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    str x23, [sp, #16]
    movn w8, #10
    mov w9, w8
    mov w10, #5
    eor w8, w9, w10
    lsr w23, w10, #5
    orr w8, w8, w23
    mov w11, w8
    mvn w8, w9
    mvn w23, w10
    and w8, w8, w23
    mov w11, w8
    ldr x23, [sp, #16]
    ldp x29, x30, [sp], #32
    mov x0, #0
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
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    str x23, [sp, #16]
    movn w8, #10
    mov w9, w8
    mov w10, #5
    eor w8, w9, w10
    lsr w23, w10, w9
    orr w8, w8, w23
    mov w11, w8
    mvn w8, w9
    mvn w23, w10
    and w8, w8, w23
    mov w11, w8
    ldr x23, [sp, #16]
    ldp x29, x30, [sp], #32
    mov x0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    str x23, [sp, #16]
    movn w8, #10
    mov w9, w8
    mov w10, #5
    eor w8, w9, w10
    lsr w23, w10, w9
    orr w8, w8, w23
    mov w11, w8
    mvn w8, w9
    mvn w23, w10
    and w8, w8, w23
    mov w11, w8
    ldr x23, [sp, #16]
    ldp x29, x30, [sp], #32
    mov x0, #0
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
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mov w10, #5
    mov w8, #30
    orr w8, w8, #15
    mov w11, w8
    mvn w8, w9
    mvn w23, w10
    and w8, w8, w23
    mov w11, w8
    ldp x29, x30, [sp], #32
    mov x0, #0
    mov x8, #1
    svc #0

.data

)arm";
#elif defined(__APPLE__) || defined(__bsdi__)
    std::string expected = R"arm(
.section	__TEXT,__text,regular,pure_instructions

    .align 3

    .global _start

_start:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    movn w8, #10
    mov w9, w8
    mov w10, #5
    mov w8, #30
    orr w8, w8, #15
    mov w11, w8
    mvn w8, w9
    mvn w23, w10
    and w8, w8, w23
    mov w11, w8
    ldp x29, x30, [sp], #32
    mov x0, #0
    mov x16, #1
    svc #0x80

.section	__TEXT,__cstring,cstring_literals

)arm";
#endif
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("bitwise_4", expected);
}