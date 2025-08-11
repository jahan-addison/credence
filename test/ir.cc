#include <doctest/doctest.h> // for ResultBuilder, TestCase, CHECK, TEST_CASE
#include <map>               // for map
#include <memory>            // for unique_ptr
#include <roxas/ir/ir.h>     // for Intermediate_Representation
#include <roxas/ir/types.h>  // for Value_Type, Type_, RValue, Byte
#include <roxas/json.h>      // for JSON
#include <roxas/symbol.h>    // for Symbol_Table
#include <string>            // for basic_string, string
#include <tuple>             // for get, make_tuple
#include <utility>           // for pair, get, make_pair
#include <variant>           // for get, monostate

struct Fixture
{
    json::JSON lvalue_ast_node_json;
    json::JSON assignment_symbol_table;

    Fixture()
    {
        assignment_symbol_table = json::JSON::Load(
            "{\n  \"main\" : {\n    \"column\" : 1,\n    \"end_column\" : 5,\n "
            "   "
            "\"end_pos\" : 4,\n    \"line\" : 1,\n    \"start_pos\" : 0,\n    "
            "\"type\" : \"function_definition\"\n  },\n  \"x\" : {\n    "
            "\"column\" "
            ": 3,\n    \"end_column\" : 4,\n    \"end_pos\" : 13,\n    "
            "\"line\" : "
            "2,\n    \"start_pos\" : 12,\n    \"type\" : \"number_literal\"\n  "
            "}\n}");
        lvalue_ast_node_json = json::JSON::Load(
            " {\n        \"left\" : [{\n            \"left\" : [{\n            "
            "    "
            "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n        "
            "    "
            "  }, {\n                \"node\" : \"lvalue\",\n                "
            "\"root\" : \"y\"\n              }, {\n                \"node\" : "
            "\"lvalue\",\n                \"root\" :\"z\"\n              }],\n "
            "   "
            "        \"node\" : \"statement\",\n            \"root\" : "
            "\"auto\"\n  "
            "        }, {\n            \"left\" : [[{\n                  "
            "\"left\" "
            ": {\n                    \"node\" : \"lvalue\",\n                 "
            "   "
            "\"root\" : \"x\"\n                  },\n                  "
            "\"node\" : "
            "\"assignment_expression\",\n                  \"right\" : {\n     "
            "    "
            "           \"node\" : \"number_literal\",\n                    "
            "\"root\" : 5\n                  },\n                  \"root\" : "
            "[\"=\", null]\n                }]],\n            \"node\" : "
            "\"statement\",\n            \"root\" : \"rvalue\"\n          }]\n "
            "}\n ");
    }
    ~Fixture() = default;
};

// TEST_CASE_FIXTURE(Fixture, "ir/ir.cc:
// Intermediate_Representation::parse_node")
// {
//     using namespace ir;
//     json::JSON obj;
//     obj["symbols"] = assignment_symbol_table;

//     obj["test"] = json::JSON::Load(
//         " {\n        \"left\" : [{\n            \"left\" : [{\n "
//         "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n " "  },
//         {\n                \"node\" : \"lvalue\",\n                "
//         "\"root\" : \"y\"\n              }, {\n                \"node\" : "
//         "\"lvalue\",\n                \"root\" :\"z\"\n              }],\n "
//         "        \"node\" : \"statement\",\n            \"root\" : \"auto\"\n
//         " "        }, {\n            \"left\" : [[{\n \"left\" "
//         ": {\n                    \"node\" : \"lvalue\",\n "
//         "\"root\" : \"x\"\n                  },\n                  \"node\" :
//         "
//         "\"assignment_expression\",\n                  \"right\" : {\n " "
//         \"node\" : \"number_literal\",\n                    "
//         "\"root\" : 5\n                  },\n                  \"root\" : "
//         "[\"=\", null]\n                }]],\n            \"node\" : "
//         "\"statement\",\n            \"root\" : \"rvalue\"\n          }]\n "
//         "}\n ");

