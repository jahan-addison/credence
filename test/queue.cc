#include <doctest/doctest.h> // for TestCase, TEST_CASE
#include <iostream>          // for ostringstream, cout
#include <map>               // for map
#include <memory>            // for allocator, shared_ptr, make_shared
#include <roxas/ir/table.h>  // for Table
#include <roxas/json.h>      // for JSON
#include <roxas/operators.h> // for operator<<, operator_to_string
#include <roxas/queue.h>     // for rvalues_to_queue, RValue_Queue
#include <roxas/symbol.h>    // for Symbol_Table
#include <roxas/types.h>     // for RValue, Type_
#include <roxas/util.h>      // for dump_value_type, overload
#include <sstream>           // for basic_ostringstream
#include <string>            // for basic_string, char_traits, string
#include <utility>           // for pair
#include <variant>           // for monostate, visit
#include <vector>            // for vector

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

std::string queue_of_rvalues_to_string(roxas::RValue_Queue* rvalues_queue)
{
    using namespace roxas;
    using namespace roxas::type;
    auto oss = std::ostringstream();
    for (auto& item : *rvalues_queue) {
        std::visit(util::overload{ [&](type::Operator op) {
                                      oss << type::operator_to_string(op)
                                          << " ";
                                  },
                                   [&](type::RValue::Type_Pointer& s) {
                                       oss << rvalue_to_string(*s);
                                   }

                   },
                   item);
    }
    return oss.str();
}

TEST_CASE("ir/queue.cc: rvalues_to_queue")
{
    using namespace roxas;
    using namespace roxas::type;
    json::JSON obj;

    obj["complex"] = json::JSON::Load(
        "{\n                  \"left\" : {\n                    \"node\" : "
        "\"number_literal\",\n                    \"root\" : 5\n               "
        "   },\n                  \"node\" : \"relation_expression\",\n        "
        "          \"right\" : {\n                    \"left\" : {\n           "
        "           \"node\" : \"number_literal\",\n                      "
        "\"root\" : 5\n                    },\n                    \"node\" : "
        "\"relation_expression\",\n                    \"right\" : {\n         "
        "             \"left\" : {\n                        \"left\" : {\n     "
        "                     \"node\" : \"lvalue\",\n                         "
        " \"root\" : \"exp\"\n                        },\n                     "
        "   \"node\" : \"function_expression\",\n                        "
        "\"right\" : [{\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 2\n       "
        "                   }, {\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 5\n       "
        "                   }],\n                        \"root\" : \"exp\"\n  "
        "                    },\n                      \"node\" : "
        "\"relation_expression\",\n                      \"right\" : {\n       "
        "                 \"left\" : {\n                          \"left\" : "
        "{\n                            \"node\" : \"number_literal\",\n       "
        "                     \"root\" : 4\n                          },\n     "
        "                     \"node\" : \"unary_expression\",\n               "
        "           \"root\" : [\"~\"]\n                        },\n           "
        "             \"node\" : \"relation_expression\",\n                    "
        "    \"right\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 2\n         "
        "               },\n                        \"root\" : [\"^\"]\n       "
        "               },\n                      \"root\" : [\"/\"]\n         "
        "           },\n                    \"root\" : [\"+\"]\n               "
        "   },\n                  \"root\" : [\"*\"]\n                }\n      "
        "    ");
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

    std::string complex_expected =
        "(5:int:4) (5:int:4) exp (2:int:4) (5:int:4) PUSH PUSH CALL + * "
        "(4:int:4) (2:int:4) ^ ~ / ";
    std::string unary_expected = "(5:int:4) ~ ";
    std::string equal_expected = "x (5:int:4) (5:int:4) + = ";
    std::string unary_relation_expected = "(5:int:4) ~ (2:int:4) ^ ";
    std::string ternary_expected =
        "x (5:int:4) (4:int:4) (10:int:4) (1:int:4) ?: < = ";
    std::string function_expected =
        "puts (1:int:4) (2:int:4) (3:int:4) PUSH PUSH PUSH CALL ";

    std::vector<type::RValue::Type_Pointer> rvalues{};
    RValue_Queue list{};
    std::string test{};

    rvalues.push_back(std::make_shared<type::RValue::Type>(
        table.from_rvalue(obj["complex"]).value));
    rvalues_to_queue(rvalues, &list);
    test = queue_of_rvalues_to_string(&list);
    CHECK(test == complex_expected);
    rvalues.clear();
    list.clear();

    rvalues.push_back(std::make_shared<type::RValue::Type>(
        table.from_rvalue(obj["unary"]).value));
    rvalues_to_queue(rvalues, &list);
    test = queue_of_rvalues_to_string(&list);
    CHECK(test == unary_expected);
    rvalues.clear();
    list.clear();

    rvalues.push_back(std::make_shared<type::RValue::Type>(
        table.from_rvalue(obj["equal"]).value));
    rvalues_to_queue(rvalues, &list);
    test = queue_of_rvalues_to_string(&list);
    CHECK(test == equal_expected);
    rvalues.clear();
    list.clear();

    rvalues.push_back(std::make_shared<type::RValue::Type>(
        table.from_rvalue(obj["unary_relation"]).value));
    rvalues_to_queue(rvalues, &list);
    test = queue_of_rvalues_to_string(&list);
    CHECK(test == unary_relation_expected);
    rvalues.clear();
    list.clear();

    rvalues.push_back(std::make_shared<type::RValue::Type>(
        table.from_rvalue(obj["ternary"]).value));
    rvalues_to_queue(rvalues, &list);
    test = queue_of_rvalues_to_string(&list);
    CHECK(test == ternary_expected);
    rvalues.clear();
    list.clear();

    rvalues.push_back(std::make_shared<type::RValue::Type>(
        table.from_rvalue(obj["function"]).value));
    rvalues_to_queue(rvalues, &list);
    test = queue_of_rvalues_to_string(&list);
    CHECK(test == function_expected);
    rvalues.clear();
    list.clear();
}
