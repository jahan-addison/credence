#include <doctest/doctest.h> // for TestCase, TEST_CASE
#include <iostream>          // for cout
#include <matchit.h>
#include <memory>  // for make_shared
#include <ostream> // for basic_ostream, operator<<
#include <ranges>
#include <roxas/ir/table.h>  // for Table
#include <roxas/json.h>      // for JSON
#include <roxas/operators.h> // for operator_to_string
#include <roxas/queue.h>     // for Table
#include <roxas/types.h>     // for RValue
#include <roxas/util.h>      // for overload, unravel_nested_node_a...
#include <sstream>
#include <string>  // for basic_string, char_traits
#include <variant> // for visit, monostate
#include <vector>  // for vector

std::string rvalue_to_string(roxas::type::RValue::Type& rvalue)
{
    using namespace roxas::type;
    auto oss = std::ostringstream();
    std::visit(roxas::util::overload{
                   [&](std::monostate) {},
                   [&](RValue::RValue_Pointer&) {},
                   [&](RValue::Value& s) {
                       oss << roxas::util::dump_value_type(s) << " ";
                   },
                   [&](RValue::LValue& s) { oss << s.first << " "; },
                   [&](RValue::Unary& s) {
                       oss << s.first << rvalue_to_string(s.second->value);
                   },
                   [&](RValue::Relation& s) {
                       for (auto& relation : s.second) {
                           oss << rvalue_to_string(relation->value) << " ";
                       }
                   },
                   [&](RValue::Function& s) { oss << s.first.first << " "; },
                   [&](RValue::Symbol& s) { oss << s.first.first << " "; } },
               rvalue);
    return oss.str();
}

