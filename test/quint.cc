#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase, TEST_CA...
#include <map>               // for map
#include <roxas/ir/quint.h>  // for build_from_auto_statement
#include <roxas/ir/types.h>  // for Value_Type, Type_, Byte
#include <roxas/json.h>      // for JSON
#include <roxas/symbol.h>    // for Symbol_Table
#include <string>            // for basic_string
#include <utility>           // for pair, make_pair
#include <variant>           // for monostate

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

TEST_CASE_FIXTURE(Fixture, "ir/quint.cc: build_from_auto_statement")
{
    using namespace roxas::ir;
    using namespace roxas::ir::quint;
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

    roxas::Symbol_Table<> symbols{};
    build_from_auto_statement(symbols, obj["test"]);
    CHECK(symbols.table_.size() == 3);

    CHECK(symbols.table_.contains("x") == true);
    CHECK(symbols.table_.contains("y") == true);
    CHECK(symbols.table_.contains("z") == true);

    type::Value_Type empty_value =
        std::make_pair(std::monostate(), type::Type_["null"]);
    type::Value_Type word_value =
        std::make_pair("__WORD__", type::Type_["word"]);
    type::Value_Type byte_value = std::make_pair(static_cast<type::Byte>('0'),
                                                 std::make_pair("byte", 50));

    CHECK(symbols.table_["x"] == byte_value);
    CHECK(symbols.table_["y"] == word_value);
    CHECK(symbols.table_["z"] == empty_value);
}