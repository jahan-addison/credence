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
        fixture_path.append("test/fixtures/arm64/ast");                      \
        auto file_path =                                                     \
            fs::path(fixture_path).append(fmt::format("{}.json", ast_path)); \
        auto fixture_content =                                               \
            easyjson::JSON::load_file(file_path.string()).to_deque();        \
        credence::target::arm64::emit(                                       \
            test, fixture_content[0], fixture_content[1], true);             \
        REQUIRE(test.str() == expected);                                     \
    } while (0)

#define SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST_FROM_AST(                     \
    ast_path, expected, syscall)                                               \
    do {                                                                       \
        using namespace credence::target::arm64;                               \
        auto test = std::ostringstream{};                                      \
        auto fixture_path = fs::path(ROOT_PATH);                               \
        fixture_path.append("test/fixtures/arm64/ast");                        \
        auto file_path =                                                       \
            fs::path(fixture_path).append(fmt::format("{}.json", ast_path));   \
        auto fixture_content =                                                 \
            easyjson::JSON::load_file(file_path.string()).to_deque();          \
        runtime::add_stdlib_functions_to_symbols(fixture_content[0], syscall); \
        credence::target::arm64::emit(                                         \
            test, fixture_content[0], fixture_content[1], true);               \
        REQUIRE(test.str() == expected);                                       \
    } while (0)

#define SETUP_ARM64_FIXTURE_SHOULD_THROW_FROM_AST(ast_path)                  \
    do {                                                                     \
        using namespace credence::target::arm64;                             \
        auto test = std::ostringstream{};                                    \
        auto fixture_path = fs::path(ROOT_PATH);                             \
        fixture_path.append("test/fixtures/arm64/ast");                      \
        auto file_path =                                                     \
            fs::path(fixture_path).append(fmt::format("{}.json", ast_path)); \
        auto fixture_content =                                               \
            easyjson::JSON::load_file(file_path.string()).to_deque();        \
        REQUIRE_THROWS(credence::target::arm64::emit(                        \
            test, fixture_content[0], fixture_content[1], true));            \
    } while (0)

TEST_CASE("target/arm64: fixture: math_constant.b")
{
    std::string expected = R"arm(
.align 2

.data

.text
    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w0, #1
    mov w0, #5
    str w0, [sp, #4]
    ldr  w0, [sp, #4]
    sub w0, w0, #0
    ldr w0, [sp, #0]
    add w0, w0, w0
    str w0, [sp, #0]
    mov w1, #10
    mul w0, w0, w1
    str w0, [sp, #8]
    add sp, sp, #16
    ldp x29, x30, [sp], #16

)arm";
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("math_constant", expected);
}

TEST_CASE("target/arm64: fixture: math_constant_2.b")
{
    std::string expected = R"arm(
.align 2

.data

.text
    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w0, #4
    add w0, w0, #1
    str w0, [sp, #4]
    mov w0, #2
    ldr w1, [sp, #4]
    sub w0, w0, w1
    str w0, [sp, #8]
    add sp, sp, #16
    ldp x29, x30, [sp], #16

)arm";
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("math_constant_2", expected);
}

TEST_CASE("target/arm64: fixture: math_constant_4.b")
{
    std::string expected = R"arm(
.align 2

.data

.text
    .global _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w0, #20
    str w0, [sp, #4]
    mov w0, #10
    str w0, [sp, #8]
    ldr  w0, [sp, #4]
    sdiv x0, x0, [sp, #8]
    str w0, [sp, #12]
    ldr  w0, [sp, #4]
    ldr w1, [sp, #8]
    add w0, w0, w1
    str w0, [sp, #12]
    ldr  w0, [sp, #4]
    ldr w1, [sp, #8]
    sub w0, w0, w1
    str w0, [sp, #12]
    ldr  w0, [sp, #4]
    ldr w1, [sp, #8]
    mul w0, w0, w1
    str w0, [sp, #12]
    ldr  w0, [sp, #4]
    str x1, [sp, #12]
    mov w0, #10
    str w0, [sp, #12]
    add sp, sp, #16
    ldp x29, x30, [sp], #16

)arm";
    SETUP_ARM64_FIXTURE_AND_TEST_FROM_AST("math_constant_4", expected);
}