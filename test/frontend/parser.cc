#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase

#include <credence/frontend/parser.h>    // for Parser
#include <credence/frontend/serialize.h> // for dump
#include <credence/util.h>               // for STRINGIFY, read_file_from_path
#include <filesystem>                    // for path, create_directories
#include <fmt/format.h>                  // for format
#include <fstream>                       // for ofstream
#include <sstream>                       // for ostringstream

namespace fs = std::filesystem;

#define ROOT_PATH STRINGIFY(ROOT_TEST_PATH)

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
 * @brief Parse a .b fixture and compare its dump against the golden file
 *
 * The expected output lives beside the sources in expected/ as readable
 * text and not JSON, so a failing diff shows the shape of the tree that
 * changed.
 *
 * Running the suite with CREDENCE_BLESS set rewrites the golden files from
 * the parser instead of comparing against them. Regeneration therefore
 * goes through the same dump call as the comparison, and the two cannot
 * disagree about how a tree is written out. Review the diff afterwards, as
 * a change means the shape of the parsed tree changed.
 */
#define SETUP_PARSE_FIXTURE_AND_TEST_AST(name)                                \
    do {                                                                      \
        auto fixture_path = get_root_path().append("test/fixtures/language"); \
        auto source_path =                                                    \
            fs::path(fixture_path).append(fmt::format("{}.b", name));         \
        auto expected_root = fs::path(fixture_path).append("expected");       \
        auto expected_path =                                                  \
            fs::path(expected_root).append(fmt::format("{}.ast", name));      \
        auto source =                                                         \
            credence::util::read_file_from_path(source_path.string());        \
        auto tree = credence::frontend::Parser::parse(source);                \
        auto actual = std::ostringstream{};                                   \
        credence::frontend::ast::dump(actual, tree);                          \
        if (blessing_fixtures()) {                                            \
            fs::create_directories(expected_root);                            \
            std::ofstream out(expected_path, std::ios::binary);               \
            out << actual.str();                                              \
            MESSAGE("blessed " << expected_path.string());                    \
        } else {                                                              \
            auto expected =                                                   \
                credence::util::read_file_from_path(expected_path.string());  \
            CHECK(actual.str() == expected);                                  \
        }                                                                     \
    } while (0)

TEST_CASE("frontend/parser.cc: function definition with no parameters")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("function_no_params");
}

TEST_CASE("frontend/parser.cc: function definition with parameters")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("function_with_params");
}

TEST_CASE("frontend/parser.cc: vector definition, sized with initial values")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("vector_sized_init");
}

TEST_CASE("frontend/parser.cc: vector definition, sized with no initial values")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("vector_no_size");
}

TEST_CASE("frontend/parser.cc: vector definition, bare (no size, no values)")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("vector_bare");
}

TEST_CASE("frontend/parser.cc: auto statement, including a local vector")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("auto_statement");
}

TEST_CASE("frontend/parser.cc: extrn statement")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("extrn_statement");
}

TEST_CASE("frontend/parser.cc: if and else statement")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("if_else");
}

TEST_CASE("frontend/parser.cc: if statement with no else")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("if_no_else");
}

TEST_CASE("frontend/parser.cc: while statement")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("while_statement");
}

TEST_CASE("frontend/parser.cc: switch and case statement")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("switch_case");
}

TEST_CASE("frontend/parser.cc: break statement")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("break_statement");
}

TEST_CASE("frontend/parser.cc: goto and label statements")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("goto_label");
}

TEST_CASE("frontend/parser.cc: label statement with whitespace before ':'")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("label_with_space");
}

TEST_CASE("frontend/parser.cc: return statement with a value")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("return_value");
}

TEST_CASE("frontend/parser.cc: chained binary operators nest to the right")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("right_assoc_chain");
}

TEST_CASE("frontend/parser.cc: mixed binary operators stay in source order")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("mixed_operators");
}

