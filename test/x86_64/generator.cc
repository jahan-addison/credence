#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase, TEST_CASE

#include <credence/frontend/compile.h>        // for compile
#include <credence/frontend/hir/hir.h>        // for Unit
#include <credence/ir/symbols.h>              // for hoisted_symbols
#include <credence/target/x86_64/generator.h> // for emit
#include <credence/target/x86_64/runtime.h>   // for library
#include <easyjson.h>                         // for JSON
#include <filesystem>                         // for path
#include <fmt/format.h>                       // for format
#include <fstream>
#include <sstream> // for char_traits, basic_ost...
#include <string>  // for basic_string, allocator
#include <utility> // for pair

using namespace credence;
using namespace credence::target::x86_64;

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

/**
 * @brief Whether the run should rewrite the golden files instead of testing
 */
inline bool blessing_fixtures()
{
    return std::getenv("CREDENCE_BLESS") != nullptr;
}

/**
 * @brief Compare generated machine code against its golden file
 *
 * Running the suite with CREDENCE_BLESS set rewrites the golden from the
 * backend instead of comparing against it, through the same emit call the
 * comparison uses, so the two cannot disagree about what was generated.
 *
 * The .b source is what the runtime tests in compiled-test.sh compile and
 * execute, so a golden follows its source and never the other way around.
 * Review the diff afterwards, as a change means the machine code changed.
 */
inline void check_against_golden(std::string const& actual,
    fs::path const& expected_path)
{
    if (blessing_fixtures()) {
        fs::create_directories(expected_path.parent_path());
        std::ofstream out(expected_path, std::ios::binary);
        out << actual;
        MESSAGE("blessed " << expected_path.string());
        return;
    }
    REQUIRE(actual == read_file_from_path(expected_path.string()));
}

/**
 * @brief Parse a fixture source into the symbols and unit that emit takes
 *
 * The tests read the .b source rather than a stored tree, so the input of a
 * code generation test is the program itself and cannot drift from it.
 * Every fixture reaches the backend through here.
 */
struct Fixture
{
    credence::util::AST_Node symbols;
    credence::frontend::hir::Unit unit;
};

inline Fixture parse_platform_fixture(std::string_view name)
{
    auto source_path = fs::path(get_root_path())
                           .append("test/fixtures/platform")
                           .append(fmt::format("{}.b", name));
    auto source = read_file_from_path(source_path.string());

    auto program = credence::frontend::compile(source);
    auto symbols = credence::ir::hoisted_symbols(program.unit);

    return Fixture{ std::move(symbols), std::move(program.unit) };
}

#define SETUP_X86_64_FIXTURE_AND_TEST(path_name, os)                        \
    do {                                                                    \
        using namespace credence::target::x86_64;                           \
        auto test = std::ostringstream{};                                   \
        auto expected_root = get_root_path().append(                        \
            fmt::format("test/x86_64/expected/{}", os));                    \
        auto expected_path =                                                \
            fs::path(expected_root).append(fmt::format("{}.s", path_name)); \
        auto fixture = parse_platform_fixture(path_name);                   \
        credence::target::x86_64::emit(                                     \
            test, fixture.symbols, fixture.unit, true);                     \
        check_against_golden(test.str(), expected_path);                    \
    } while (0)

#define SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST(path_name, os, syscall)   \
    do {                                                                    \
        using namespace credence::target::x86_64;                           \
        auto test = std::ostringstream{};                                   \
        auto expected_root = get_root_path().append(                        \
            fmt::format("test/x86_64/expected/{}", os));                    \
        auto expected_path =                                                \
            fs::path(expected_root).append(fmt::format("{}.s", path_name)); \
        auto fixture = parse_platform_fixture(path_name);                   \
        credence::target::common::runtime::add_stdlib_functions_to_symbols( \
            fixture.symbols,                                                \
            credence::target::common::assembly::OS_Type::Linux,             \
            credence::target::common::assembly::Arch_Type::X8664,           \
            syscall);                                                       \
        credence::target::x86_64::emit(                                     \
            test, fixture.symbols, fixture.unit, false);                    \
        check_against_golden(test.str(), expected_path);                    \
    } while (0)

#define SETUP_X86_64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(               \
    path_name, os, syscall)                                                 \
    do {                                                                    \
        using namespace credence::target::x86_64;                           \
        auto test = std::ostringstream{};                                   \
        auto expected_root = get_root_path().append(                        \
            fmt::format("test/x86_64/expected/{}", os));                    \
        auto expected_path =                                                \
            fs::path(expected_root).append(fmt::format("{}.s", path_name)); \
        auto fixture = parse_platform_fixture(path_name);                   \
        credence::target::common::runtime::add_stdlib_functions_to_symbols( \
            fixture.symbols,                                                \
            credence::target::common::assembly::OS_Type::Linux,             \
            credence::target::common::assembly::Arch_Type::X8664,           \
            syscall);                                                       \
        credence::target::x86_64::emit(                                     \
            test, fixture.symbols, fixture.unit, true);                     \
        check_against_golden(test.str(), expected_path);                    \
    } while (0)

