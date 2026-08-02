#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase

#include <credence/error.h>                   // for Credence_Exception
#include <credence/frontend/compile.h>        // for compile
#include <credence/frontend/hir/hir.h>        // for Unit
#include <credence/ir/symbols.h>              // for hoisted_symbols
#include <credence/target/x86_64/generator.h> // for emit
#include <credence/util.h>                    // for read_file_from_path
#include <filesystem>                         // for path, directory_iterator
#include <sstream>                            // for ostringstream
#include <string>                             // for string

namespace fs = std::filesystem;

#define ROOT_PATH STRINGIFY(ROOT_TEST_PATH)

/****************************************************************************
 *
 * Type fixtures
 *
 * Every program in test/fixtures/types declares the outcome it expects in
 * its own text, as "// should pass" or "// should fail". The fixture is
 * the whole test, so covering a new rule is a matter of writing the
 * program and saying which way it goes.
 *
 * A rejected program has to be rejected by a diagnostic. An assertion that
 * escapes as a compile error is a defect in the compiler and not a report
 * about the program, so the two outcomes are told apart here.
 *
 ****************************************************************************/

namespace {

inline std::filesystem::path root_path()
{
    if (const char* env_root = std::getenv("CREDENCE_TEST_ROOT"))
        return fs::path(env_root);
    return fs::path(ROOT_PATH);
}

/**
 * @brief What the compiler did with a program
 */
enum class Outcome
{
    // compiled with nothing to report
    Accepted,
    // rejected by a diagnostic, which is what a bad program earns
    Rejected,
    // an internal assertion escaped, which is always a defect
    Assertion
};

/**
 * @brief The outcome a fixture declares in its own text
 */
std::string declared_outcome(std::string const& source)
{
    if (source.find("// should pass") != std::string::npos)
        return "pass";
    if (source.find("// should fail") != std::string::npos)
        return "fail";
    return {};
}

/**
 * @brief Take a program as far as x86_64 assembly and report what happened
 */
Outcome compile_outcome(std::string const& source)
{
    try {
        auto program = credence::frontend::compile(source);
        if (program.failed())
            return Outcome::Rejected;
        auto symbols = credence::ir::hoisted_symbols(program.unit);
        auto out = std::ostringstream{};
        credence::target::x86_64::emit(out, symbols, program.unit, true);
    } catch (credence::detail::Credence_Exception const& error) {
        auto what = std::string{ error.what() };
        if (what.find("Assertion") != std::string::npos)
            return Outcome::Assertion;
        return Outcome::Rejected;
    }
    return Outcome::Accepted;
}

/**
 * @brief Every fixture in test/fixtures/types, in name order
 */
std::vector<fs::path> type_fixtures()
{
    auto directory = fs::path(root_path()).append("test/fixtures/types");
    auto paths = std::vector<fs::path>{};
    for (auto const& entry : fs::directory_iterator(directory)) {
        if (entry.path().extension() == ".b")
            paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

} // namespace

TEST_CASE("types: every fixture declares the outcome it expects")
{
    for (auto const& path : type_fixtures()) {
        auto source = credence::util::read_file_from_path(path.string());
        INFO("fixture: ", path.filename().string());
        CHECK(!declared_outcome(source).empty());
    }
}

TEST_CASE("types: every fixture gets the outcome it declares")
{
    for (auto const& path : type_fixtures()) {
        auto source = credence::util::read_file_from_path(path.string());
        auto declared = declared_outcome(source);
        if (declared.empty())
            continue;

        INFO("fixture: ", path.filename().string());
        auto outcome = compile_outcome(source);
        CHECK(outcome ==
              (declared == "pass" ? Outcome::Accepted : Outcome::Rejected));
    }
}

TEST_CASE("types: no fixture reaches an internal assertion")
{
    // a program the compiler turns down has to be turned down by a
    // diagnostic naming what is wrong with it
    for (auto const& path : type_fixtures()) {
        auto source = credence::util::read_file_from_path(path.string());
        INFO("fixture: ", path.filename().string());
        CHECK(compile_outcome(source) != Outcome::Assertion);
    }
}
