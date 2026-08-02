#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase

#include <credence/frontend/compile.h>       // for compile
#include <credence/frontend/hir/hir.h>       // for Unit
#include <credence/frontend/hir/serialize.h> // for dump_linear
#include <sstream>                           // for ostringstream
#include <string>                            // for string

/****************************************************************************
 *
 * HIR lowering and type checking
 *
 * Lowering is checked by the shape it produces and not by a golden
 * file, since the pass is a small set of structural rules that are clearer
 * as assertions than as an outline.
 *
 ****************************************************************************/

namespace hir = credence::frontend::hir;

namespace {

/**
 * @brief Parse, lower, and check a whole program
 */
struct Lowered
{
    hir::Unit unit;
    std::vector<hir::Diagnostic> diagnostics;

    explicit Lowered(std::string source)
    {
        auto program = credence::frontend::compile(std::move(source));
        unit = std::move(program.unit);
        diagnostics = std::move(program.diagnostics);
    }

    /**
     * @brief The linear form of the first definition
     */
    std::string linear() const
    {
        auto out = std::ostringstream{};
        for (auto definition : unit.definitions)
            hir::dump_linear(out, unit, definition);
        return out.str();
    }

    bool contains(std::string_view text) const
    {
        return linear().find(text) != std::string::npos;
    }
};

/**
 * @brief Wrap an expression in a function body with the usual names declared
 */
Lowered expression(std::string const& text)
{
    return Lowered{ "main() {\n  auto x, a, b, c;\n  x = " + text + ";\n}\n" };
}

} // namespace

TEST_CASE("hir.cc: a mixed chain is regrouped by precedence")
{
    // the parser emits Binary(*, a, Binary(+, b, c)), which groups by
    // source order and not by what binds tighter
    auto lowered = expression("a * b + c");
    CHECK(lowered.diagnostics.empty());

    // (a * b) + c, so the multiply is reduced before the add
    CHECK(lowered.contains(
        "symbol_ref:a symbol_ref:b binary(*) symbol_ref:c binary(+)"));
}

TEST_CASE("hir.cc: a chain already in precedence order is left alone")
{
    auto lowered = expression("a + b * c");
    CHECK(lowered.diagnostics.empty());
    CHECK(lowered.contains(
        "symbol_ref:a symbol_ref:b symbol_ref:c binary(*) binary(+)"));
}

TEST_CASE("hir.cc: parentheses survive lowering")
{
    auto lowered = expression("a * (b + c)");
    CHECK(lowered.diagnostics.empty());
    CHECK(lowered.contains(
        "symbol_ref:a symbol_ref:b symbol_ref:c binary(+) binary(*)"));
}

TEST_CASE("hir.cc: equal precedence groups from the left")
{
    auto lowered = expression("a - b - c");
    CHECK(lowered.diagnostics.empty());
    CHECK(lowered.contains(
        "symbol_ref:a symbol_ref:b binary(-) symbol_ref:c binary(-)"));
}

TEST_CASE("hir.cc: a comparison binds looser than arithmetic")
{
    auto lowered = expression("a + b == c");
    CHECK(lowered.diagnostics.empty());
    CHECK(lowered.contains(
        "symbol_ref:a symbol_ref:b binary(+) symbol_ref:c binary(==)"));
}

TEST_CASE("hir.cc: a comparison yields a bool")
{
    auto lowered = expression("a == b");
    for (hir::Node_Index index = 0; index < lowered.unit.nodes.size();
        ++index) {
        if (lowered.unit.nodes[index].type == hir::Type::Binary)
            CHECK(lowered.unit.types[index] == hir::type_bool);
    }
}

TEST_CASE("hir.cc: a subtree is a contiguous range ending at its node")
{
    Lowered lowered{ "main() {\n  auto x, a, b, c;\n"
                     "  x = a * b + c;\n}\n" };

    for (hir::Node_Index index = 0; index < lowered.unit.nodes.size();
        ++index) {
        auto range = lowered.unit.subtree(index);
        CHECK(range.start <= index);
        CHECK(range.start + range.count == index + 1);
    }
}

TEST_CASE("hir.cc: parentheses are dropped")
{
    // an evaluated expression holds nothing once precedence is resolved,
    // so "(a)" lowers to exactly what "a" would have
    Lowered parenthesized{ "main() {\n  auto x, a;\n  x = (a);\n}\n" };
    Lowered bare{ "main() {\n  auto x, a;\n  x = a;\n}\n" };

    CHECK(parenthesized.diagnostics.empty());
    CHECK(parenthesized.linear() == bare.linear());
}

