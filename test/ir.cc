#include <doctest/doctest.h> // for ResultBuilder, TestCase, CHECK, TEST...
#include <map>               // for map
#include <roxas/ir/ir.h>     // for Intermediate_Representation
#include <roxas/util.h>      // for tuple_to_string
#include <string>            // for basic_string
#include <tuple>             // for get, tuple, make_tuple

#include "roxas/symbol.h"       // for Symbol_Tabl
#include <roxas/ir/operators.h> // for Operator, operator<<
#include <roxas/ir/types.h>     // for Quintuple
#include <roxas/json.h>         // for JSON

using namespace roxas;

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

TEST_CASE_FIXTURE(Fixture, "ir/ir.cc: Intermediate_Representation::parse_node")
{
    using namespace ir;
    json::JSON obj;
    obj["symbols"] = assignment_symbol_table;

    obj["test"] = json::JSON::Load(
        " {\n        \"left\" : [{\n            \"left\" : [{\n                "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n            "
        "  }, {\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"y\"\n              }, {\n                \"node\" : "
        "\"lvalue\",\n                \"root\" :\"z\"\n              }],\n    "
        "        \"node\" : \"statement\",\n            \"root\" : \"auto\"\n  "
        "        }, {\n            \"left\" : [[{\n                  \"left\" "
        ": {\n                    \"node\" : \"lvalue\",\n                    "
        "\"root\" : \"x\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 5\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }]\n "
        "}\n ");

    auto temp = Intermediate_Representation(obj["symbols"]);
    temp.parse_node(obj["test"]);
    Quintuple test = { Operator::EQUAL, "x", "5:int:4", "", "" };
    CHECK(temp.quintuples_.size() == 1);
    CHECK(temp.quintuples_[0] == test);
}

TEST_CASE_FIXTURE(Fixture,
                  "ir/ir.cc: Intermediate_Representation::from_auto_statement")
{
    using namespace ir;
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

    CHECK(temp.symbols_.table_["x"] == std::make_tuple("", "array", 50));
    CHECK(temp.symbols_.table_["y"] ==
          std::make_tuple("", "pointer", sizeof(int*)));
    CHECK(temp.symbols_.table_["z"] == std::make_tuple("", "null", 0));
}

TEST_CASE_FIXTURE(
    Fixture,
    "ir/ir.cc: Intermediate_Representation::from_assignment_expression")
{
    using namespace ir;
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

    temp.symbols_.table_.emplace("x", std::make_tuple(" ", "null", 0));

    CHECK_NOTHROW(temp.from_assignment_expression(obj["test"]));

    CHECK(util::tuple_to_string(temp.quintuples_[0]) == "=, x, 5:int:4, , ");
}

TEST_CASE_FIXTURE(
    Fixture,
    "ir/ir.cc: Intermediate_Representation::check_identifier_symbol")
{
    using namespace ir;
    json::JSON obj;
    obj["symbols"] = assignment_symbol_table;
    obj["test"] = json::JSON::Load("{\"node\":  \"lvalue\","
                                   "\"root\": \"x\""
                                   "}");
    auto temp = Intermediate_Representation(obj["test"]);
    // not declared with `auto' or `extern', should throw
    CHECK_THROWS(temp.check_identifier_symbol(obj["test"]));

    auto temp2 = Intermediate_Representation(obj["symbols"]);
    CHECK_THROWS(temp2.check_identifier_symbol(obj["test"]));

    temp2.symbols_.set_symbol_by_name("x", { "", "int", sizeof(int) });
    CHECK_NOTHROW(temp2.check_identifier_symbol(obj["test"]));
}

TEST_CASE("ir/ir.cc: Intermediate_Representation::from_number_literal")
{
    using namespace ir;
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

TEST_CASE("ir/ir.cc: Intermediate_Representation::from_string_literal")
{
    using namespace ir;
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

TEST_CASE("ir/ir.cc: Intermediate_Representation::from_constant_literal")
{
    using namespace ir;
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