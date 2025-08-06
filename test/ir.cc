#include <doctest/doctest.h>
#include <roxas/ir.h>
#include <roxas/util.h>
#include <tuple>

using namespace roxas;

TEST_CASE("ir.cc: Intermediate_Representation::from_assignment_expression")
{
    json::JSON obj;
    obj["symbols"] = json::JSON::Load(
        "{\n  \"main\" : {\n    \"column\" : 1,\n    \"end_column\" : 5,\n    "
        "\"end_pos\" : 4,\n    \"line\" : 1,\n    \"start_pos\" : 0,\n    "
        "\"type\" : \"function_definition\"\n  },\n  \"x\" : {\n    \"column\" "
        ": 3,\n    \"end_column\" : 4,\n    \"end_pos\" : 13,\n    \"line\" : "
        "2,\n    \"start_pos\" : 12,\n    \"type\" : \"number_literal\"\n  "
        "}\n}");
    obj["test"] = json::JSON::Load(
        "{\n                  \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n                  "
        "},\n                  \"node\" : \"assignment_expression\",\n         "
        "         \"right\" : {\n                    \"node\" : "
        "\"number_literal\",\n                    \"root\" : 5\n               "
        "   },\n                  \"root\" : [\"=\", null]\n                }");
    auto temp = Intermediate_Representation(obj["symbols"]);
    // no declaration with `auto' or `extern', should throw
    CHECK_THROWS(temp.from_assignment_expression(obj["test"]));

    temp.symbols_.table_.emplace("x", std::make_tuple(" ", "null", 0));

    CHECK_NOTHROW(temp.from_assignment_expression(obj["test"]));

    CHECK(util::tuple_to_string(temp.quintuples_[0]) == "=, x, 5, int, 4, , ");
}

TEST_CASE("ir.cc: Intermediate_Representation::from_identifier")
{
    json::JSON obj;
    obj["symbols"] = json::JSON::Load(
        "{\n  \"main\" : {\n    \"column\" : 1,\n    \"end_column\" : 5,\n    "
        "\"end_pos\" : 4,\n    \"line\" : 1,\n    \"start_pos\" : 0,\n    "
        "\"type\" : \"function_definition\"\n  },\n  \"x\" : {\n    \"column\" "
        ": 3,\n    \"end_column\" : 4,\n    \"end_pos\" : 13,\n    \"line\" : "
        "2,\n    \"start_pos\" : 12,\n    \"type\" : \"number_literal\"\n  "
        "}\n}");
    obj["test"] = json::JSON::Load("{\"node\":  \"lvalue\","
                                   "\"root\": \"x\""
                                   "}");
    auto temp = Intermediate_Representation(obj["test"]);
    // not declared with `auto' or `extern', should throw
    CHECK_THROWS(temp.from_identifier(obj["test"]));

    auto temp2 = Intermediate_Representation(obj["symbols"]);
    CHECK_THROWS(temp2.from_identifier(obj["test"]));

    temp2.symbols_.set_symbol_by_name("x", { "", "int", sizeof(int) });
    CHECK_NOTHROW(temp2.from_identifier(obj["test"]));
}

TEST_CASE("ir.cc: Intermediate_Representation::from_number_literal")
{
    json::JSON obj;
    obj["test"] = json::JSON::Load("{\"node\":  \"number_literal\","
                                   "\"root\": 10"
                                   "}");

    auto temp = Intermediate_Representation(obj);
    auto data = temp.from_number_literal(obj["test"]);
    auto [value, type, size] = data;
    CHECK(value == "10");
    CHECK(type == "int");
    CHECK(size == sizeof(int));
}

TEST_CASE("ir.cc: Intermediate_Representation::from_string_literal")
{
    json::JSON obj;
    obj["test"] = json::JSON::Load("{\"node\":  \"string_literal\","
                                   "\"root\": \"test string\""
                                   "}");

    auto temp = Intermediate_Representation(obj);
    auto data = temp.from_string_literal(obj["test"]);
    auto [value, type, size] = data;
    CHECK(value == "test string");
    CHECK(type == "string");
    CHECK(size == sizeof(char) * 11);
}

TEST_CASE("ir.cc: Intermediate_Representation::from_constant_literal")
{
    json::JSON obj;
    obj["test"] = json::JSON::Load("{\"node\":  \"constant_literal\","
                                   "\"root\": \"x\""
                                   "}");

    auto temp = Intermediate_Representation(obj);
    auto data = temp.from_constant_literal(obj["test"]);
    auto [value, type, size] = data;
    CHECK(value == "x");
    CHECK(type == "int");
    CHECK(size == sizeof(int));
}