TEST_CASE("hir.cc: a name is bound to one symbol wherever it appears")
{
    Lowered lowered{ "main() {\n  auto count;\n"
                     "  count = count + count;\n}\n" };
    CHECK(lowered.diagnostics.empty());

    hir::Symbol_Index first = hir::null_symbol_index;
    std::size_t seen = 0;
    for (auto const& node : lowered.unit.nodes) {
        if (node.type != hir::Type::Symbol_Ref)
            continue;
        if (first == hir::null_symbol_index)
            first = node.data.symbol;
        CHECK(node.data.symbol == first);
        ++seen;
    }
    CHECK(seen == 3);
}

TEST_CASE("hir.cc: a parameter is visible in the body")
{
    Lowered lowered{ "add(a, b) {\n  return(a + b);\n}\n" };
    CHECK(lowered.diagnostics.empty());
}

TEST_CASE("hir.cc: an undeclared name is reported")
{
    Lowered lowered{ "main() {\n  x = 1;\n}\n" };
    REQUIRE(lowered.diagnostics.size() >= 1);
    CHECK(lowered.diagnostics[0].message.find("was not declared") !=
          std::string::npos);
}

TEST_CASE("hir.cc: a constant subscript outside a vector is reported")
{
    Lowered lowered{ "main() {\n  auto v[3];\n  v[7] = 1;\n}\n" };
    REQUIRE(lowered.diagnostics.size() >= 1);
    CHECK(lowered.diagnostics[0].message.find("outside") != std::string::npos);
}

TEST_CASE("hir.cc: a subscript within a vector is accepted")
{
    Lowered lowered{ "main() {\n  auto v[3];\n  v[2] = 1;\n}\n" };
    CHECK(lowered.diagnostics.empty());
}

TEST_CASE("hir.cc: subscripting a word is allowed")
{
    // B has no separate pointer type, so a word holds an address as
    // readily as a number and may be subscripted
    Lowered lowered{ "main() {\n  auto x;\n  x[0] = 1;\n}\n" };
    CHECK(lowered.diagnostics.empty());
}

TEST_CASE("hir.cc: a call may name a function this unit never defines")
{
    // the standard library and other objects are resolved by the linker
    Lowered lowered{ "main() {\n  auto x;\n  x = 1;\n  printf(x);\n}\n" };
    CHECK(lowered.diagnostics.empty());
}

TEST_CASE("hir.cc: a call may name a function defined further down")
{
    Lowered lowered{ "main() {\n  auto x;\n  x = later();\n}\n"
                     "later() {\n  return(1);\n}\n" };
    CHECK(lowered.diagnostics.empty());
}

TEST_CASE("hir.cc: a pointer declaration is typed as a pointer")
{
    Lowered lowered{ "main() {\n  auto x, *y;\n  y = &x;\n}\n" };
    CHECK(lowered.diagnostics.empty());

    bool seen = false;
    for (auto const& symbol : lowered.unit.symbol_table.symbols()) {
        if (lowered.unit.string(symbol.name) != "y")
            continue;
        CHECK(lowered.unit.type_table.kind_of(symbol.type) ==
              hir::Type_Kind::Pointer);
        seen = true;
    }
    CHECK(seen);
}

TEST_CASE("hir.cc: a goto with no matching label is reported")
{
    Lowered lowered{ "main() {\n  goto nowhere;\n}\n" };
    REQUIRE(lowered.diagnostics.size() >= 1);
    CHECK(lowered.diagnostics[0].message.find("never defined") !=
          std::string::npos);
}

TEST_CASE("hir.cc: a goto reaching forward to its label is accepted")
{
    Lowered lowered{ "main() {\n  auto x;\n  goto done;\n"
                     "  x = 1;\ndone:\n  x = 2;\n}\n" };
    CHECK(lowered.diagnostics.empty());
}

TEST_CASE("hir.cc: redeclaring a name in one scope is reported")
{
    Lowered lowered{ "main() {\n  auto x;\n  auto x;\n}\n" };
    REQUIRE(lowered.diagnostics.size() >= 1);
    CHECK(lowered.diagnostics[0].message.find("already declared") !=
          std::string::npos);
}

TEST_CASE("hir.cc: every node is given a type")
{
    Lowered lowered{ "main() {\n  auto x, v[2];\n"
                     "  x = 1 + 2;\n  v[0] = x;\n}\n" };
    CHECK(lowered.diagnostics.empty());

    for (auto type : lowered.unit.types)
        CHECK(type != hir::null_type_index);
}

