#include <doctest/doctest.h>
#include <roxas/ir/ir.h>
#include <string>

#include <roxas/json.h>

using namespace roxas;

TEST_CASE("ir/emit.cc: emit")
{
    using namespace ir;
    json::JSON obj;
    obj["symbols"] = json::JSON::Load(
        "{\n  \"main\" : {\n    \"column\" : 1,\n    \"end_column\" : 5,\n    "
        "\"end_pos\" : 4,\n    \"line\" : 1,\n    \"start_pos\" : 0,\n    "
        "\"type\" : \"function_definition\"\n  },\n  \"x\" : {\n    \"column\" "
        ": 3,\n    \"end_column\" : 4,\n    \"end_pos\" : 13,\n    \"line\" : "
        "2,\n    \"start_pos\" : 12,\n    \"type\" : \"number_literal\"\n  "
        "}\n}");

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
}