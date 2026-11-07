#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase

#include <credence/target/common/assembly.h> // for get_result_from_trivia...
#include <credence/target/common/types.h>    // for Immediate
#include <credence/types.h> // for get_value_from_rvalue_data_type, from_rva...
#include <vector>           // for vector

using Immediate = credence::target::common::Immediate;

namespace {

/**
 * @brief The value of an immediate as a string, e.g. "1"
 */
inline std::string get_value(Immediate const& immediate)
{
    return std::string{ credence::type::get_value_from_rvalue_data_type(
        immediate) };
}

} // namespace

TEST_CASE("get_result_from_trivial_integral_expression")
{
    using credence::target::common::assembly::
        get_result_from_trivial_integral_expression;
    auto ten = Immediate{ "10", "int", 4UL };
    auto three = Immediate{ "3", "int", 4UL };
    CHECK(get_value(get_result_from_trivial_integral_expression(
              ten, "+", three)) == "13");
    CHECK(get_value(get_result_from_trivial_integral_expression(
              ten, "-", three)) == "7");
    CHECK(get_value(get_result_from_trivial_integral_expression(
              ten, "*", three)) == "30");
    CHECK(get_value(get_result_from_trivial_integral_expression(
              ten, "/", three)) == "3");
    CHECK(get_value(get_result_from_trivial_integral_expression(
              ten, "%", three)) == "1");
    CHECK(get_value(get_result_from_trivial_integral_expression(
              Immediate{ "10", "long", 8UL },
              "%",
              Immediate{ "4", "long", 8UL })) == "2");
}

TEST_CASE("get_result_from_trivial_relational_expression")
{
    using credence::target::common::assembly::
        get_result_from_trivial_relational_expression;
    auto one = Immediate{ "1", "int", 4UL };
    auto two = Immediate{ "2", "int", 4UL };
    auto zero = Immediate{ "0", "int", 4UL };
    CHECK(get_value(get_result_from_trivial_relational_expression(
              one, "<", two)) == "1");
    CHECK(get_value(get_result_from_trivial_relational_expression(
              one, ">=", two)) == "0");
    CHECK(get_value(get_result_from_trivial_relational_expression(
              zero, "||", one)) == "1");
    CHECK(get_value(get_result_from_trivial_relational_expression(
              zero, "&&", one)) == "0");

    auto hello = Immediate{ "\"hello\"", "string", 5UL };
    auto world = Immediate{ "\"world\"", "string", 5UL };
    CHECK(get_value(get_result_from_trivial_relational_expression(
              hello, "==", hello)) == "1");
    CHECK(get_value(get_result_from_trivial_relational_expression(
              hello, "!=", hello)) == "0");
    CHECK(get_value(get_result_from_trivial_relational_expression(
              hello, "<", world)) == "1");
}

TEST_CASE("get_result_from_trivial_bitwise_expression")
{
    using credence::target::common::assembly::
        get_result_from_trivial_bitwise_expression;
    auto five = Immediate{ "5", "int", 4UL };
    auto three = Immediate{ "3", "int", 4UL };
    CHECK(get_value(get_result_from_trivial_bitwise_expression(
              five, "&", three)) == "1");
    CHECK(get_value(get_result_from_trivial_bitwise_expression(
              five, "|", three)) == "7");
    CHECK(get_value(get_result_from_trivial_bitwise_expression(
              five, "^", three)) == "6");
    CHECK(get_value(get_result_from_trivial_bitwise_expression(
              five, "<<", three)) == "40");
    CHECK(get_value(get_result_from_trivial_bitwise_expression(
              five, ">>", Immediate{ "1", "int", 4UL })) == "2");
}

TEST_CASE("get_unquoted_space_offsets")
{
    using credence::type::get_unquoted_space_offsets;
    CHECK(get_unquoted_space_offsets("x + y") ==
          std::vector<std::size_t>{ 1, 3 });
    CHECK(get_unquoted_space_offsets(R"(("a b":string:3) == x)").size() == 2);
    CHECK(get_unquoted_space_offsets("(' ':char:1) == c").size() == 2);
    CHECK(get_unquoted_space_offsets(R"(("a \" b":string:5))").empty());
}

TEST_CASE("from_rvalue_binary_expression")
{
    using credence::type::from_rvalue_binary_expression;
    using credence::type::is_binary_expression;
    auto [lhs, rhs, op] = from_rvalue_binary_expression(
        R"(("a b":string:3) == ("a b":string:3))");
    CHECK(lhs == R"(("a b":string:3))");
    CHECK(rhs == R"(("a b":string:3))");
    CHECK(op == "==");
    CHECK(is_binary_expression(R"(("a b":string:3) != ("a  c":string:4))"));
    CHECK(not is_binary_expression(R"(("m is %d":string:7))"));
}
