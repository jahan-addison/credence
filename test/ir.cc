#include <doctest/doctest.h>
#include <roxas/ir.h>

using namespace roxas;

TEST_CASE("ir.cc: Intermediate_Representation::from_number_literal")
{
    json::JSON obj;
    obj["test"] = json::JSON::Load("{\"node\":  \"number_literal\","
                                   "\"root\": \"5\""
                                   "}");

    auto temp = Intermediate_Representation();
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

    auto temp = Intermediate_Representation();
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

    auto temp = Intermediate_Representation();
    temp.from_constant_literal(obj["test"]);
    auto [value, type, size] = temp.symbols_.get_symbol_by_name("_t0");
    CHECK(value == "x");
    CHECK(type == "int");
    CHECK(size == sizeof(int));
}