TEST_CASE("hir.cc: the type table interns a derived type once")
{
    hir::Type_Table types{};
    auto first = types.pointer_to(hir::type_int);
    auto second = types.pointer_to(hir::type_int);
    CHECK(first == second);

    auto other = types.pointer_to(hir::type_char);
    CHECK(other != first);
}

TEST_CASE("hir.cc: a closed scope keeps its symbols addressable")
{
    hir::Symbol_Table symbols{};
    symbols.push_scope();
    auto inner = symbols.declare(0, hir::type_int, hir::Storage::Auto);
    symbols.pop_scope();

    // the handle still names the same declaration after the scope closed
    CHECK(symbols.at(inner).type == hir::type_int);
    // but the name is no longer visible
    CHECK(symbols.lookup(0) == hir::null_symbol_index);
}

/****************************************************************************
 *
 * Address resolution
 *
 * What an expression addresses is resolved in the frontend, so that no
 * later stage recovers it from the name of a symbol.
 *
 ****************************************************************************/

TEST_CASE("hir.cc: a constant subscript is folded to a byte offset")
{
    Lowered lowered{ "main() {\n  auto v[4];\n  v[2] = 1;\n}\n" };
    CHECK(lowered.diagnostics.empty());

    bool seen = false;
    for (hir::Node_Index index = 0; index < lowered.unit.nodes.size();
        ++index) {
        if (lowered.unit.nodes[index].type != hir::Type::Subscript)
            continue;
        seen = true;
        // element 2 of a word vector, so two words in
        auto width = lowered.unit.type_table.size_of(hir::type_word);
        CHECK(lowered.unit.offsets[index] == 2 * width);
    }
    CHECK(seen);
}

TEST_CASE("hir.cc: a variable subscript has no constant offset")
{
    Lowered lowered{ "main() {\n  auto v[4], i;\n  i = 1;\n  v[i] = 1;\n}\n" };

    for (hir::Node_Index index = 0; index < lowered.unit.nodes.size();
        ++index) {
        if (lowered.unit.nodes[index].type != hir::Type::Subscript)
            continue;
        CHECK(lowered.unit.offsets[index] == hir::unknown_offset);
    }
}

TEST_CASE("hir.cc: a vector used as a value decays to a pointer")
{
    Lowered lowered{ "main() {\n  auto v[4], p;\n  p = v;\n}\n" };

    // the declaration keeps its vector type
    bool checked = false;
    for (auto const& symbol : lowered.unit.symbol_table.symbols()) {
        if (symbol.storage != hir::Storage::Vector)
            continue;
        CHECK(lowered.unit.type_table.kind_of(symbol.type) ==
              hir::Type_Kind::Vector);
        checked = true;
    }
    CHECK(checked);

    // the use is typed as a pointer instead
    bool decayed = false;
    for (hir::Node_Index index = 0; index < lowered.unit.nodes.size();
        ++index) {
        auto const& node = lowered.unit.nodes[index];
        if (node.type != hir::Type::Symbol_Ref)
            continue;
        if (lowered.unit.symbol_table.at(node.data.symbol).storage !=
            hir::Storage::Vector)
            continue;
        CHECK(lowered.unit.type_table.kind_of(lowered.unit.types[index]) ==
              hir::Type_Kind::Pointer);
        decayed = true;
    }
    CHECK(decayed);
}

TEST_CASE("hir.cc: a subscript base is not decayed")
{
    Lowered lowered{ "main() {\n  auto v[4];\n  v[1] = 1;\n}\n" };

    for (hir::Node_Index index = 0; index < lowered.unit.nodes.size();
        ++index) {
        auto const& node = lowered.unit.nodes[index];
        if (node.type != hir::Type::Subscript)
            continue;
        auto base = node.data.binary.lhs;
        // the base addresses the vector itself, so it keeps its type
        CHECK(lowered.unit.type_table.kind_of(lowered.unit.types[base]) ==
              hir::Type_Kind::Vector);
    }
}

TEST_CASE("hir.cc: string literals are collected")
{
    Lowered lowered{ "main() {\n  auto a, b;\n"
                     "  a = \"one\";\n  b = \"two\";\n}\n" };
    CHECK(lowered.unit.string_literals.size() == 2);
}

TEST_CASE("hir.cc: the address of a character inside a string is reported")
{
    Lowered lowered{ "main() {\n  auto s, p;\n"
                     "  s = \"hello\";\n  p = &s[1];\n}\n" };
    bool reported = false;
    for (auto const& diagnostic : lowered.diagnostics) {
        if (diagnostic.message.find("already a pointer") != std::string::npos)
            reported = true;
    }
    CHECK(reported);
}
