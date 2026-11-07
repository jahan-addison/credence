#include <doctest/doctest.h> // for ResultBuilder, REQUIRE, TestCase

#include <credence/error.h>                   // for Credence_Exception
#include <credence/language/hir/table.h>      // for Table
#include <credence/language/parser.h>         // for Parser
#include <credence/language/symbol_builder.h> // for Symbol_Builder
#include <credence/util.h>                    // for STRINGIFY, contains
#include <easyjson.h>                         // for JSON
#include <filesystem>                         // for path
#include <format>                             // for format
#include <string>                             // for basic_string, string
#include <string_view>                        // for string_view

namespace fs = std::filesystem;

#define ROOT_PATH STRINGIFY(ROOT_TEST_PATH)

inline std::filesystem::path get_root_path()
{
    if (const char* env_root = std::getenv("CREDENCE_TEST_ROOT"))
        return fs::path(env_root);
    return fs::path(ROOT_PATH);
}

struct HIR_Table_Fixture
{
    using Node = credence::util::AST_Node;
    using Table = credence::language::hir::Table;

    /**
     * @brief Run semantic analysis on a B program
     */
    static inline Table make_table(std::string const& source, Node& ast)
    {
        ast = credence::language::Parser::parse(source);
        auto symbols = credence::language::Symbol_Builder::build(ast);
        auto table = Table{ symbols };
        table.build_from_definitions(ast);
        return table;
    }

    /**
     * @brief The semantic error of a B program, or empty if it has none
     */
    static inline std::string get_semantic_error(std::string const& source)
    {
        try {
            Node ast{};
            make_table(source, ast);
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
};

TEST_CASE_FIXTURE(HIR_Table_Fixture, "Type Checking")
{
    // clang-format off
    // Type checking test cases from test/fixtures/types
    // each index corresponds to an ast in the fixtures
    const auto statuses = {
        false, false, false, false,
        false, false,  true,  false,
        false, true,  false, false,
        false, false, false, true,
        false, true, true, true,
        true, true
    };
    // clang-format on
    auto type_fixtures_path = get_root_path();
    type_fixtures_path.append("test/fixtures/types/ast");
    for (std::size_t i = 1; i <= statuses.size(); i++) {
        auto* status = statuses.begin() + i - 1;
        auto file_path =
            fs::path(type_fixtures_path).append(std::format("{}.json", i));
        auto path_contents =
            easyjson::JSON::load_file(file_path.string()).to_deque();
        if (*status) {
            try {
                Table::build(path_contents[0], path_contents[1]);
            } catch (...) {
                FAIL(std::format(
                    "FAIL: Expected '{}.json' to NOT throw exception", i));
            }
        } else
            try {
                Table::build(path_contents[0], path_contents[1]);
                FAIL(std::format(
                    "FAIL: Expected '{}.json' to throw exception", i));
            } catch (...) {
                // expected
            }
    }
}

TEST_CASE_FIXTURE(HIR_Table_Fixture, "build_from_definitions")
{
    Node ast{};
    auto table = make_table(R"b(
main() {
  extrn unit;
  auto x, arr[3];
  x = unit;
}

add(a, *b) {
  return(a);
}

unit 10;
)b",
        ast);
    auto objects = table.objects_;
    REQUIRE(objects->functions.contains("main"));
    REQUIRE(objects->functions.contains("add"));
    REQUIRE(objects->labels.contains("add"));
    auto& parameters = objects->functions.at("add").get().parameters;
    REQUIRE(parameters.size() == 2);
    REQUIRE(parameters[0] == "a");
    REQUIRE(parameters[1] == "*b");
    REQUIRE(objects->vectors.at("arr").get().size == 3);
    REQUIRE(objects->vectors.at("unit").get().data["0"] ==
            credence::type::Data_Type{ "10", "int", 4UL });
    REQUIRE(objects->globals.is_defined("unit"));
}

TEST_CASE_FIXTURE(HIR_Table_Fixture, "build_from_vector_definition")
{
    CHECK(has_semantic_error(
        "main() { } v [2] 1, 2, 3;", "invalid vector definition"));
    Node ast{};
    auto table = make_table("main() { } v [2] 1, 2.5;", ast);
    REQUIRE(table.vectors_.at("v").size == 2);
    REQUIRE(table.vectors_.at("v").data.at("0").type == "int");
    REQUIRE(table.vectors_.at("v").data.at("1").type == "double");
}

TEST_CASE_FIXTURE(HIR_Table_Fixture, "build_from_function_definition")
{
    CHECK(has_semantic_error(
        "main() { } main() { }", "function name already exists"));
    Node ast{};
    auto table = make_table("main() { } f(a, *b, c[2]) { }", ast);
    auto& frame = table.frames_.at("f");
    REQUIRE(frame.parameters.size() == 3);
    REQUIRE(frame.locals.at("a").type == "word");
    REQUIRE(frame.locals.at("b").storage ==
            credence::language::hir::checking::Storage::POINTER);
    REQUIRE(frame.locals.at("c").storage ==
            credence::language::hir::checking::Storage::POINTER);
}

TEST_CASE_FIXTURE(HIR_Table_Fixture, "build_from_auto_statement")
{
    CHECK(has_semantic_error(
        "main() { auto x, x; }", "already defined in auto declaration"));
    CHECK(
        has_semantic_error("main() { auto v[1000]; }", "is a stack overflow"));
    Node ast{};
    auto table = make_table("main() { auto x, *p, v[3]; }", ast);
    auto& frame = table.frames_.at("main");
    REQUIRE(frame.locals.at("x").type == "null");
    REQUIRE(frame.locals.at("p").storage ==
            credence::language::hir::checking::Storage::POINTER);
    REQUIRE(frame.vectors.at("v").size == 3);
}

TEST_CASE_FIXTURE(HIR_Table_Fixture, "build_from_extrn_statement")
{
    CHECK(has_semantic_error(
        "main() { extrn v; }", "symbol not defined in global scope"));
    CHECK(get_semantic_error("main() { extrn v; } v 1;").empty());
}

TEST_CASE_FIXTURE(HIR_Table_Fixture, "build_from_label_statement")
{
    CHECK(has_semantic_error(
        "main() { auto x; a: x = 1; a: x = 2; }", "label is already defined"));
    CHECK(get_semantic_error(
        "main() { auto x; a: x = 1; } f() { auto x; a: x = 1; }")
            .empty());
}

TEST_CASE_FIXTURE(HIR_Table_Fixture, "build_from_goto_statement")
{
    CHECK(has_semantic_error("main() { goto b; }", "label does not exist"));
    CHECK(get_semantic_error("main() { auto x; a: x = 1; goto a; }").empty());
}

TEST_CASE_FIXTURE(HIR_Table_Fixture, "build_from_return_statement")
{
    // a function may return from more than one statement
    CHECK(get_semantic_error(R"b(
main() {
  auto x;
  x = 1;
  if (x) return(1);
  return(2);
}
)b")
            .empty());
    CHECK(get_semantic_error(R"b(
exp(x, y) {
  if (x == 1 || y == 1)
    return(x * y);
  return(exp(x - 1, y - 1));
}

main() { }
)b")
            .empty());
    // but each return value must be the same type
    CHECK(has_semantic_error(R"b(
main() {
  auto x;
  x = 1;
  if (x) return(1);
  return(2.5);
}
)b",
        "is not the same type as a previous return"));
}

