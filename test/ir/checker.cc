#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase

#include <credence/frontend/compile.h>        // for compile
#include <credence/frontend/hir/hir.h>        // for Unit
#include <credence/ir/symbols.h>              // for hoisted_symbols
#include <credence/ir/table.h>                // for emit
#include <credence/target/x86_64/generator.h> // for emit
#include <credence/util.h>                    // for AST_Node
#include <sstream>                            // for ostringstream
#include <string>                             // for string

/****************************************************************************
 *
 * Object table and type checker
 *
 * The table places storage from the ITA and the checker rejects what a
 * declaration does not allow. Both read a name through the stack frame,
 * where a pointer lives in the address table and a scalar lives in the
 * value table, so a name reached through the wrong one asserts instead of
 * reporting a type error.
 *
 * These programs are all legal B. What each one covers is a path that
 * reached storage by a route the frame had no entry for.
 *
 ****************************************************************************/

namespace {

/**
 * @brief Take a source string as far as the object table
 */
void through_table(std::string const& source)
{
    auto program = credence::frontend::compile(source);
    REQUIRE(program.diagnostics.empty());
    auto symbols = credence::ir::hoisted_symbols(program.unit);
    auto out = std::ostringstream{};
    credence::ir::emit(out, symbols, program.unit);
}

/**
 * @brief Take a source string all the way to x86_64 assembly
 */
void through_backend(std::string const& source)
{
    auto program = credence::frontend::compile(source);
    REQUIRE(program.diagnostics.empty());
    auto symbols = credence::ir::hoisted_symbols(program.unit);
    auto out = std::ostringstream{};
    credence::target::x86_64::emit(out, symbols, program.unit, true);
}

} // namespace

TEST_CASE("checker.cc: a pointer takes the address of a string definition")
{
    // "str" is a definition of one string, so the object table holds it as
    // a vector of one element. A pointer assigned that name is a pointer to
    // the string, which the pointer path allows
    through_table("main() {\n  auto *y;\n  extrn str;\n  y = str;\n}\n"
                  "str \"puts\";\n");
}

TEST_CASE("checker.cc: a pointer to a string definition reaches the backend")
{
    through_backend("main() {\n  auto *y, z;\n  extrn str;\n  y = str;\n"
                    "  z = *y;\n}\n"
                    "str \"puts\";\n");
}

TEST_CASE("checker.cc: a vector reached by name addresses its first element")
{
    // a bare name carries no subscript, so the offset of the storage it
    // addresses is zero and resolving it must not walk back into the name
    through_table("main() {\n  auto *y;\n  extrn mess;\n  y = mess;\n}\n"
                  "mess [3] \"a\", \"b\", \"c\";\n");
}

TEST_CASE("checker.cc: a vector is subscripted by a parameter")
{
    // the index is not known until the call, so the storage the subscript
    // addresses is a word and no entry for the name exists in the frame
    through_backend("snide(errno) {\n  auto t;\n  extrn mess;\n"
                    "  t = mess[errno];\n}\n"
                    "mess [3] \"a\", \"b\", \"c\";\n");
}

TEST_CASE("checker.cc: a vector is subscripted by a local")
{
    through_backend("main() {\n  auto t, i;\n  extrn mess;\n  i = 1;\n"
                    "  t = mess[i];\n}\n"
                    "mess [3] \"a\", \"b\", \"c\";\n");
}

TEST_CASE("checker.cc: a pointer to a scalar is still rejected")
{
    // the pointer path has to keep reporting a type error where the
    // right-hand-side addresses nothing
    auto source = std::string{ "main() {\n  auto *y, z;\n  y = z;\n}\n" };
    CHECK_THROWS(through_table(source));
}

TEST_CASE("checker.cc: a pointer assigned a literal is still rejected")
{
    auto source = std::string{ "main() {\n  auto *y;\n  y = 1;\n}\n" };
    CHECK_THROWS(through_table(source));
}