//     auto temp = Intermediate_Representation(obj["symbols"]);
//     temp.parse_node(obj["test"]);
//     Quintuple test = { Operator::EQUAL, "x", "5:int:4", "", "" };
// }

TEST_CASE_FIXTURE(Fixture,
                  "ir/ir.cc: Intermediate_Representation::from_auto_statement")
{
    using namespace roxas::ir;
    json::JSON obj;
    obj["test"] = json::JSON::Load(
        "{\n  \"left\" : [{\n      \"left\" : {\n        \"node\" : "
        "\"number_literal\",\n        \"root\" : 50\n      },\n      \"node\" "
        ": \"vector_lvalue\",\n      \"root\" : \"x\"\n    }, {\n      "
        "\"left\" : {\n        \"node\" : \"lvalue\",\n        \"root\" : "
        "\"y\"\n      },\n      \"node\" : \"indirect_lvalue\",\n      "
        "\"root\" : [\"*\"]\n    }, {\n      \"node\" : \"lvalue\",\n      "
        "\"root\" : \"z\"\n    }],\n  \"node\" : \"statement\",\n  \"root\" : "
        "\"auto\"\n}");

    auto temp = Intermediate_Representation(obj["test"]);
    temp.from_auto_statement(obj["test"]);

    CHECK(temp.symbols_.table_.size() == 3);

    CHECK(temp.symbols_.table_.contains("x") == true);
    CHECK(temp.symbols_.table_.contains("y") == true);
    CHECK(temp.symbols_.table_.contains("z") == true);

    type::Value_Type empty_value =
        std::make_pair(std::monostate(), type::Type_["null"]);
    type::Value_Type word_value =
        std::make_pair("__WORD_", type::Type_["word"]);
    type::Value_Type byte_value = std::make_pair(static_cast<type::Byte>('0'),
                                                 std::make_pair("byte", 50));

    CHECK(temp.symbols_.table_["x"] == byte_value);
    CHECK(temp.symbols_.table_["y"] == word_value);
    CHECK(temp.symbols_.table_["z"] == empty_value);
}

TEST_CASE_FIXTURE(
    Fixture,
    "ir/ir.cc: Intermediate_Representation::from_assignment_expression")
{
    using namespace roxas::ir;
    json::JSON obj;
    obj["symbols"] = assignment_symbol_table;
    obj["test"] = json::JSON::Load(
        "{\n                  \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n               "
        "   "
        "},\n                  \"node\" : \"assignment_expression\",\n     "
        "    "
        "         \"right\" : {\n                    \"node\" : "
        "\"number_literal\",\n                    \"root\" : 5\n           "
        "    "
        "   },\n                  \"root\" : [\"=\", null]\n               "
        " }");
    auto temp = Intermediate_Representation(obj["symbols"]);
    // no declaration with `auto' or `extern', should throw
    CHECK_THROWS(temp.from_assignment_expression(obj["test"]));

    type::Value_Type value_type = { std::monostate(), type::Type_["null"] };
    type::Value_Type assigned_type = { 5, type::Type_["int"] };

    temp.symbols_.table_.emplace("x", value_type);

    auto expr = temp.from_assignment_expression(obj["test"]);

    auto lhs = std::get<type::RValue::Symbol>(expr.value).first;
    auto* rhs = &std::get<type::RValue::Symbol>(expr.value).second;

    CHECK(lhs.first == "x");
    CHECK(lhs.second == value_type);
    CHECK(std::get<type::Value_Type>((*rhs)->value) == assigned_type);
}

