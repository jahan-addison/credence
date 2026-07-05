#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase

#include <credence/language/parser.h> // for Parser
#include <credence/util.h>            // for STRINGIFY, read_file_from_path
#include <easyjson.h>                 // for JSON
#include <filesystem>                 // for path
#include <fmt/format.h>               // for format

namespace fs = std::filesystem;

#define ROOT_PATH STRINGIFY(ROOT_TEST_PATH)

#define SETUP_PARSE_FIXTURE_AND_TEST_AST(name)                             \
    do {                                                                   \
        auto fixture_path =                                                \
            fs::path(ROOT_PATH).append("test/fixtures/language");          \
        auto source_path =                                                 \
            fs::path(fixture_path).append(fmt::format("{}.b", name));      \
        auto expected_path = fs::path(fixture_path)                        \
                                 .append("ast")                            \
                                 .append(fmt::format("{}.json", name));    \
        auto source =                                                      \
            credence::util::read_file_from_path(source_path.string());     \
        auto actual = credence::language::Parser::parse(source);           \
        auto expected = easyjson::JSON::load_file(expected_path.string()); \
        CHECK(actual == expected);                                         \
    } while (0)

TEST_CASE("parser.cc: function definition with no parameters")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("function_no_params");
}

TEST_CASE("parser.cc: function definition with parameters")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("function_with_params");
}

TEST_CASE("parser.cc: vector definition, sized with initial values")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("vector_sized_init");
}

TEST_CASE("parser.cc: vector definition, sized with no initial values")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("vector_no_size");
}

TEST_CASE("parser.cc: vector definition, bare (no size, no initial values)")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("vector_bare");
}

TEST_CASE("parser.cc: auto statement, including a local vector")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("auto_statement");
}

TEST_CASE("parser.cc: extrn statement")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("extrn_statement");
}

TEST_CASE("parser.cc: if/else statement")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("if_else");
}

TEST_CASE("parser.cc: if statement with no else")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("if_no_else");
}

TEST_CASE("parser.cc: while statement")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("while_statement");
}

TEST_CASE("parser.cc: switch/case statement")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("switch_case");
}

TEST_CASE("parser.cc: break statement")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("break_statement");
}

TEST_CASE("parser.cc: goto and label statements")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("goto_label");
}

TEST_CASE("parser.cc: label statement with whitespace before ':'")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("label_with_space");
}

TEST_CASE("parser.cc: return statement with a value")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("return_value");
}

TEST_CASE("parser.cc: chained binary operators are right-associative")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("right_assoc_chain");
}

TEST_CASE("parser.cc: mixed binary operators keep grammar's flat right-nesting")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("mixed_operators");
}

TEST_CASE("parser.cc: bare ternary expression")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("ternary_bare");
}

TEST_CASE(
    "parser.cc: ternary nested inside a relation_expression's right operand")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("ternary_relation_condition");
}

TEST_CASE("parser.cc: nested (chained) ternary expressions")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("ternary_nested");
}

TEST_CASE("parser.cc: chained assignment expression")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("chained_assignment");
}

TEST_CASE("parser.cc: unary, pre/post inc-dec, and address-of forms")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("unary_forms");
}

TEST_CASE("parser.cc: indirect lvalue has low effective precedence")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("indirect_lvalue_precedence");
}

TEST_CASE("parser.cc: vector indexing")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("vector_indexing");
}

TEST_CASE("parser.cc: function calls, including nested calls and no args")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("function_calls");
}

TEST_CASE("parser.cc: char literal of a single space keeps its quotes")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("char_literal_space");
}

TEST_CASE("parser.cc: char literal strips its quotes in the general case")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("char_literal_normal");
}

TEST_CASE("parser.cc: integer, float, and double literals")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("numeric_literals");
}

TEST_CASE("parser.cc: string literal")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("string_literal");
}

TEST_CASE("parser.cc: consecutive expression statements merge into one "
          "rvalue_statement")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("multiple_expr_statements");
}

TEST_CASE("parser.cc: a bare ';' is an empty expression")
{
    SETUP_PARSE_FIXTURE_AND_TEST_AST("bare_semicolon");
}
