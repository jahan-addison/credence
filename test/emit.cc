#include <doctest/doctest.h> // for ResultBuilder, TestCase, CHECK, TEST_CA...
#include <roxas/ir/operators.h> // for roxas
#include <roxas/json.h>         // for JSON
#include <string>               // for basic_string

#include <roxas/json.h>

using namespace roxas;

struct Fixture
{
    json::JSON lvalue_symbol_table_json;
    json::JSON lvalue_ast_node_json;

    Fixture()
    {
        lvalue_symbol_table_json = json::JSON::Load(
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