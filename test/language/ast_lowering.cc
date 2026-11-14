#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase

#include <credence/language/ast_lowering.h> // for AST_Lowering
#include <credence/language/hir/hir.h>      // for Data_Kind
#include <credence/language/literal.h>      // for Literal, NULL_LITERAL, lit...
#include <credence/symbol.h>                // for Symbol_Table
#include <deque>                            // for deque
#include <easyjson.h>                       // for JSON
#include <map>                              // for map
#include <memory>                           // for shared_ptr
#include <string>                           // for basic_string, string
#include <test/fixture.h>                   // for load_json_fixture
#include <tuple>                            // for get, tie
#include <utility>                          // for pair, make_pair, get
#include <variant>                          // for get, monostate

using namespace credence;
using namespace credence::language::type;

struct Fixture
{
    credence::util::AST_Node lvalue_ast_node_json;
    credence::util::AST_Node assignment_symbol_table;

    Fixture()
    {
        assignment_symbol_table = credence::test::load_json_fixture(
            "language/lowering/assignment_symbols");
        lvalue_ast_node_json =
            credence::test::load_json_fixture("language/lowering/lvalue");
    }
    ~Fixture() = default;
};

TEST_CASE("ast_lowering.cc: AST_Lowering::lower_from_node")
{
    credence::util::AST_Node obj;
    using std::get;
    obj["test"] =
        credence::test::load_json_fixture("language/lowering/lower_from_node");
    auto temp = language::AST_Lowering(obj);
    credence::language::literal::Literal null = language::literal::NULL_LITERAL;

    temp.symbols_.table_.emplace("x", null);
    temp.symbols_.table_.emplace("c", null);
    temp.symbols_.table_.emplace("putchar", null);
    temp.symbols_.table_.emplace("getchar", null);
    // check all
    for (auto& rvalue : obj["test"].array_range()) {
        CHECK_NOTHROW(temp.lower_from_node(rvalue));
    }
}

TEST_CASE("ast_lowering.cc: AST_Lowering::function_expression")
{
    credence::util::AST_Node obj;
    using std::get;
    obj["test"] = credence::test::load_json_fixture(
        "language/lowering/function_expression");
    auto temp = language::AST_Lowering(obj);
    credence::language::literal::Literal null = language::literal::NULL_LITERAL;

    temp.symbols_.table_.emplace("x", null);
    temp.symbols_.table_.emplace("y", null);
    temp.symbols_.table_.emplace("putchar", null);
    temp.symbols_.table_.emplace("z", null);

    auto test1 = temp.from_function_expression_node(obj["test"]);
    auto function =
        get<credence::language::hir::Data_Kind::Function>(test1.value);
    // parameters
    CHECK(get<credence::language::hir::Data_Kind::LValue>(
              function.second[0]->value)
              .first == "x");
    CHECK(get<credence::language::hir::Data_Kind::LValue>(
              function.second[1]->value)
              .first == "y");
    CHECK(get<credence::language::hir::Data_Kind::LValue>(
              function.second[2]->value)
              .first == "z");
}

