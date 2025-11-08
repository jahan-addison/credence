#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase, TEST_CASE

#include <credence/target/x86_64/generator.h> // for emit
#include <filesystem>                         // for path
#include <format>                             // for format
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

inline auto fixture_files_root_path()
{
    auto fixtures_path = fs::path(ROOT_PATH);
    fixtures_path.append("test/fixtures/x86_64/ast");
    return fixtures_path;
}

TEST_CASE("target/x86_64: fixture: math_constant.b")
{
    using namespace credence::target::x86_64;
    auto test = std::ostringstream{};
    auto fixture_path = fs::path(ROOT_PATH);
    fixture_path.append("test/fixtures/x86_64/ast");
    // clang-format off
    auto file_path = fs::path(fixture_path)
        .append(std::format("{}.json", "math_constant"));
    // clang-format on
    auto fixture_content = json::JSON::load_file(file_path.string()).to_deque();
    credence::target::x86_64::emit(
        test, fixture_content[0], fixture_content[1]);
    std::string expected = R"x86(
.intel_syntax noprefix

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
    REQUIRE(test.str() == expected);
}

TEST_CASE("target/x86_64: fixture: math_constant_2.b")
{
    using namespace credence::target::x86_64;
    auto test = std::ostringstream{};
    auto fixture_path = fs::path(ROOT_PATH);
    fixture_path.append("test/fixtures/x86_64/ast");
    // clang-format off
    auto file_path = fs::path(fixture_path)
        .append(std::format("{}.json", "math_constant_2"));
    // clang-format on
    auto fixture_content = json::JSON::load_file(file_path.string()).to_deque();
    credence::target::x86_64::emit(
        test, fixture_content[0], fixture_content[1]);
    std::string expected = R"x86(
.intel_syntax noprefix

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
    REQUIRE(test.str() == expected);
}

TEST_CASE("target/x86_64: fixture: math_constant_4.b")
{
    using namespace credence::target::x86_64;
    auto test = std::ostringstream{};
    auto fixture_path = fs::path(ROOT_PATH);
    fixture_path.append("test/fixtures/x86_64/ast");
    // clang-format off
    auto file_path = fs::path(fixture_path)
        .append(std::format("{}.json", "math_constant_4"));
    // clang-format on
    auto fixture_content = json::JSON::load_file(file_path.string()).to_deque();
    credence::target::x86_64::emit(
        test, fixture_content[0], fixture_content[1]);
    std::string expected = R"x86(
.intel_syntax noprefix

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
    mov esi, dword ptr [rbp - 8]
    idiv esi
    mov dword ptr [rbp - 12], edx
    mov eax, 10
    mov dword ptr [rbp - 12], eax
_L1:
    xor eax, eax
    pop rbp
    ret

)x86";
    REQUIRE(test.str() == expected);
}