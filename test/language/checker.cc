#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase

#include <credence/error.h>                   // for Credence_Exception
#include <credence/language/hir/checker.h>    // for Type_Checker, Frame
#include <credence/language/hir/table.h>      // for Table
#include <credence/language/parser.h>         // for Parser
#include <credence/language/symbol_builder.h> // for Symbol_Builder
#include <credence/util.h>                    // for AST_Node, contains
#include <easyjson.h>                         // for JSON
#include <string>                             // for basic_string, string
#include <string_view>                        // for string_view

struct Type_Checker_Fixture
{
    using Node = credence::util::AST_Node;
    using Table = credence::language::hir::Table;
    using Storage = credence::language::hir::checking::Storage;

    /**
     * @brief The semantic error of a B program, or empty if it has none
     */
    static inline std::string get_semantic_error(std::string const& source)
    {
        try {
            auto ast = credence::language::Parser::parse(source);
            auto symbols = credence::language::Symbol_Builder::build(ast);
            Table::build(symbols, ast);
        } catch (credence::detail::Credence_Exception const& e) {
            return e.what();
        }
        return {};
    }

    /**
     * @brief Check that a B program raises a semantic error with a message
     */
    static inline bool has_semantic_error(std::string const& source,
        std::string_view message)
    {
        return credence::util::contains(get_semantic_error(source), message);
    }

    /**
     * @brief Check that a B program has no semantic errors
     */
    static inline bool is_valid(std::string const& source)
    {
        return get_semantic_error(source).empty();
    }
};

TEST_CASE("is_compatible")
{
    using credence::language::hir::checking::Type_Checker;
    CHECK(Type_Checker::is_compatible("int", "int"));
    CHECK(Type_Checker::is_compatible("null", "double"));
    CHECK(Type_Checker::is_compatible("int", "null"));
    CHECK(Type_Checker::is_compatible("word", "int"));
    CHECK(Type_Checker::is_compatible("int", "word"));
    CHECK_FALSE(Type_Checker::is_compatible("int", "double"));
    CHECK_FALSE(Type_Checker::is_compatible("string", "int"));
}

TEST_CASE("is_numeric")
{
    using credence::language::hir::checking::Type_Checker;
    CHECK(Type_Checker::is_numeric("int"));
    CHECK(Type_Checker::is_numeric("double"));
    CHECK(Type_Checker::is_numeric("word"));
    CHECK_FALSE(Type_Checker::is_numeric("null"));
    CHECK_FALSE(Type_Checker::is_numeric("string"));
}

TEST_CASE_FIXTURE(Type_Checker_Fixture, "assign_to_scalar")
{
    CHECK(is_valid("main() { auto x, y; x = 5; y = x; }"));
    // a literal does not change the type of a scalar
    CHECK(has_semantic_error("main() { auto x; x = 10; x = 3.14; }",
        "right-hand-side '3.14' with type 'double' is not the same type "
        "(int)"));
    CHECK(has_semantic_error("main() { auto x, y; x = 5; y = 2.5; x = y; }",
        "right-hand-side 'y' with type 'double' is not the same type (int)"));
    CHECK(has_semantic_error("main() { auto x; x = \"hello\"; }",
        "left-hand-side 'x' is not a pointer"));
    CHECK(has_semantic_error("main() { auto x, *p, y; y = 1; p = &y; x = p; }",
        "left-hand-side 'x' is not a pointer"));
}