TEST_CASE("ast_lowering.cc: AST_Lowering::evaluated_expression")
{
    credence::util::AST_Node obj;
    using std::get;
    obj["test"] = credence::test::load_json_fixture(
        "language/lowering/evaluated_expression");
    auto temp = language::AST_Lowering(obj);
    credence::language::literal::Literal null = language::literal::NULL_LITERAL;

    temp.symbols_.table_.emplace("x", null);

    auto expressions = obj["test"].to_deque();
    auto test1 = temp.from_evaluated_expression_node(expressions.at(0));
    auto expr1 = get<credence::language::hir::Data_Kind::Pointer>(test1.value);
    CHECK(get<credence::language::hir::Data_Kind::Relation>(
              get<credence::language::hir::Data_Kind::Pointer>(expr1->value)
                  ->value)
              .first == Operator::B_MUL);
    auto test2 = temp.from_evaluated_expression_node(expressions.at(1));
    auto expr2 = get<credence::language::hir::Data_Kind::Pointer>(test2.value);
    CHECK(get<credence::language::hir::Data_Kind::LValue>(expr2->value).first ==
          "*x");
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_relation_expression")
{
    credence::util::AST_Node obj;
    obj["test"] = credence::test::load_json_fixture(
        "language/lowering/relation_expression");
    auto temp = language::AST_Lowering(obj);
    credence::language::literal::Literal null = language::literal::NULL_LITERAL;

    std::vector<credence::language::hir::Data_Kind::Pointer> arguments{};
    temp.symbols_.table_.emplace("x", null);

    auto relation_expressions = obj["test"].to_deque();

    // forgive me
    using std::get;

    auto test1 = temp.from_relation_expression_node(relation_expressions.at(0));
    CHECK(std::get<credence::language::hir::Data_Kind::Relation>(test1.value)
              .first == Operator::B_MUL);
    arguments =
        std::get<credence::language::hir::Data_Kind::Relation>(test1.value)
            .second;
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(
              arguments[0]->value)
              .first == "x");
    CHECK(std::get<int>(std::get<credence::language::literal::Literal>(
              arguments[1]->value)
                  .first) == 10);

    // ternary relation test
    auto test2 = temp.from_relation_expression_node(relation_expressions.at(1));
    CHECK(std::get<credence::language::hir::Data_Kind::Relation>(test2.value)
              .first == Operator::R_LE);
    arguments = std::move(
        std::get<credence::language::hir::Data_Kind::Relation>(test2.value)
            .second);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(
              arguments[0]->value)
              .first == "x");
    CHECK(
        get<int>(get<credence::language::literal::Literal>(arguments[1]->value)
                .first) == 5);
    CHECK(
        get<int>(get<credence::language::literal::Literal>(arguments[2]->value)
                .first) == 10);
    CHECK(
        get<int>(get<credence::language::literal::Literal>(arguments[3]->value)
                .first) == 1);

    auto test3 = temp.from_relation_expression_node(relation_expressions.at(2));
    CHECK(std::get<credence::language::hir::Data_Kind::Relation>(test3.value)
              .first == Operator::R_EQUAL);
    arguments = std::move(
        std::get<credence::language::hir::Data_Kind::Relation>(test3.value)
            .second);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(
              arguments[0]->value)
              .first == "x");
    CHECK(std::get<int>(std::get<credence::language::literal::Literal>(
              arguments[1]->value)
                  .first) == 5);

    auto test4 = temp.from_relation_expression_node(relation_expressions.at(3));
    CHECK(std::get<credence::language::hir::Data_Kind::Relation>(test4.value)
              .first == Operator::R_NEQUAL);
    arguments = std::move(
        std::get<credence::language::hir::Data_Kind::Relation>(test4.value)
            .second);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(
              arguments[0]->value)
              .first == "x");
    CHECK(std::get<int>(std::get<credence::language::literal::Literal>(
              arguments[1]->value)
                  .first) == 5);

    auto test5 = temp.from_relation_expression_node(relation_expressions.at(4));
    CHECK(std::get<credence::language::hir::Data_Kind::Relation>(test5.value)
              .first == Operator::XOR);
    arguments = std::move(
        std::get<credence::language::hir::Data_Kind::Relation>(test5.value)
            .second);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(
              arguments[0]->value)
              .first == "x");
    CHECK(std::get<int>(std::get<credence::language::literal::Literal>(
              arguments[1]->value)
                  .first) == 0);

    auto test6 = temp.from_relation_expression_node(relation_expressions.at(5));
    CHECK(std::get<credence::language::hir::Data_Kind::Relation>(test6.value)
              .first == Operator::R_LT);
    arguments = std::move(
        std::get<credence::language::hir::Data_Kind::Relation>(test6.value)
            .second);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(
              arguments[0]->value)
              .first == "x");
    CHECK(std::get<int>(std::get<credence::language::literal::Literal>(
              arguments[1]->value)
                  .first) == 5);

    auto test7 = temp.from_relation_expression_node(relation_expressions.at(6));
    CHECK(std::get<credence::language::hir::Data_Kind::Relation>(test7.value)
              .first == Operator::R_LE);
    arguments = std::move(
        std::get<credence::language::hir::Data_Kind::Relation>(test7.value)
            .second);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(
              arguments[0]->value)
              .first == "x");
    CHECK(std::get<int>(std::get<credence::language::literal::Literal>(
              arguments[1]->value)
                  .first) == 10);
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_unary_expression")
{
    credence::util::AST_Node obj;
    obj["test"] =
        credence::test::load_json_fixture("language/lowering/unary_expression");
    auto temp = language::AST_Lowering(obj);

    credence::language::literal::Literal null = language::literal::NULL_LITERAL;

    temp.symbols_.table_.emplace("x", null);

    auto unary_expressions = obj["test"].to_deque();
    std::shared_ptr<credence::language::hir::Data_Kind> lvalue, constant;

    auto test1 = temp.from_unary_expression_node(unary_expressions.at(0));
    lvalue =
        std::get<credence::language::hir::Data_Kind::Unary>(test1.value).second;
    CHECK(std::get<credence::language::hir::Data_Kind::Unary>(test1.value)
              .first == Operator::POST_INC);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(lvalue->value)
              .first == "x");

    auto test2 = temp.from_unary_expression_node(unary_expressions.at(1));
    lvalue =
        std::get<credence::language::hir::Data_Kind::Unary>(test2.value).second;
    CHECK(std::get<credence::language::hir::Data_Kind::Unary>(test2.value)
              .first == Operator::PRE_INC);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(lvalue->value)
              .first == "x");

    auto test3 = temp.from_unary_expression_node(unary_expressions.at(2));
    lvalue =
        std::get<credence::language::hir::Data_Kind::Unary>(test3.value).second;
    CHECK(std::get<credence::language::hir::Data_Kind::Unary>(test3.value)
              .first == Operator::U_ADDR_OF);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(lvalue->value)
              .first == "x");

    auto test4 = temp.from_unary_expression_node(unary_expressions.at(3));
    constant =
        std::get<credence::language::hir::Data_Kind::Unary>(test4.value).second;
    CHECK(std::get<credence::language::hir::Data_Kind::Unary>(test4.value)
              .first == Operator::U_ONES_COMPLEMENT);
    CHECK(std::get<int>(
              std::get<credence::language::literal::Literal>(constant->value)
                  .first) == 5);

    auto test5 = temp.from_unary_expression_node(unary_expressions.at(4));
    lvalue =
        std::get<credence::language::hir::Data_Kind::Unary>(test5.value).second;
    CHECK(std::get<credence::language::hir::Data_Kind::Unary>(test5.value)
              .first == Operator::U_INDIRECTION);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(lvalue->value)
              .first == "x");

    auto test6 = temp.from_unary_expression_node(unary_expressions.at(5));
    constant =
        std::get<credence::language::hir::Data_Kind::Unary>(test6.value).second;
    CHECK(std::get<credence::language::hir::Data_Kind::Unary>(test6.value)
              .first == Operator::U_MINUS);
    CHECK(std::get<int>(
              std::get<credence::language::literal::Literal>(constant->value)
                  .first) == 5);

    auto test7 = temp.from_unary_expression_node(unary_expressions.at(6));
    lvalue =
        std::get<credence::language::hir::Data_Kind::Unary>(test7.value).second;
    CHECK(std::get<credence::language::hir::Data_Kind::Unary>(test7.value)
              .first == Operator::U_NOT);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(lvalue->value)
              .first == "x");
}

