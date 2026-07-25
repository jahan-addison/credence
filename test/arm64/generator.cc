/**
 * target/arm64: code generator tests
 */
#include <doctest/doctest.h>

#include <credence/target/arm64/generator.h>
#include <credence/target/arm64/runtime.h>
#include <cstdlib>
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

inline std::string read_file_from_path(std::string_view path)
{
    std::ifstream f(path.data(), std::ios::in | std::ios::binary);
    const auto sz = fs::file_size(path);

    std::string result(sz, '\0');
    f.read(result.data(), sz);

    return result;
}

inline std::filesystem::path get_root_path()
{
    if (const char* env_root = std::getenv("CREDENCE_TEST_ROOT"))
        return fs::path(env_root);
    return fs::path(ROOT_PATH);
}

#define SETUP_ARM64_FIXTURE_AND_TEST(path_name, os)                            \
    do {                                                                       \
        using namespace credence::target::arm64;                               \
        auto test = std::ostringstream{};                                      \
        auto fixture_path =                                                    \
            get_root_path().append("test/fixtures/platform/ast");              \
        auto expected_root =                                                   \
            get_root_path().append(fmt::format("test/arm64/expected/{}", os)); \
        auto file_path =                                                       \
            fs::path(fixture_path).append(fmt::format("{}.json", path_name));  \
        auto expected_path =                                                   \
            fs::path(expected_root).append(fmt::format("{}.s", path_name));    \
        auto fixture_content =                                                 \
            easyjson::JSON::load_file(file_path.string()).to_deque();          \
        credence::target::arm64::emit(                                         \
            test, fixture_content[0], fixture_content[1], true);               \
        auto expected_content = read_file_from_path(expected_path.string());   \
        REQUIRE(test.str() == expected_content);                               \
    } while (0)

#define SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST(path_name, os, syscall)       \
    do {                                                                       \
        using namespace credence::target::arm64;                               \
        auto test = std::ostringstream{};                                      \
        auto fixture_path =                                                    \
            get_root_path().append("test/fixtures/platform/ast");              \
        auto expected_root =                                                   \
            get_root_path().append(fmt::format("test/arm64/expected/{}", os)); \
        auto file_path =                                                       \
            fs::path(fixture_path).append(fmt::format("{}.json", path_name));  \
        auto expected_path =                                                   \
            fs::path(expected_root).append(fmt::format("{}.s", path_name));    \
        auto fixture_content =                                                 \
            easyjson::JSON::load_file(file_path.string()).to_deque();          \
        credence::target::common::runtime::add_stdlib_functions_to_symbols(    \
            fixture_content[0],                                                \
            credence::target::common::assembly::OS_Type::BSD,                  \
            credence::target::common::assembly::Arch_Type::ARM64,              \
            syscall);                                                          \
        credence::target::arm64::emit(                                         \
            test, fixture_content[0], fixture_content[1], false);              \
        auto expected_content = read_file_from_path(expected_path.string());   \
        REQUIRE(test.str() == expected_content);                               \
    } while (0)

#define SETUP_ARM64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(                   \
    path_name, os, syscall)                                                    \
    do {                                                                       \
        using namespace credence::target::arm64;                               \
        auto test = std::ostringstream{};                                      \
        auto fixture_path =                                                    \
            get_root_path().append("test/fixtures/platform/ast");              \
        auto expected_root =                                                   \
            get_root_path().append(fmt::format("test/arm64/expected/{}", os)); \
        auto file_path =                                                       \
            fs::path(fixture_path).append(fmt::format("{}.json", path_name));  \
        auto expected_path =                                                   \
            fs::path(expected_root).append(fmt::format("{}.s", path_name));    \
        auto fixture_content =                                                 \
            easyjson::JSON::load_file(file_path.string()).to_deque();          \
        credence::target::common::runtime::add_stdlib_functions_to_symbols(    \
            fixture_content[0],                                                \
            credence::target::common::assembly::OS_Type::BSD,                  \
            credence::target::common::assembly::Arch_Type::ARM64,              \
            syscall);                                                          \
        credence::target::arm64::emit(                                         \
            test, fixture_content[0], fixture_content[1], true);               \
        auto expected_content = read_file_from_path(expected_path.string());   \
        REQUIRE(test.str() == expected_content);                               \
    } while (0)

#define SETUP_ARM64_FIXTURE_SHOULD_THROW_FROM_AST(ast_path)                  \
    do {                                                                     \
        using namespace credence::target::arm64;                             \
        auto test = std::ostringstream{};                                    \
        auto fixture_path = get_root_path();                                 \
        fixture_path.append("test/fixtures/platform/ast");                   \
        auto file_path =                                                     \
            fs::path(fixture_path).append(fmt::format("{}.json", ast_path)); \
        auto fixture_content =                                               \
            easyjson::JSON::load_file(file_path.string()).to_deque();        \
        REQUIRE_THROWS(credence::target::arm64::emit(                        \
            test, fixture_content[0], fixture_content[1], true));            \
    } while (0)

TEST_CASE("target/arm64: fixture: math_constant.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("math_constant", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("math_constant", "bsd");
#endif
};

TEST_CASE("target/arm64: fixture: math_constant_8.b")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST("math_constant_8", "linux", true);
#else
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST("math_constant_8", "bsd", true);
#endif
}

TEST_CASE("target/arm64: fixture: math_constant_2.b")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("math_constant_2", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("math_constant_2", "bsd");
#endif
}

TEST_CASE("target/arm64: fixture: math_constant_4.b")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("math_constant_4", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("math_constant_4", "bsd");
#endif
}