TEST_CASE_FIXTURE(HIR_Table_Fixture, "build_from_switch_statement")
{
    CHECK(has_semantic_error(R"b(
main() {
  auto x, *p;
  x = 1;
  switch (x) {
    case 1:
      p = x;
      break;
  }
}
)b",
        "right-hand-side 'x' is not a pointer"));
}

TEST_CASE_FIXTURE(HIR_Table_Fixture, "build_from_while_statement")
{
    CHECK(has_semantic_error(R"b(
main() {
  auto x, y;
  x = 1;
  while (x < 3) {
    y = "s";
  }
}
)b",
        "left-hand-side 'y' is not a pointer"));
}

TEST_CASE_FIXTURE(HIR_Table_Fixture,
    "set_pointer_parameters_from_call_arguments")
{
    using credence::language::hir::checking::Storage;
    Node ast{};
    auto table = make_table(R"b(
main(argc, argv) {
  auto *m, x;
  m = "pointer";
  x = 5;
  show(argv[1]);
  show(m);
  count(x);
}
show(s) {
  print2(s);
}
print2(t) {
  return(t);
}
count(n) {
  return(n);
}
)b",
        ast);
    auto& functions = table.objects_->functions;
    REQUIRE(functions.at("show").get().parameters.front() == "*s");
    REQUIRE(functions.at("print2").get().parameters.front() == "*t");
    REQUIRE(functions.at("count").get().parameters.front() == "n");
    REQUIRE(
        table.frames_.at("show").locals.at("s").storage == Storage::POINTER);
    REQUIRE(
        table.frames_.at("count").locals.at("n").storage == Storage::SCALAR);
}