TEST_CASE_FIXTURE(Fixture, "ir/ir.cc: Intermediate_Representation::is_symbol")
{
    using namespace roxas::ir;
    json::JSON obj;
    obj["symbols"] = assignment_symbol_table;
    obj["test"] = json::JSON::Load("{\"node\":  \"lvalue\","
                                   "\"root\": \"x\""
                                   "}");
    auto temp = Intermediate_Representation(obj["test"]);
    // not declared with `auto' or `extern', should throw
    CHECK(temp.is_symbol(obj["test"]) == false);

    auto temp2 = Intermediate_Representation(obj["symbols"]);
    CHECK(temp2.is_symbol(obj["test"]) == false);

    type::Value_Type value_type = { std::monostate(), type::Type_["null"] };

    temp2.symbols_.set_symbol_by_name("x", value_type);
    CHECK(temp2.is_symbol(obj["test"]) == true);
}

TEST_CASE("ir/ir.cc: Intermediate_Representation::from_indirect_identifier")
{
    using namespace roxas::ir;
    json::JSON obj;
    obj["test"] = json::JSON::Load(
        "{\n                \"left\" : {\n                  \"node\" : "
        "\"lvalue\",\n                  \"root\" : \"x\"\n                },\n "
        "               \"node\" : \"indirect_lvalue\",\n                "
        "\"root\" : [\"*\"]\n              }");

    auto temp = Intermediate_Representation(obj["test"]);
    CHECK_THROWS(temp.from_indirect_identifier(obj["test"]));
    type::Value_Type test = { "__WORD_", type::Type_["word"] };
    temp.symbols_.table_["x"] = test;
    temp.from_indirect_identifier(obj["test"]);
}

TEST_CASE("ir/ir.cc: Intermediate_Representation::from_vector_idenfitier")
{
    using namespace roxas::ir;
    json::JSON obj;
    obj["test"] = json::JSON::Load(
        "{\n                \"left\" : {\n                  \"node\" : "
        "\"number_literal\",\n                  \"root\" : 50\n                "
        "},\n                \"node\" : \"vector_lvalue\",\n                "
        "\"root\" : \"x\"\n              }");

    auto temp = Intermediate_Representation(obj["test"]);
    CHECK_THROWS(temp.from_vector_idenfitier(obj["test"]));
    type::Value_Type test = std::make_pair('0', std::make_pair("byte", 50));
    temp.symbols_.table_["x"] = test;
    CHECK(temp.from_vector_idenfitier(obj["test"]) == test);
}

TEST_CASE("ir/ir.cc: Intermediate_Representation::from_number_literal")
{
    using namespace roxas::ir;
    json::JSON obj;
    obj["test"] = json::JSON::Load("{\"node\":  \"number_literal\","
                                   "\"root\": 10"
                                   "}");

    auto temp = Intermediate_Representation(obj);
    auto data = temp.from_number_literal(obj["test"]);
    auto [value, type] = data;
    CHECK(std::get<int>(value) == 10);
    CHECK(type.first == "int");
    CHECK(type.second == sizeof(int));
}

TEST_CASE("ir/ir.cc: Intermediate_Representation::from_string_literal")
{
    using namespace roxas::ir;
    json::JSON obj;
    obj["test"] = json::JSON::Load("{\"node\":  \"string_literal\","
                                   "\"root\": \"test string\""
                                   "}");

    auto temp = Intermediate_Representation(obj);
    auto data = temp.from_string_literal(obj["test"]);
    auto [value, type] = data;
    CHECK(std::get<std::string>(value) == "test string");
    CHECK(type.first == "string");
    CHECK(type.second == sizeof(char) * 11);
}

TEST_CASE("ir/ir.cc: Intermediate_Representation::from_constant_literal")
{
    using namespace roxas::ir;
    json::JSON obj;
    obj["test"] = json::JSON::Load("{\"node\":  \"constant_literal\","
                                   "\"root\": \"x\""
                                   "}");

    auto temp = Intermediate_Representation(obj);
    auto data = temp.from_constant_literal(obj["test"]);
    auto [value, type] = data;
    CHECK(std::get<char>(value) == 'x');
    CHECK(type.first == "char");
    CHECK(type.second == sizeof(char));
}