TEST_CASE_FIXTURE(Type_Checker_Fixture, "assign_to_scalar: word")
{
    // a binary expression or call result is a word, and a word agrees with
    // every type
    CHECK(is_valid(R"b(
main() {
  auto x, a, b, c;
  a = 1;
  b = 2;
  c = 3;
  x = a + b;
  x = c;
}
)b"));
    CHECK(is_valid(R"b(
main() {
  auto x, y;
  x = 1;
  y = (x + 1) * 2;
  x = y;
}
)b"));
    CHECK(is_valid("main() { auto x; x = f(); x = 5; } f() { return(1); }"));
}

TEST_CASE_FIXTURE(Type_Checker_Fixture, "from_assignment_expression")
{
    // a copy of a parameter is a word
    CHECK(is_valid("main() { } f(a) { auto b; b = a; }"));
    CHECK(is_valid("main() { } f(a) { a = a + 2; return(a); }"));
    // a parameter in an expression or a return is allowed
    CHECK(is_valid("main() { } f(a) { auto b; b = a + 1; return(b); }"));
    CHECK(is_valid("main() { } f(a) { return(a); }"));
    CHECK(is_valid("main() { auto x, y; x = y = 2; }"));
}

TEST_CASE_FIXTURE(Type_Checker_Fixture, "assign_to_pointer")
{
    CHECK(is_valid("main() { auto *p, *q, x; x = 1; p = &x; q = p; }"));
    CHECK(is_valid("main() { auto *p; p = \"hello\"; }"));
    CHECK(is_valid("main() { auto *p, *q; p = q; }"));
    CHECK(is_valid("main() { auto *p, x; x = 1; p = &x; p = p + 1; }"));
    CHECK(has_semantic_error("main() { auto *p, x; x = 1; p = x; }",
        "right-hand-side 'x' is not a pointer"));
    CHECK(has_semantic_error(
        "main() { auto *p; p = 1; }", "right-hand-side '1' is not a pointer"));
}

TEST_CASE_FIXTURE(Type_Checker_Fixture, "assign_to_pointer: pointer type")
{
    // once a pointer points to a type, it may only point to that type
    CHECK(has_semantic_error(
        "main() { auto *p, x, d; x = 1; d = 2.5; p = &x; p = &d; }",
        "'p' is a pointer to 'int', and '&d' is a pointer to 'double'"));
    CHECK(is_valid("main() { auto *p, x, y; x = 1; y = 2; p = &x; p = &y; }"));
    CHECK(has_semantic_error(
        "main() { auto *p, *q, d; d = 2.5; *p = 5; q = &d; p = q; }",
        "'p' is a pointer to 'int'"));
}

TEST_CASE_FIXTURE(Type_Checker_Fixture, "assign_to_dereference: null pointer")
{
    // an assignment through a null pointer sets the type it points to
    CHECK(is_valid("main() { auto *m, x; *m = 5; x = *m; }"));
    CHECK(has_semantic_error("main() { auto *m; *m = 5; *m = 2.5; }",
        "'*m' with type 'int' is not the same type (double)"));
    CHECK(has_semantic_error("main() { auto *m, d; d = 2.5; *m = 5; m = &d; }",
        "'m' is a pointer to 'int'"));
    // a dereference before the assignment is a null pointer dereference
    CHECK(has_semantic_error(
        "main() { auto *m, x; x = *m; *m = 5; }", "'m' is a null pointer"));
}

TEST_CASE_FIXTURE(Type_Checker_Fixture, "assign_to_dereference")
{
    CHECK(is_valid("main() { auto *p, x; x = 1; p = &x; *p = 2; x = *p; }"));
    CHECK(has_semantic_error("main() { auto *p, x; x = 1; p = &x; *p = 2.5; }",
        "is not the same type"));
    CHECK(has_semantic_error("main() { auto x; x = 1; *x = 2; }",
        "left-hand-side 'x' is not a pointer"));
    CHECK(has_semantic_error(
        "main() { auto *p, *q, x; x = 1; p = &x; q = p; *x = q; }",
        "is not a pointer"));
    // a store through a pointer parameter
    CHECK(is_valid("main() { } f(*b) { auto x; x = 2; *b = x; }"));
}

TEST_CASE_FIXTURE(Type_Checker_Fixture, "assign_to_vector")
{
    CHECK(is_valid("main() { extrn v; v[0] = 5; } v [3] 1, 2, 3;"));
    CHECK(is_valid("main() { auto v[3]; v[0] = 4; v[1] = v[0]; }"));
    CHECK(has_semantic_error(R"b(
main() {
  extrn nmbrs, strs;
  strs[1] = nmbrs[2];
}

nmbrs [3] 10, 20, 30;
strs [3] "hello", "there", "hi";
)b",
        "left-hand-side 'strs[1]' with type 'string' is not the same type"));
}

TEST_CASE_FIXTURE(Type_Checker_Fixture, "get_vector_at_offset")
{
    CHECK(has_semantic_error("main() { extrn v; v[3] = 1; } v [3] 1, 2, 3;",
        "invalid out-of-range vector access 'v' at index '3'"));
    CHECK(has_semantic_error(
        "main() { auto v[3]; v[1000] = 1; }", "is a buffer-overflow"));
    CHECK(has_semantic_error(
        "main() { auto x; x = v[1]; }", "identifier does not exist"));
    CHECK(has_semantic_error(
        "main() { auto v[3]; v[i] = 1; }", "invalid vector offset 'i'"));
    // the constant of a scalar offset is bounds checked
    CHECK(has_semantic_error(R"b(
main() {
  auto x, y;
  extrn unit, mess;
  x = unit;
  y = mess[x];
}

unit 5;
mess [3] "too bad", "tough luck", "that sucks";
)b",
        "at index '5', the value of 'x'"));
}