TEST_CASE("target/arm64: fixture: math_constant_5.b")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("math_constant_5", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("math_constant_5", "bsd");
#endif
}

TEST_CASE("target/arm64: fixture: math_constant_6.b")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("math_constant_6", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("math_constant_6", "bsd");
#endif
}

TEST_CASE("target/arm64: fixture: math_constant_7.b")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("math_constant_7", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("math_constant_7", "bsd");
#endif
}

TEST_CASE("target/arm64: fixture: relation_constant.b")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("relation_constant", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("relation_constant", "bsd");
#endif
}

TEST_CASE("target/arm64: fixture: bitwise_constant_1.b")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("bitwise_constant_1", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("bitwise_constant_1", "bsd");
#endif
}

TEST_CASE("target/arm64: fixture: bitwise_2.b")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("bitwise_2", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("bitwise_2", "bsd");
#endif
}

TEST_CASE("target/arm64: fixture: bitwise_3.b")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("bitwise_3", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("bitwise_3", "bsd");
#endif
}

TEST_CASE("target/arm64: fixture: bitwise_4.b")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("bitwise_4", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("bitwise_4", "bsd");
#endif
}

TEST_CASE("target/arm64: fixture: pointers_1.b")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("pointers_1", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("pointers_1", "bsd");
#endif
}

TEST_CASE("target/arm64: fixture: pointers_2.b")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("pointers_2", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("pointers_2", "bsd");
#endif
}

TEST_CASE("target/arm64: fixture: pointers_3.b")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("pointers_3", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("pointers_3", "bsd");
#endif
}

TEST_CASE("target/arm64: fixture: pointers_4.b")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("pointers_4", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("pointers_4", "bsd");
#endif
}

TEST_CASE("target/arm64: fixture: pointers_5.b")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("pointers_5", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("pointers_5", "bsd");
#endif
}

TEST_CASE("target/arm64: fixture: string_1.b")
{
    SETUP_ARM64_FIXTURE_SHOULD_THROW_FROM_AST("string_2");

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("string_1", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("string_1", "bsd");
#endif
}

TEST_CASE("target/arm64: fixture: vector_1.b")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("vector_1", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("vector_1", "bsd");
#endif
}

TEST_CASE("target/arm64: fixture: vector_2.b")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("vector_2", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("vector_2", "bsd");
#endif
}

TEST_CASE("target/arm64: fixture: vector_4.b")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "vector_4", "linux", true);
#else
    SETUP_ARM64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "vector_4", "bsd", true);
#endif
}

TEST_CASE("target/arm64: fixture: globals 1, 2")
{
    SETUP_ARM64_FIXTURE_SHOULD_THROW_FROM_AST("globals_2");

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_FIXTURE_AND_TEST("globals_1", "linux");
#else
    SETUP_ARM64_FIXTURE_AND_TEST("globals_1", "bsd");
#endif
}

TEST_CASE("target/arm64: fixture: globals 3")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "globals_3", "linux", true);
#else
    SETUP_ARM64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "globals_3", "bsd", true);
#endif
}

TEST_CASE("target/arm64: fixture: syscall kernel write")
{

#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST("stdlib/write", "linux", true);
#else
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST("stdlib/write", "bsd", true);
#endif
}

TEST_CASE("target/arm64: fixture: stdlib print")
{
    SETUP_ARM64_FIXTURE_SHOULD_THROW_FROM_AST("stdlib/print_2");
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST("stdlib/print", "linux", true);
#else
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST("stdlib/print", "bsd", true);
#endif
}

TEST_CASE("target/arm64: fixture: call_1")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "call_1", "linux", false);
#else
    SETUP_ARM64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST("call_1", "bsd", false);
#endif
}

TEST_CASE("target/arm64: fixture: call_2")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "call_2", "linux", false);
#else
    SETUP_ARM64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST("call_2", "bsd", false);
#endif
}

TEST_CASE("target/arm64: fixture: readme 2")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST("readme_2", "linux", false);
#else
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST("readme_2", "bsd", false);
#endif
}

TEST_CASE("target/arm64: fixture: stdlib putchar")
{
    SETUP_ARM64_FIXTURE_SHOULD_THROW_FROM_AST("stdlib/putchar_2");
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST(
        "stdlib/putchar_1", "linux", false);
#else
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST("stdlib/putchar_1", "bsd", false);
#endif
}

TEST_CASE("target/arm64: fixture: relational/if_1.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "relational/if_1", "linux", false);
#else
    SETUP_ARM64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "relational/if_1", "bsd", false);
#endif
}

TEST_CASE("target/arm64: fixture: relational while")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "relational/while_1", "linux", false);
#else
    SETUP_ARM64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "relational/while_1", "bsd", false);
#endif
}

TEST_CASE("target/arm64: fixture: relational switch")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "relational/switch_1", "linux", false);
#else
    SETUP_ARM64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "relational/switch_1", "bsd", false);
#endif
}

TEST_CASE("target/arm64: fixture: relational if 2")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "relational/if_2", "linux", false);
#else
    SETUP_ARM64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "relational/if_2", "bsd", false);
#endif
}

TEST_CASE("target/arm64: fixture: stdlib printf")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST("stdlib/printf_1", "linux", false);
#else
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST("stdlib/printf_1", "bsd", false);
#endif
}

TEST_CASE("target/arm64: fixture: argc_argv")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST("argc_argv", "linux", false);
#else
    SETUP_ARM64_WITH_STDLIB_FIXTURE_AND_TEST("argc_argv", "bsd", false);
#endif
}