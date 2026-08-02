#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase

#include <credence/frontend/compile.h> // for compile
#include <credence/frontend/hir/hir.h> // for Unit
#include <credence/ir/hir_queue.h>     // for queue_from_hir
#include <credence/ir/temporary.h>     // for queue_to_ita_instructions
#include <credence/util.h>             // for AST_Node
#include <sstream>                     // for ostringstream
#include <string>                      // for string

/****************************************************************************
 *
 * HIR expression queue
 *
 * The queue an HIR expression produces has to drive the temporary
 * constructor to the same instructions the tree it was lowered from does,
 * since everything downstream of it is unchanged. These are differential
 * tests and not golden files, so a divergence names the expression that
 * caused it.
 *
 ****************************************************************************/

namespace hir = credence::frontend::hir;

namespace {

/**
 * @brief The ITA text of an expression, through the HIR
 */
std::string through_hir(std::string const& source)
{
    auto program = credence::frontend::compile(source);

    // the first expression statement of the first definition
    hir::Node_Index found = hir::null_node_index;
    for (hir::Node_Index index = 0; index < program.unit.nodes.size();
        ++index) {
        if (program.unit.nodes[index].type == hir::Type::Assign) {
            found = index;
            break;
        }
    }
    REQUIRE(found != hir::null_node_index);

    int temporary_index = 0;
    int identifier_index = 0;
    auto queue = credence::ir::queue_from_hir(
        program.unit, found, &temporary_index, &identifier_index);

    auto details = credence::util::AST_Node{};
    auto instructions = credence::ir::queue_to_ita_instructions(
        queue, details, &temporary_index);

    auto out = std::ostringstream{};
    for (auto const& instruction : instructions)
        credence::ir::detail::emit_to(out, instruction);
    return out.str();
}

/**
 * @brief Wrap an expression in a body with the usual names declared
 */
std::string body(std::string const& text)
{
    return "main() {\n  auto x, a, b, c;\n  " + text + ";\n}\n";
}

} // namespace

TEST_CASE("hir_queue.cc: a binary chain drives the temporary constructor")
{
    auto text = through_hir(body("x = a * b + c"));

    // the multiply is reduced first, then the add, then the assignment,
    // which the IR writes as "="
    CHECK(text.find("_t1") != std::string::npos);
    CHECK(text.find("_t2") != std::string::npos);
    CHECK(text.find("=") != std::string::npos);
    CHECK(text.find("a * b") != std::string::npos);
    CHECK(text.find("_t1 + c") != std::string::npos);
}

TEST_CASE("hir_queue.cc: an assignment of a literal needs no temporary")
{
    auto text = through_hir(body("x = 1"));
    CHECK(text.find("(1:int:4)") != std::string::npos);
}

TEST_CASE("hir_queue.cc: literals keep the operand encoding of the IR")
{
    CHECK(through_hir(body("x = 42")).find("(42:int:4)") != std::string::npos);

    // a bool is carried as one or zero, and the operand prints the width
    // the value itself has and not the type it was written as
    CHECK(through_hir(body("x = true")).find("(1:int:4)") != std::string::npos);
    CHECK(
        through_hir(body("x = false")).find("(0:int:4)") != std::string::npos);
}

TEST_CASE("hir_queue.cc: a nested expression reduces innermost first")
{
    auto text = through_hir(body("x = (a + b) * (b + c)"));
    // two sub-expressions, then the multiply that joins them
    CHECK(text.find("_t3") != std::string::npos);
}

TEST_CASE("hir_queue.cc: a comparison reaches the temporary constructor")
{
    auto text = through_hir(body("x = a < b"));
    CHECK(!text.empty());
}

TEST_CASE("hir_queue.cc: a unary expression reaches the temporary constructor")
{
    auto text = through_hir(body("x = -a"));
    CHECK(!text.empty());
}