#define SETUP_X86_64_FIXTURE_SHOULD_THROW(path_name)      \
    do {                                                  \
        using namespace credence::target::x86_64;         \
        auto test = std::ostringstream{};                 \
        auto fixture = parse_platform_fixture(path_name); \
        REQUIRE_THROWS(credence::target::x86_64::emit(    \
            test, fixture.symbols, fixture.unit, true));  \
    } while (0)

TEST_CASE("target/x86_64: fixture: math_constant.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("math_constant", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("math_constant", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: math_constant_2.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("math_constant_2", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("math_constant_2", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: math_constant_4.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("math_constant_4", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("math_constant_4", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: math_constant_5.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("math_constant_5", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("math_constant_5", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: math_constant_6.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("math_constant_6", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("math_constant_6", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: math_constant_7.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("math_constant_7", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("math_constant_7", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: relation_constant.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("relation_constant", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("relation_constant", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: bitwise_constant_1.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("bitwise_constant_1", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("bitwise_constant_1", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: bitwise_2.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("bitwise_2", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("bitwise_2", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: bitwise_3.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("bitwise_3", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("bitwise_3", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: bitwise_4.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("bitwise_4", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("bitwise_4", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: pointers_1.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("pointers_1", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("pointers_1", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: pointers_2.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("pointers_2", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("pointers_2", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: pointers_3.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("pointers_3", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("pointers_3", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: pointers_4.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("pointers_4", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("pointers_4", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: strings")
{
    SETUP_X86_64_FIXTURE_SHOULD_THROW("string_2");
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("string_1", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("string_1", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: vector_1.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("vector_1", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("vector_1", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: vector_2.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("vector_2", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("vector_2", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: vector_3.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("vector_3", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("vector_3", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: globals")
{
    SETUP_X86_64_FIXTURE_SHOULD_THROW("globals_2");
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_FIXTURE_AND_TEST("globals_1", "linux");
#else
    SETUP_X86_64_FIXTURE_AND_TEST("globals_1", "bsd");
#endif
}

TEST_CASE("target/x86_64: fixture: syscall kernel write")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST("stdlib/write", "linux", true);
#else
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST("stdlib/write", "bsd", true);
#endif
}

TEST_CASE("target/x86_64: fixture: stdlib print")
{
    SETUP_X86_64_FIXTURE_SHOULD_THROW("stdlib/print_2");
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST("stdlib/print", "linux", true);
#else
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST("stdlib/print", "bsd", true);
#endif
}

TEST_CASE("target/x86_64: fixture: call_1.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "call_1", "linux", false);
#else
    SETUP_X86_64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "call_1", "bsd", false);
#endif
}

TEST_CASE("target/x86_64: fixture: call_2.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "call_2", "linux", false);
#else
    SETUP_X86_64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "call_2", "bsd", false);
#endif
}

TEST_CASE("target/x86_64: fixture: readme_2.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST("readme_2", "linux", false);
#else
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST("readme_2", "bsd", false);
#endif
}

TEST_CASE("target/x86_64: fixture: address_of_2.b")
{
    SETUP_X86_64_FIXTURE_SHOULD_THROW("address_of_1");
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "address_of_2", "linux", false);
#else
    SETUP_X86_64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "address_of_2", "bsd", false);
#endif
}

TEST_CASE("target/x86_64: fixture: string_3.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "string_3", "linux", false);
#else
    SETUP_X86_64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "string_3", "bsd", false);
#endif
}

TEST_CASE("target/x86_64: fixture: stdlib putchar")
{
    SETUP_X86_64_FIXTURE_SHOULD_THROW("stdlib/putchar_2");
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST(
        "stdlib/putchar_1", "linux", false);
#else
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST("stdlib/putchar_1", "bsd", false);
#endif
}

TEST_CASE("target/x86_64: fixture: relational/if_1.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "relational/if_1", "linux", false);
#else
    SETUP_X86_64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "relational/if_1", "bsd", false);
#endif
}

TEST_CASE("target/x86_64: fixture: relational/while_1.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "relational/while_1", "linux", false);
#else
    SETUP_X86_64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "relational/while_1", "bsd", false);
#endif
}

TEST_CASE("target/x86_64: fixture: relational/switch_1.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "relational/switch_1", "linux", false);
#else
    SETUP_X86_64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "relational/switch_1", "bsd", false);
#endif
}

TEST_CASE("target/x86_64: fixture: relational/if_2.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "relational/if_2", "linux", false);
#else
    SETUP_X86_64_WITH_STDLIB_NO_SYMBOLS_FIXTURE_AND_TEST(
        "relational/if_2", "bsd", false);
#endif
}

TEST_CASE("target/x86_64: fixture: stdlib/printf_1.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST(
        "stdlib/printf_1", "linux", false);
#else
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST("stdlib/printf_1", "bsd", false);
#endif
}

TEST_CASE("target/x86_64: fixture: argc_argv.b")
{
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST("argc_argv", "linux", false);
#else
    SETUP_X86_64_WITH_STDLIB_FIXTURE_AND_TEST("argc_argv", "bsd", false);
#endif
}