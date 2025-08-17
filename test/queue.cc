#include <doctest/doctest.h> // for TestCase, TEST_CASE
#include <iostream>          // for cout
#include <memory>            // for make_shared
#include <ostream>           // for basic_ostream, operator<<
#include <roxas/ir/table.h>  // for Table
#include <roxas/json.h>      // for JSON
#include <roxas/operators.h> // for operator_to_string
#include <roxas/queue.h>     // for Table
#include <roxas/types.h>     // for RValue
#include <roxas/util.h>      // for overload, unravel_nested_node_a...
#include <string>            // for basic_string, char_traits
#include <variant>           // for visit, monostate
#include <vector>            // for vector

TEST_CASE("ir/queue.cc: rvalues_to_queue")
{
    using namespace roxas;
    using namespace roxas::type;
    json::JSON obj;
    obj["test"] = json::JSON::Load(
        "{\n            \"left\" : [[{\n                  \"left\" : {\n       "
        "             \"node\" : \"number_literal\",\n                    "
        "\"root\" : 5\n                  },\n                  \"node\" : "
        "\"relation_expression\",\n                  \"right\" : {\n           "
        "         \"left\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 5\n             "
        "       },\n                    \"node\" : \"relation_expression\",\n  "
        "                  \"right\" : {\n                      \"left\" : {\n "
        "                       \"left\" : {\n                          "
        "\"node\" : \"lvalue\",\n                          \"root\" : "
        "\"double\"\n                        },\n                        "
        "\"node\" : \"function_expression\",\n                        "
        "\"right\" : [{\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 5\n       "
        "                   }],\n                        \"root\" : "
        "\"double\"\n                      },\n                      \"node\" "
        ": \"relation_expression\",\n                      \"right\" : {\n     "
        "                   \"left\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 3\n         "
        "               },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : {\n     "
        "                     \"node\" : \"number_literal\",\n                 "
        "         \"root\" : 2\n                        },\n                   "
        "     \"root\" : [\"/\"]\n                      },\n                   "
        "   \"root\" : [\"+\"]\n                    },\n                    "
        "\"root\" : [\"*\"]\n                  },\n                  \"root\" "
        ": [\"*\"]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }");

    ir::Table table{ obj };
    auto statement = obj["test"]["left"];
    std::vector<type::RValue::Type_Pointer> rvalues{};

    auto* unraveled_statements = util::unravel_nested_node_array(&statement);
    for (auto& expression : unraveled_statements->ArrayRange()) {
        auto value = std::make_shared<type::RValue::Type>(
            table.from_rvalue(expression).value);
        rvalues.push_back(value);
    }
    RValue_Evaluation_Queue list{};
    rvalues_to_queue(rvalues, &list);
    for (auto& item : list) {
        std::visit(util::overload{
                       [&](type::Operator op) {
                           std::cout << type::operator_to_string(op) << " ";
                       },
                       [&](type::RValue::Type_Pointer& s) {
                           std::visit(util::overload{
                                          [&](std::monostate) {},
                                          [&](RValue::RValue_Pointer&) {},
                                          [&](RValue::Value& s) {
                                              std::cout
                                                  << util::dump_value_type(s)
                                                  << " ";
                                          },
                                          [&](RValue::LValue&) {},
                                          [&](RValue::Unary&) {},
                                          [&](RValue::Relation&) {},
                                          [&](RValue::Function& s) {
                                              std::cout << s.first << " ";
                                          },
                                          [&](RValue::Symbol&) {

                                          } },
                                      *s);
                       }

                   },
                   item);
    }
}
