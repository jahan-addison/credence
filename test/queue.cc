// clang-format off
#include <vector>             // for vector
#include <doctest/doctest.h>  // for ResultBuilder, CHECK, TestCase, TEST_CASE
#include <credence/rvalue.h>   // for RValue_Parser
#include <credence/json.h>       // for JSON
#include <credence/queue.h>      // for rvalues_to_queue, RValue_Queue
#include <credence/symbol.h>     // for Symbol_Table
#include <credence/types.h>      // for RValue, Type_
#include <credence/util.h>       // for queue_of_rvalues_to_string
#include <map>                // for map
#include <memory>             // for make_shared
#include <string>             // for basic_string, string
#include <utility>            // for pair
#include <variant>            // for monostate
// clang-format on

TEST_CASE("ir/queue.cc: rvalues_to_queue")
{
    using namespace credence;
    using namespace credence::type;
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
    obj["evaluated"] = json::JSON::Load(
        "{\n                  \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n                  "
        "},\n                  \"node\" : \"assignment_expression\",\n         "
        "         \"right\" : {\n                    \"left\" : {\n            "
        "          \"node\" : \"evaluated_expression\",\n                      "
        "\"root\" : {\n                        \"left\" : {\n                  "
        "        \"node\" : \"number_literal\",\n                          "
        "\"root\" : 5\n                        },\n                        "
        "\"node\" : \"relation_expression\",\n                        "
        "\"right\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 5\n         "
        "               },\n                        \"root\" : [\"*\"]\n       "
        "               }\n                    },\n                    "
        "\"node\" : \"relation_expression\",\n                    \"right\" : "
        "{\n                      \"node\" : \"evaluated_expression\",\n       "
        "               \"root\" : {\n                        \"left\" : {\n   "
        "                       \"node\" : \"number_literal\",\n               "
        "           \"root\" : 6\n                        },\n                 "
        "       \"node\" : \"relation_expression\",\n                        "
        "\"right\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 6\n         "
        "               },\n                        \"root\" : [\"*\"]\n       "
        "               }\n                    },\n                    "
        "\"root\" : [\"+\"]\n                  },\n                  \"root\" "
        ": [\"=\", null]\n                }");

    obj["evaluated_2"] = json::JSON::Load(
        "{\n                  \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n                  "
        "},\n                  \"node\" : \"assignment_expression\",\n         "
        "         \"right\" : {\n                    \"left\" : {\n            "
        "          \"node\" : \"evaluated_expression\",\n                      "
        "\"root\" : {\n                        \"left\" : {\n                  "
        "        \"node\" : \"number_literal\",\n                          "
        "\"root\" : 5\n                        },\n                        "
        "\"node\" : \"relation_expression\",\n                        "
        "\"right\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 6\n         "
        "               },\n                        \"root\" : [\"+\"]\n       "
        "               }\n                    },\n                    "
        "\"node\" : \"relation_expression\",\n                    \"right\" : "
        "{\n                      \"node\" : \"evaluated_expression\",\n       "
        "               \"root\" : {\n                        \"left\" : {\n   "
        "                       \"node\" : \"number_literal\",\n               "
        "           \"root\" : 5\n                        },\n                 "
        "       \"node\" : \"relation_expression\",\n                        "
        "\"right\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 6\n         "
        "               },\n                        \"root\" : [\"+\"]\n       "
        "               }\n                    },\n                    "
        "\"root\" : [\"*\"]\n                  },\n                  \"root\" "
        ": [\"=\", null]\n                }");
    obj["evaluated_3"] = json::JSON::Load(
        "{\n                  \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n                  "
        "},\n                  \"node\" : \"assignment_expression\",\n         "
        "         \"right\" : {\n                    \"left\" : {\n            "
        "          \"node\" : \"evaluated_expression\",\n                      "
        "\"root\" : {\n                        \"left\" : {\n                  "
        "        \"node\" : \"number_literal\",\n                          "
        "\"root\" : 5\n                        },\n                        "
        "\"node\" : \"relation_expression\",\n                        "
        "\"right\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 5\n         "
        "               },\n                        \"root\" : [\"+\"]\n       "
        "               }\n                    },\n                    "
        "\"node\" : \"relation_expression\",\n                    \"right\" : "
        "{\n                      \"node\" : \"evaluated_expression\",\n       "
        "               \"root\" : {\n                        \"left\" : {\n   "
        "                       \"node\" : \"number_literal\",\n               "
        "           \"root\" : 6\n                        },\n                 "
        "       \"node\" : \"relation_expression\",\n                        "
        "\"right\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 6\n         "
        "               },\n                        \"root\" : [\"+\"]\n       "
        "               }\n                    },\n                    "
        "\"root\" : [\"*\"]\n                  },\n                  \"root\" "
        ": [\"=\", null]\n                }");

    RValue_Parser parser{ obj };
    RValue::Value null = { std::monostate(), credence::type::Type_["null"] };
    parser.symbols_.table_.emplace("x", null);
    parser.symbols_.table_.emplace("double", null);
    parser.symbols_.table_.emplace("exp", null);
    parser.symbols_.table_.emplace("puts", null);
    parser.symbols_.table_.emplace("y", null);

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
    std::string evaluated_expected =
        "x (5:int:4) (5:int:4) * (6:int:4) (6:int:4) * + = ";
    std::string evaluated_expected_2 =
        "x (5:int:4) (6:int:4) + (5:int:4) (6:int:4) + * = ";

    std::vector<type::RValue::Type_Pointer> rvalues{};
    RValue_Queue list{};
    std::string test{};

    rvalues.push_back(std::make_shared<type::RValue::Type>(
        parser.from_rvalue(obj["complex"]).value));
    rvalues_to_queue(rvalues, &list);
    test = util::queue_of_rvalues_to_string(&list);
    CHECK(test == complex_expected);
    rvalues.clear();
    list.clear();

    rvalues.push_back(std::make_shared<type::RValue::Type>(
        parser.from_rvalue(obj["unary"]).value));
    rvalues_to_queue(rvalues, &list);
    test = util::queue_of_rvalues_to_string(&list);
    CHECK(test == unary_expected);
    rvalues.clear();
    list.clear();

    rvalues.push_back(std::make_shared<type::RValue::Type>(
        parser.from_rvalue(obj["equal"]).value));
    rvalues_to_queue(rvalues, &list);
    test = util::queue_of_rvalues_to_string(&list);
    CHECK(test == equal_expected);
    rvalues.clear();
    list.clear();

    rvalues.push_back(std::make_shared<type::RValue::Type>(
        parser.from_rvalue(obj["unary_relation"]).value));
    rvalues_to_queue(rvalues, &list);
    test = util::queue_of_rvalues_to_string(&list);
    CHECK(test == unary_relation_expected);
    rvalues.clear();
    list.clear();

    rvalues.push_back(std::make_shared<type::RValue::Type>(
        parser.from_rvalue(obj["ternary"]).value));
    rvalues_to_queue(rvalues, &list);
    test = util::queue_of_rvalues_to_string(&list);
    CHECK(test == ternary_expected);
    rvalues.clear();
    list.clear();

    rvalues.push_back(std::make_shared<type::RValue::Type>(
        parser.from_rvalue(obj["function"]).value));
    rvalues_to_queue(rvalues, &list);
    test = util::queue_of_rvalues_to_string(&list);
    CHECK(test == function_expected);
    rvalues.clear();
    list.clear();

    rvalues.push_back(std::make_shared<type::RValue::Type>(
        parser.from_rvalue(obj["evaluated"]).value));
    rvalues_to_queue(rvalues, &list);
    test = util::queue_of_rvalues_to_string(&list);
    CHECK(test == evaluated_expected);
    rvalues.clear();
    list.clear();

    rvalues.push_back(std::make_shared<type::RValue::Type>(
        parser.from_rvalue(obj["evaluated_2"]).value));
    rvalues_to_queue(rvalues, &list);
    test = util::queue_of_rvalues_to_string(&list);
    CHECK(test == evaluated_expected_2);
    rvalues.clear();
    list.clear();

    rvalues.push_back(std::make_shared<type::RValue::Type>(
        parser.from_rvalue(obj["evaluated_3"]).value));
    rvalues_to_queue(rvalues, &list);
    rvalues.clear();
    list.clear();
}