TEST_CASE("ir/queue.cc: rvalues_to_queue")
{
    using namespace roxas;
    using namespace roxas::type;
    json::JSON obj;
    obj["simple"] = json::JSON::Load(
        "{\n                  \"left\" : {\n                    \"node\" : "
        "\"number_literal\",\n                    \"root\" : 5\n               "
        "   },\n                  \"node\" : \"relation_expression\",\n        "
        "          \"right\" : {\n                    \"left\" : {\n           "
        "           \"node\" : \"number_literal\",\n                      "
        "\"root\" : 5\n                    },\n                    \"node\" : "
        "\"relation_expression\",\n                    \"right\" : {\n         "
        "             \"left\" : {\n                        \"node\" : "
        "\"number_literal\",\n                        \"root\" : 4\n           "
        "           },\n                      \"node\" : "
        "\"relation_expression\",\n                      \"right\" : {\n       "
        "                 \"left\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 3\n         "
        "               },\n                        \"node\" : "
        "\"unary_expression\",\n                        \"root\" : [\"~\"]\n   "
        "                   },\n                      \"root\" : [\"+\"]\n     "
        "               },\n                    \"root\" : [\"-\"]\n           "
        "       },\n                  \"root\" : [\"*\"]\n                }");

    /* clang-format off */
    obj["complex"] = json::JSON::Load("{\n                  \"left\" : {\n                    \"node\" : \"number_literal\",\n                    \"root\" : 5\n                  },\n                  \"node\" : \"relation_expression\",\n                  \"right\" : {\n                    \"left\" : {\n                      \"node\" : \"number_literal\",\n                      \"root\" : 5\n                    },\n                    \"node\" : \"relation_expression\",\n                    \"right\" : {\n                      \"left\" : {\n                        \"left\" : {\n                          \"node\" : \"lvalue\",\n                          \"root\" : \"exp\"\n                        },\n                        \"node\" : \"function_expression\",\n                        \"right\" : [{\n                            \"node\" : \"number_literal\",\n                            \"root\" : 2\n                          }, {\n                            \"node\" : \"number_literal\",\n                            \"root\" : 5\n                          }],\n                        \"root\" : \"exp\"\n                      },\n                      \"node\" : \"relation_expression\",\n                      \"right\" : {\n                        \"left\" : {\n                          \"left\" : {\n                            \"node\" : \"number_literal\",\n                            \"root\" : 4\n                          },\n                          \"node\" : \"unary_expression\",\n                          \"root\" : [\"~\"]\n                        },\n                        \"node\" : \"relation_expression\",\n                        \"right\" : {\n                          \"node\" : \"number_literal\",\n                          \"root\" : 2\n                        },\n                        \"root\" : [\"^\"]\n                      },\n                      \"root\" : [\"/\"]\n                    },\n                    \"root\" : [\"+\"]\n                  },\n                  \"root\" : [\"*\"]\n}");
    /* clang-format on */
    obj["unary"] = json::JSON::Load(
        "{\n  \"left\": {\n    \"node\": \"number_literal\",\n    \"root\": "
        "5\n  },\n  \"node\": \"unary_expression\",\n  \"root\": [\n    "
        "\"~\"\n  ]\n}");
    obj["equal"] = json::JSON::Load(
        "{\n                  \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n                  "
        "},\n                  \"node\" : \"assignment_expression\",\n         "
        "         \"right\" : {\n                    \"left\" : {\n            "
        "          \"node\" : \"number_literal\",\n                      "
        "\"root\" : 5\n                    },\n                    \"node\" : "
        "\"relation_expression\",\n                    \"right\" : {\n         "
        "             \"node\" : \"number_literal\",\n                      "
        "\"root\" : 5\n                    },\n                    \"root\" : "
        "[\"+\"]\n                  },\n                  \"root\" : [\"=\", "
        "null]\n                }\n");
    obj["unary_relation"] = json::JSON::Load(
        "{\n                  \"left\" : {\n                    \"left\" : {\n "
        "                     \"node\" : \"number_literal\",\n                 "
        "     \"root\" : 5\n                    },\n                    "
        "\"node\" : \"unary_expression\",\n                    \"root\" : "
        "[\"~\"]\n                  },\n                  \"node\" : "
        "\"relation_expression\",\n                  \"right\" : {\n           "
        "         \"node\" : \"number_literal\",\n                    \"root\" "
        ": 2\n                  },\n                  \"root\" : [\"^\"]\n     "
        "           }");
    obj["ternary"] = json::JSON::Load(
        "{\n                  \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n                  "
        "},\n                  \"node\" : \"assignment_expression\",\n         "
        "         \"right\" : {\n                    \"left\" : {\n            "
        "          \"node\" : \"number_literal\",\n                      "
        "\"root\" : 5\n                    },\n                    \"node\" : "
        "\"relation_expression\",\n                    \"right\" : {\n         "
        "             \"left\" : {\n                        \"node\" : "
        "\"number_literal\",\n                        \"root\" : 10\n          "
        "            },\n                      \"node\" : "
        "\"ternary_expression\",\n                      \"right\" : {\n        "
        "                \"node\" : \"number_literal\",\n                      "
        "  \"root\" : 1\n                      },\n                      "
        "\"root\" : {\n                        \"node\" : "
        "\"number_literal\",\n                        \"root\" : 4\n           "
        "           }\n                    },\n                    \"root\" : "
        "[\"<\"]\n                  },\n                  \"root\" : [\"=\", "
        "null]\n                }");
    obj["function"] = json::JSON::Load(
        "{\n                  \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"puts\"\n                "
        "  },\n                  \"node\" : \"function_expression\",\n         "
        "         \"right\" : [{\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 1\n             "
        "       }, {\n                      \"node\" : \"number_literal\",\n   "
        "                   \"root\" : 2\n                    }, {\n           "
        "           \"node\" : \"number_literal\",\n                      "
        "\"root\" : 3\n                    }],\n                  \"root\" : "
        "\"puts\"\n                },\n");
    ;

    ir::Table table{ obj };
    RValue::Value null = { std::monostate(), roxas::type::Type_["null"] };
    table.symbols_.table_.emplace("x", null);
    table.symbols_.table_.emplace("double", null);
    table.symbols_.table_.emplace("exp", null);
    table.symbols_.table_.emplace("puts", null);
    table.symbols_.table_.emplace("y", null);

    auto statement = obj["simple"];
    std::cout << "JSON: " << statement.dump() << std::endl;
    std::vector<type::RValue::Type_Pointer> rvalues{};
    RValue_Queue list{};
    rvalues.push_back(std::make_shared<type::RValue::Type>(
        table.from_rvalue(statement).value));
    rvalues_to_queue(rvalues, &list);
    for (auto& item : list) {
        std::visit(util::overload{ [&](type::Operator op) {
                                      std::cout << type::operator_to_string(op)
                                                << " ";
                                  },
                                   [&](type::RValue::Type_Pointer& s) {
                                       std::cout << rvalue_to_string(*s);
                                   }

                   },
                   item);
    }
}