TEST_CASE("frontend/parser.cc: bare ternary expression")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("ternary_bare");
}

TEST_CASE("frontend/parser.cc: ternary nested in a binary right operand")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("ternary_relation_condition");
}

TEST_CASE("frontend/parser.cc: nested ternary expressions")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("ternary_nested");
}

TEST_CASE("frontend/parser.cc: chained assignment expression")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("chained_assignment");
}

TEST_CASE("frontend/parser.cc: unary, inc-dec, and address-of forms")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("unary_forms");
}

TEST_CASE("frontend/parser.cc: indirect lvalue has low effective precedence")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("indirect_lvalue_precedence");
}

TEST_CASE("frontend/parser.cc: vector indexing")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("vector_indexing");
}

TEST_CASE("frontend/parser.cc: function calls, nested and with no arguments")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("function_calls");
}

TEST_CASE("frontend/parser.cc: char literal of a single space keeps its quotes")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("char_literal_space");
}

TEST_CASE("frontend/parser.cc: char literal strips its quotes in the general "
          "case")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("char_literal_normal");
}

TEST_CASE("frontend/parser.cc: integer, float, and double literals")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("numeric_literals");
}

TEST_CASE("frontend/parser.cc: string literal")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("string_literal");
}

TEST_CASE("frontend/parser.cc: consecutive expression statements merge into "
          "one rvalue_statement")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("multiple_expr_statements");
}

TEST_CASE("frontend/parser.cc: a bare ';' is an empty expression")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("bare_semicolon");
}

/****************************************************************************
 *
 * Interning and arena invariants
 *
 * These hold for every tree the parser builds, so they are checked directly
 * and not through a golden file.
 *
 ****************************************************************************/

TEST_CASE("frontend/parser.cc: equal text interns to one handle")
{
    using namespace credence::frontend;

    auto tree = Parser::parse("main() {\n  auto count;\n"
                              "  count = count + count;\n}\n");

    // "main" and "count" are the only names, however often they appear
    CHECK(tree.strings.size() == 2);
    CHECK(tree.string_text.size() == 9);

    // the same name always yields the same handle
    ast::String_Index first = ast::null_string_index;
    std::size_t seen = 0;
    for (auto const& node : tree.nodes) {
        if (node.type != ast::Type::Identifier)
            continue;
        auto text = tree.string(node.data.string);
        if (text != "count")
            continue;
        if (first == ast::null_string_index)
            first = node.data.string;
        CHECK(node.data.string == first);
        ++seen;
    }
    CHECK(seen == 4);
}

TEST_CASE("frontend/parser.cc: children are appended before their parent")
{
    using namespace credence::frontend;

    auto tree =
        Parser::parse("main() {\n  auto x;\n  x = f(a + b, c * d);\n}\n");

    // a bottom-up pass over the array must never reach a parent first
    for (ast::Node_Index index = 0; index < tree.nodes.size(); ++index) {
        auto const& node = tree.nodes[index];
        switch (ast::payload_of(node.type)) {
            case ast::Payload::Span: {
                auto span = node.data.span;
                for (std::uint32_t i = 0; i < span.count; ++i) {
                    auto child = tree.extra[span.start + i];
                    if (child != ast::null_node_index)
                        CHECK(child < index);
                }
                break;
            }
            case ast::Payload::Binary:
                CHECK(node.data.binary.lhs < index);
                CHECK(node.data.binary.rhs < index);
                break;
            case ast::Payload::Unary:
                if (node.data.unary != ast::null_node_index)
                    CHECK(node.data.unary < index);
                break;
            default:
                break;
        }
    }
}

TEST_CASE("frontend/parser.cc: metadata is parallel to the node array")
{
    using namespace credence::frontend;

    auto tree = Parser::parse("main() {\n  auto x;\n  x = 1;\n}\n");
    CHECK(tree.nodes.size() == tree.metadata.size());
    CHECK(tree.root < tree.nodes.size());
}