TEST_CASE_FIXTURE(Fixture,
    "ast_lowering.cc: AST_Lowering::from_assignment_expression")
{
    credence::util::AST_Node obj;
    obj["symbols"] = assignment_symbol_table;
    obj["test"] = credence::test::load_json_fixture(
        "language/lowering/assignment_expression");
    auto temp = language::AST_Lowering(obj["symbols"]);
    // no declaration with `auto' or `extern', should throw
    CHECK_THROWS(temp.from_assignment_expression_node(obj["test"]));

    credence::language::literal::Literal value_type =
        language::literal::NULL_LITERAL;

    credence::language::literal::Literal assigned_type = { 5,
        language::literal::TYPE_LITERAL.at("int") };

    temp.symbols_.table_.emplace("x", value_type);

    auto expr = temp.from_assignment_expression_node(obj["test"]);

    auto lhs =
        std::get<credence::language::hir::Data_Kind::Symbol>(expr.value).first;
    auto* rhs =
        &std::get<credence::language::hir::Data_Kind::Symbol>(expr.value)
             .second;

    CHECK(lhs.first == "x");
    CHECK(lhs.second == value_type);
    CHECK(std::get<credence::language::literal::Literal>((*rhs)->value) ==
          assigned_type);
}

TEST_CASE_FIXTURE(Fixture, "ast_lowering.cc: AST_Lowering::is_symbol")
{
    credence::util::AST_Node obj;
    obj["symbols"] = assignment_symbol_table;
    obj["test"] =
        credence::util::AST_Node::load(R"({"node": "lvalue", "root": "x"})");
    auto temp = language::AST_Lowering(obj["test"]);
    // not declared with `auto' or `extern', should throw
    CHECK(temp.is_symbol(obj["test"]) == false);

    auto temp2 = language::AST_Lowering(obj["symbols"]);
    CHECK(temp2.is_symbol(obj["test"]) == false);

    credence::language::literal::Literal value_type =
        language::literal::NULL_LITERAL;
    temp2.symbols_.set_symbol_by_name("x", value_type);
    CHECK(temp2.is_symbol(obj["test"]) == true);
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_lvalue_expression")
{
    credence::util::AST_Node obj;
    obj["test"] = credence::test::load_json_fixture(
        "language/lowering/lvalue_expression");
    auto temp = language::AST_Lowering(obj);
    credence::language::literal::Literal empty_value =
        std::make_pair('0', std::make_pair("byte", 50));
    auto lvalues = obj["test"].to_deque();
    auto [vector, pointer, normal] =
        std::tie(lvalues.at(0), lvalues.at(1), lvalues.at(2));
    temp.symbols_.table_["x"] = empty_value;
    temp.symbols_.table_["y"] = empty_value;
    temp.symbols_.table_["z"] = empty_value;
    // test vector
    auto test1 = temp.from_lvalue_expression_node(vector);
    CHECK(test1.first == "x[50]");
    // test pointer
    auto test2 = temp.from_lvalue_expression_node(pointer);
    CHECK(test2.first == "*y");
    // normal variable
    auto test3 = temp.from_lvalue_expression_node(normal);
    CHECK(test3.first == "z");
    CHECK(test3.second == empty_value);
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_indirect_identifier")
{
    credence::util::AST_Node obj;
    obj["test"] = credence::test::load_json_fixture(
        "language/lowering/indirect_identifier");

    auto temp = language::AST_Lowering(obj["test"]);
    CHECK_THROWS(temp.from_indirect_identifier_node(obj["test"]));
    credence::language::literal::Literal value =
        std::make_pair('0', std::make_pair("byte", 50));
    temp.symbols_.table_["x"] = value;
    auto test = temp.from_indirect_identifier_node(obj["test"]);
    CHECK(test == value);
}
TEST_CASE("ast_lowering.cc: AST_Lowering::from_vector_idenfitier")
{
    credence::util::AST_Node obj;
    obj["test"] = credence::test::load_json_fixture(
        "language/lowering/vector_identifier");

    auto temp = language::AST_Lowering(obj["test"]);
    CHECK_THROWS(temp.from_vector_idenfitier_node(obj["test"]));
    credence::language::literal::Literal test =
        std::make_pair('0', std::make_pair("byte", 50));
    temp.symbols_.table_["x"] = test;
    CHECK(temp.from_vector_idenfitier_node(obj["test"]) == test);
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_constant_expression")
{
    credence::util::AST_Node obj;
    obj["test"] = credence::util::AST_Node::load(
        R"({"node": "integer_literal", "root": 10})");

    auto temp = language::AST_Lowering(obj);
    auto data = temp.from_constant_expression_node(obj["test"]);
    auto [value, type] = data;
    CHECK(std::get<int>(value) == 10);
    CHECK(type.first == "int");
    CHECK(type.second == sizeof(int));
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_integer_literal")
{
    credence::util::AST_Node obj;
    obj["test"] = credence::util::AST_Node::load(
        R"({"node": "integer_literal", "root": 10})");

    auto temp = language::AST_Lowering(obj);
    auto data = temp.from_integer_literal_node(obj["test"]);
    auto [value, type] = data;
    CHECK(std::get<int>(value) == 10);
    CHECK(type.first == "int");
    CHECK(type.second == sizeof(int));
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_float_literal")
{
    credence::util::AST_Node obj;
    obj["test"] = credence::util::AST_Node::load(
        R"({"node": "float_literal", "root": 10.0002})");

    auto temp = language::AST_Lowering(obj);
    auto data = temp.from_float_literal_node(obj["test"]);
    auto [value, type] = data;
    CHECK(std::get<float>(value) == 10.0002f);
    CHECK(type.first == "float");
    CHECK(type.second == sizeof(float));
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_double_literal")
{
    credence::util::AST_Node obj;
    obj["test"] = credence::util::AST_Node::load(
        R"({"node": "double_literal", "root": 10.05})");

    auto temp = language::AST_Lowering(obj);
    auto data = temp.from_double_literal_node(obj["test"]);
    auto [value, type] = data;
    CHECK(std::get<double>(value) == 10.05);
    CHECK(type.first == "double");
    CHECK(type.second == sizeof(double));
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_string_literal")
{
    credence::util::AST_Node obj;
    obj["test"] =
        credence::test::load_json_fixture("language/lowering/string_literal");

    auto temp = language::AST_Lowering(obj);
    auto data = temp.from_string_literal_node(obj["test"]);
    auto [value, type] = data;
    CHECK(std::get<std::string>(value) == "hello world");
    CHECK(type.first == "string");
    CHECK(type.second == std::string{ "hello world" }.size());

    obj["escaped"] = credence::test::load_json_fixture(
        "language/lowering/escaped_string_literal");
    auto [escaped_value, escaped_type] =
        temp.from_string_literal_node(obj["escaped"]);
    CHECK(std::get<std::string>(escaped_value) == "hello\\n");
    CHECK(escaped_type.second == 6UL);
}

TEST_CASE("literal_to_string")
{
    auto escaped = language::literal::Literal{
        std::string{ "hello\\n" },
        { "string", 6UL }
    };
    CHECK(language::literal::literal_to_string(escaped) ==
          "(\"hello\\n\":string:6)");
    auto plain = language::literal::Literal{
        std::string{ "hello" },
        { "string", 5UL }
    };
    CHECK(
        language::literal::literal_to_string(plain) == "(\"hello\":string:5)");
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_constant_literal")
{
    credence::util::AST_Node obj;
    obj["test"] = credence::util::AST_Node::load(
        R"({"node": "constant_literal", "root": "x"})");

    auto temp = language::AST_Lowering(obj);
    auto data = temp.from_constant_literal_node(obj["test"]);
    auto [value, type] = data;
    CHECK(std::get<char>(value) == 'x');
    CHECK(type.first == "char");
    CHECK(type.second == sizeof(char));
}