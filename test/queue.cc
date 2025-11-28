#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase, TEST_CASE

#include <credence/expression.h> // for Expression_Parser
#include <credence/queue.h>  // for rvalues_to_queue, queue_of_rvalues_to_s...
#include <credence/symbol.h> // for Symbol_Table
#include <credence/values.h> // for RValue, Type_
#include <map>               // for map
#include <simplejson.h>      // for JSON
#include <string>            // for operator==, basic_string, operator<<
#include <utility>           // for pair
#include <variant>           // for monostate
#include <vector>            // for vector

TEST_CASE("ir/queue.cc: rvalues_to_queue")
{
    using namespace credence;
    using namespace credence::type;
    credence::util::AST_Node obj;

    obj["complex"] = credence::util::AST_Node::load(
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
    obj["unary"] = credence::util::AST_Node::load(
        "{\n  \"left\": {\n    \"node\": \"number_literal\",\n    \"root\": "
        "5\n  },\n  \"node\": \"unary_expression\",\n  \"root\": [\n    "
        "\"~\"\n  ]\n}");
    obj["equal"] = credence::util::AST_Node::load(
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
    obj["unary_relation"] = credence::util::AST_Node::load(
        "{\n                  \"left\" : {\n                    \"left\" : {\n "
        "                     \"node\" : \"number_literal\",\n                 "
        "     \"root\" : 5\n                    },\n                    "
        "\"node\" : \"unary_expression\",\n                    \"root\" : "
        "[\"~\"]\n                  },\n                  \"node\" : "
        "\"relation_expression\",\n                  \"right\" : {\n           "
        "         \"node\" : \"number_literal\",\n                    \"root\" "
        ": 2\n                  },\n                  \"root\" : [\"^\"]\n     "
        "           }");
    obj["ternary"] = credence::util::AST_Node::load(
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
    obj["function"] = credence::util::AST_Node::load(
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
    obj["evaluated"] = credence::util::AST_Node::load(
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

    obj["evaluated_2"] = credence::util::AST_Node::load(
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
    obj["evaluated_3"] = credence::util::AST_Node::load(
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
    obj["functions"] = credence::util::AST_Node::load(
        "{\n                  \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n                  "
        "},\n                  \"node\" : \"assignment_expression\",\n         "
        "         \"right\" : {\n                    \"left\" : {\n            "
        "          \"node\" : \"lvalue\",\n                      \"root\" : "
        "\"exp\"\n                    },\n                    \"node\" : "
        "\"function_expression\",\n                    \"right\" : [{\n        "
        "                \"left\" : {\n                          \"node\" : "
        "\"lvalue\",\n                          \"root\" : \"exp\"\n           "
        "             },\n                        \"node\" : "
        "\"function_expression\",\n                        \"right\" : [{\n    "
        "                        \"node\" : \"number_literal\",\n              "
        "              \"root\" : 1\n                          }, {\n          "
        "                  \"node\" : \"number_literal\",\n                    "
        "        \"root\" : 2\n                          }],\n                 "
        "       \"root\" : \"exp\"\n                      }, {\n               "
        "         \"left\" : {\n                          \"node\" : "
        "\"lvalue\",\n                          \"root\" : \"sub\"\n           "
        "             },\n                        \"node\" : "
        "\"function_expression\",\n                        \"right\" : [{\n    "
        "                        \"node\" : \"number_literal\",\n              "
        "              \"root\" : 1\n                          }, {\n          "
        "                  \"node\" : \"number_literal\",\n                    "
        "        \"root\" : 2\n                          }],\n                 "
        "       \"root\" : \"sub\"\n                      }],\n                "
        "    \"root\" : \"exp\"\n                  },\n                  "
        "\"root\" : [\"=\", null]\n                }");

    Expression_Parser parser{ obj };
    value::Literal null = value::Expression::NULL_LITERAL;
    parser.symbols_.table_.emplace("x", null);
    parser.symbols_.table_.emplace("double", null);
    parser.symbols_.table_.emplace("exp", null);
    parser.symbols_.table_.emplace("sub", null);
    parser.symbols_.table_.emplace("puts", null);
    parser.symbols_.table_.emplace("y", null);

    std::string complex_expected =
        "(5:int:4) (5:int:4) exp _p1 (2:int:4) = _p2 (5:int:4) = _p1 _p2 PUSH "
        "PUSH CALL (4:int:4) (2:int:4) ^ ~ / + * ";
    std::string unary_expected = "(5:int:4) ~ ";
    std::string equal_expected = "x (5:int:4) (5:int:4) + = ";
    std::string unary_relation_expected = "(5:int:4) ~ (2:int:4) ^ ";
    std::string ternary_expected =
        "x (10:int:4) (1:int:4) (5:int:4) (4:int:4) < PUSH ?: = ";
    std::string function_expected =
        "puts _p1 (1:int:4) = _p2 (2:int:4) = _p3 (3:int:4) = _p1 _p2 _p3 PUSH "
        "PUSH PUSH CALL ";
    std::string evaluated_expected =
        "x (5:int:4) (5:int:4) * (6:int:4) (6:int:4) * + = ";
    std::string evaluated_expected_2 =
        "x (5:int:4) (6:int:4) + (5:int:4) (6:int:4) + * = ";
    std::string functions_expected =
        "x exp _p1 exp _p2 (1:int:4) = _p3 (2:int:4) = _p2 _p3 PUSH PUSH CALL "
        "= _p4 sub _p5 (1:int:4) = _p6 (2:int:4) = _p5 _p6 PUSH PUSH CALL = "
        "_p1 _p4 PUSH PUSH CALL = ";

    std::vector<value::Array> rvalues{};
    auto expressions = queue::Expressions{};
    std::unique_ptr<queue::Queue> queue{};
    std::string test{};

    expressions.emplace_back(
        value::make_value_type_pointer(
            parser.parse_from_node(obj["complex"]).value));
    queue = queue::make_queue_from_expression_operands(expressions);
    test = queue::queue_of_expressions_to_string(*queue);
    CHECK(test == complex_expected);
    expressions.clear();

    expressions.emplace_back(
        value::make_value_type_pointer(
            parser.parse_from_node(obj["unary"]).value));
    queue = queue::make_queue_from_expression_operands(expressions);
    test = queue::queue_of_expressions_to_string(*queue);
    CHECK(test == unary_expected);
    expressions.clear();

    expressions.emplace_back(
        value::make_value_type_pointer(
            parser.parse_from_node(obj["equal"]).value));
    queue = queue::make_queue_from_expression_operands(expressions);
    test = queue::queue_of_expressions_to_string(*queue);
    CHECK(test == equal_expected);
    expressions.clear();

    expressions.emplace_back(
        value::make_value_type_pointer(
            parser.parse_from_node(obj["unary_relation"]).value));

    queue = queue::make_queue_from_expression_operands(expressions);
    test = queue::queue_of_expressions_to_string(*queue);
    CHECK(test == unary_relation_expected);
    expressions.clear();

    expressions.emplace_back(
        value::make_value_type_pointer(
            parser.parse_from_node(obj["ternary"]).value));

    queue = queue::make_queue_from_expression_operands(expressions);
    test = queue::queue_of_expressions_to_string(*queue);
    CHECK(test == ternary_expected);
    expressions.clear();

    expressions.emplace_back(
        value::make_value_type_pointer(
            parser.parse_from_node(obj["function"]).value));

    queue = queue::make_queue_from_expression_operands(expressions);
    test = queue::queue_of_expressions_to_string(*queue);
    CHECK(test == function_expected);
    expressions.clear();

    expressions.emplace_back(
        value::make_value_type_pointer(
            parser.parse_from_node(obj["evaluated"]).value));
    queue = queue::make_queue_from_expression_operands(expressions);
    test = queue::queue_of_expressions_to_string(*queue);
    CHECK(test == evaluated_expected);
    expressions.clear();

    expressions.emplace_back(
        value::make_value_type_pointer(
            parser.parse_from_node(obj["evaluated_2"]).value));

    queue = queue::make_queue_from_expression_operands(expressions);
    test = queue::queue_of_expressions_to_string(*queue);
    CHECK(test == evaluated_expected_2);
    expressions.clear();

    expressions.emplace_back(
        value::make_value_type_pointer(
            parser.parse_from_node(obj["functions"]).value));

    queue = queue::make_queue_from_expression_operands(expressions);
    test = queue::queue_of_expressions_to_string(*queue);
    CHECK(test == functions_expected);
    expressions.clear();
}
