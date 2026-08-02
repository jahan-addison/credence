#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase

#include <credence/frontend/compile.h> // for compile
#include <credence/frontend/hir/hir.h> // for Unit
#include <credence/ir/symbols.h>       // for hoisted_symbols
#include <credence/util.h>             // for AST_Node
#include <filesystem>                  // for path
#include <fmt/format.h>                // for format
#include <string>                      // for string

namespace fs = std::filesystem;
namespace hir = credence::frontend::hir;

#define ROOT_PATH STRINGIFY(ROOT_TEST_PATH)

/****************************************************************************
 *
 * Hoisted symbol table
 *
 * The object table and the backends read this while placing storage, so
 * what matters is the shape each name is given and not the order the
 * table is built in.
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
 * @brief The hoisted symbol table of a source string
 */
credence::util::AST_Node symbols_of(std::string const& source)
{
    auto program = credence::frontend::compile(source);
    return credence::ir::hoisted_symbols(program.unit);
}

/**
 * @brief The shape a name was given, or empty when it is absent
 */
std::string shape_of(credence::util::AST_Node& table, std::string const& name)
{
    if (!table.has_key(name))
        return {};
    return table[name]["type"].to_string();
}

} // namespace

TEST_CASE("symbols.cc: a function definition keeps its shape")
{
    auto table = symbols_of("main() {\n  auto x;\n  x = 1;\n}\n");
    CHECK(shape_of(table, "main") == "function_definition");
    CHECK(shape_of(table, "x") == "lvalue");
}

TEST_CASE("symbols.cc: a pointer is told apart from a scalar")
{
    auto table = symbols_of("main() {\n  auto x, *y;\n  y = &x;\n}\n");
    CHECK(shape_of(table, "x") == "lvalue");
    CHECK(shape_of(table, "y") == "indirect_lvalue");
}

TEST_CASE("symbols.cc: a vector at file scope carries its size")
{
    auto table = symbols_of("main() {\n  auto x;\n  x = 1;\n}\n"
                            "mess [2] \"a\", \"b\";\n");
    CHECK(shape_of(table, "mess") == "vector_definition");
    CHECK(table["mess"]["size"].to_int() == 2);
}

TEST_CASE("symbols.cc: a vector inside a function carries its size")
{
    auto table = symbols_of("main() {\n  auto v[3];\n  v[0] = 1;\n}\n");
    CHECK(shape_of(table, "v") == "vector_lvalue");
    CHECK(table["v"]["size"].to_int() == 3);
}

TEST_CASE("symbols.cc: a parameter is a declared name")
{
    auto table = symbols_of("add(a, b) {\n  return(a + b);\n}\n");
    CHECK(shape_of(table, "add") == "function_definition");
    CHECK(shape_of(table, "a") == "lvalue");
    CHECK(shape_of(table, "b") == "lvalue");
}

TEST_CASE("symbols.cc: a pointer parameter is told apart")
{
    auto table = symbols_of("identity(*y) {\n  return(y);\n}\n");
    CHECK(shape_of(table, "y") == "indirect_lvalue");
}

TEST_CASE("symbols.cc: an extrn vector keeps the shape of its definition")
{
    // "extrn mess" names the vector defined below, so the table has to
    // carry the size the object table places storage from
    auto table = symbols_of("main() {\n  auto x;\n  extrn mess;\n"
                            "  x = mess[1];\n}\n"
                            "mess [2] \"a\", \"b\";\n");
    CHECK(shape_of(table, "mess") == "vector_definition");
    CHECK(table["mess"]["size"].to_int() == 2);
}

TEST_CASE("symbols.cc: a label is a declared name")
{
    auto table = symbols_of("main() {\n  auto x;\n  x = 1;\n"
                            "done:\n  x = 2;\n}\n");
    CHECK(shape_of(table, "done") == "label");
}

TEST_CASE("symbols.cc: every fixture builds a table")
{
    // the shapes the object table places storage from must exist for every
    // program the code generation tests cover
    static constexpr std::string_view names[] = { "math_constant",
        "pointers_1",
        "vector_1",
        "globals_1",
        "stdlib/write",
        "relational/if_1",
        "argc_argv" };

    for (auto name : names) {
        auto path = fs::path(root_path())
                        .append("test/fixtures/platform")
                        .append(fmt::format("{}.b", name));
        auto source = credence::util::read_file_from_path(path.string());
        auto table = symbols_of(source);
        CHECK(!table.dump_keys().empty());
    }
}
