#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase

#include <credence/language/parser.h>       // for Parser
#include <credence/language/symbol_table.h> // for Symbol_Table_Builder

#include <credence/util.h> // for STRINGIFY, read_file_from_path
#include <easyjson.h>      // for JSON
#include <filesystem>      // for path
#include <fmt/format.h>    // for format

namespace fs = std::filesystem;

#define ROOT_PATH STRINGIFY(ROOT_TEST_PATH)

#define SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS(name)                          \
    do {                                                                    \
        auto fixture_path =                                                 \
            fs::path(ROOT_PATH).append("test/fixtures/language");           \
        auto source_path =                                                  \
            fs::path(fixture_path).append(fmt::format("{}.b", name));       \
        auto expected_path = fs::path(fixture_path)                         \
                                 .append("symbols")                         \
                                 .append(fmt::format("{}.json", name));     \
        auto source =                                                       \
            credence::util::read_file_from_path(source_path.string());      \
        auto ast = credence::language::Parser::parse(source);               \
        auto actual = credence::language::Symbol_Table_Builder::build(ast); \
        auto expected = easyjson::JSON::load_file(expected_path.string());  \
        CHECK(actual == expected);                                          \
    } while (0)

TEST_CASE("symbol_table.cc: function definition with no parameters")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("function_no_params");
}

TEST_CASE("symbol_table.cc: function definition with parameters, has a return")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("function_with_params");
}

TEST_CASE("symbol_table.cc: vector definition, sized with initial values")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("vector_sized_init");
}

TEST_CASE("symbol_table.cc: vector definition, sized with no initial values")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("vector_no_size");
}

TEST_CASE(
    "symbol_table.cc: vector definition, bare (no size, no initial values)")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("vector_bare");
}

TEST_CASE("symbol_table.cc: auto statement, including a local vector")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("auto_statement");
}

TEST_CASE("symbol_table.cc: extrn statement")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("extrn_statement");
}

TEST_CASE("symbol_table.cc: if/else statement")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("if_else");
}

TEST_CASE("symbol_table.cc: if statement with no else")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("if_no_else");
}

TEST_CASE("symbol_table.cc: while statement")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("while_statement");
}

TEST_CASE("symbol_table.cc: switch/case statement")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("switch_case");
}

TEST_CASE("symbol_table.cc: break statement")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("break_statement");
}

TEST_CASE("symbol_table.cc: goto and label statements")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("goto_label");
}

TEST_CASE("symbol_table.cc: label statement with whitespace before ':'")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("label_with_space");
}

TEST_CASE("symbol_table.cc: return statement with a value")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("return_value");
}

TEST_CASE("symbol_table.cc: chained binary operators")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("right_assoc_chain");
}

TEST_CASE("symbol_table.cc: mixed binary operators")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("mixed_operators");
}

TEST_CASE("symbol_table.cc: bare ternary expression")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("ternary_bare");
}

TEST_CASE("symbol_table.cc: ternary nested inside a relation_expression")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("ternary_relation_condition");
}

TEST_CASE("symbol_table.cc: nested (chained) ternary expressions")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("ternary_nested");
}

TEST_CASE("symbol_table.cc: chained assignment expression")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("chained_assignment");
}

TEST_CASE("symbol_table.cc: unary, pre/post inc-dec, and address-of forms")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("unary_forms");
}

TEST_CASE("symbol_table.cc: indirect lvalue has low effective precedence")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("indirect_lvalue_precedence");
}

TEST_CASE("symbol_table.cc: vector indexing refines an lvalue to vector_lvalue")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("vector_indexing");
}

TEST_CASE("symbol_table.cc: function calls, including nested calls and no args")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("function_calls");
}

TEST_CASE("symbol_table.cc: char literal of a single space keeps its quotes")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("char_literal_space");
}

TEST_CASE("symbol_table.cc: char literal strips its quotes in the general case")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("char_literal_normal");
}

TEST_CASE("symbol_table.cc: integer, float, and double literals")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("numeric_literals");
}

TEST_CASE("symbol_table.cc: string literal")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("string_literal");
}

TEST_CASE("symbol_table.cc: consecutive expression statements share one "
          "rvalue_statement")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("multiple_expr_statements");
}

TEST_CASE("symbol_table.cc: a bare ';' is an empty expression")
{
    SETUP_PARSE_FIXTURE_AND_TEST_SYMBOLS("bare_semicolon");
}