TEST_CASE_FIXTURE(Type_Checker_Fixture, "from_vector_offset")
{
    CHECK(is_valid("main() { auto x; extrn v; x = v[1]; } v [2] 1, 2;"));
    CHECK(has_semantic_error(
        "main() { auto x, v[3]; x = v[1]; }", "element at '1' does not exist"));
    // an offset not known at compile time is the type of the first element
    CHECK(is_valid(
        "main() { } f(i) { auto x; extrn v; x = v[i]; } v [2] \"a\", \"b\";"));
    CHECK(is_valid(R"b(
main() {
  auto i, x, v[3];
  i = 0;
  while (i < 3) {
    v[i] = i;
    i = i + 1;
  }
  x = v[2];
}
)b"));
}

TEST_CASE_FIXTURE(Type_Checker_Fixture, "from_increment_expression")
{
    CHECK(is_valid("main() { auto x; x = 1; x++; }"));
    CHECK(is_valid("main() { auto x, y; x = 1; y = x + 1; y--; }"));
    CHECK(has_semantic_error("main() { auto x; x++; }",
        "'x' with type 'null' is not a numeric type"));
    // a pointer increment keeps the pointer
    CHECK(is_valid("main() { auto *p, x; x = 1; p = &x; p++; }"));
}

TEST_CASE_FIXTURE(Type_Checker_Fixture, "from_address_of_expression")
{
    CHECK(is_valid("main() { auto *p; extrn v; p = &v[1]; } v [2] 1, 2;"));
    CHECK(is_valid("main() { auto *p, *q; q = \"x\"; p = &q; }"));
    CHECK(has_semantic_error(
        "main() { auto *p; extrn v; p = &v[1]; } v [2] \"a\", \"b\";",
        "'&v[1]' is a pointer to a string pointer"));
}

TEST_CASE_FIXTURE(Type_Checker_Fixture, "from_dereference")
{
    CHECK(is_valid(R"b(
main() {
  auto *a, b, *c;
  b = 100;
  c = &b;
  a = c;
  *a = 10;
  *c = *a;
}
)b"));
    CHECK(has_semantic_error(
        "main() { auto *p, x; x = *p; }", "'p' is a null pointer"));
    CHECK(has_semantic_error("main() { auto *p, x, y; p = &x; y = *p; }",
        "'p' points to an uninitialized object"));
    CHECK(has_semantic_error(
        "main() { auto x, y; x = 1; y = *x; }", "'x' is not a pointer"));
    CHECK(is_valid("main() { } f(*a) { auto b; b = *a; }"));
}

TEST_CASE_FIXTURE(Type_Checker_Fixture, "from_unary_expression")
{
    // the parser nests "*p + 1" as "*(p + 1)", the dereference applies to p
    CHECK(is_valid("main() { auto x, *p; x = 1; p = &x; x = *p + 1; }"));
    CHECK(has_semantic_error(
        "main() { auto x, p; x = *p + 1; }", "'p' is not a pointer"));
    CHECK(is_valid("main() { auto x; x = 1; x = -x; x = ~x; x = !x; }"));
}
