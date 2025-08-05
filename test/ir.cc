#include <doctest/doctest.h>
#include <roxas/ir.h>

using namespace roxas;

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
                                   "\"root\": \"5\""
                                   "}");

    auto temp = Intermediate_Representation(obj);
    temp.from_number_literal(obj["test"]);
    auto [value, type, size] = temp.symbols_.get_symbol_by_name("_t0");
    CHECK(value == "5");
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
    temp.from_string_literal(obj["test"]);

    auto [value, type, size] = temp.symbols_.get_symbol_by_name("_t0");
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
    temp.from_constant_literal(obj["test"]);
    auto [value, type, size] = temp.symbols_.get_symbol_by_name("_t0");
    CHECK(value == "x");
    CHECK(type == "int");
    CHECK(size == sizeof(int));
}