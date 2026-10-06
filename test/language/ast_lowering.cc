#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase

#include <credence/language/ast_lowering.h> // for AST_Lowering
#include <credence/language/hir.h>          // for Data_Kind
#include <credence/language/literal.h>      // for Literal, NULL_LITERAL
#include <credence/symbol.h>                // for Symbol_Table
#include <deque>                            // for deque
#include <easyjson.h>                       // for JSON
#include <map>                              // for map
#include <memory>                           // for shared_ptr
#include <string>                           // for basic_string, string
#include <tuple>                            // for get, tie
#include <utility>                          // for pair, make_pair, get
#include <variant>                          // for get, monostate

using namespace credence;
using namespace credence::language::type;

struct Fixture
{
    credence::util::AST_Node lvalue_ast_node_json;
    credence::util::AST_Node assignment_symbol_table;

    Fixture()
    {
        assignment_symbol_table = credence::util::AST_Node::load(
            "{\n  \"main\" : {\n    \"column\" : 1,\n    \"end_column\" : "
            "5,\n "
            "   "
            "\"end_pos\" : 4,\n    \"line\" : 1,\n    \"start_pos\" : "
            "0,\n    "
            "\"type\" : \"function_definition\"\n  },\n  \"x\" : {\n    "
            "\"column\" "
            ": 3,\n    \"end_column\" : 4,\n    \"end_pos\" : 13,\n    "
            "\"line\" : "
            "2,\n    \"start_pos\" : 12,\n    \"type\" : "
            "\"integer_literal\"\n  "
            "}\n}");
        lvalue_ast_node_json = credence::util::AST_Node::load(
            " {\n        \"left\" : [{\n            \"left\" : [{\n       "
            "     "
            "    "
            "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n   "
            "     "
            "    "
            "  }, {\n                \"node\" : \"lvalue\",\n             "
            "   "
            "\"root\" : \"y\"\n              }, {\n                "
            "\"node\" : "
            "\"lvalue\",\n                \"root\" :\"z\"\n              "
            "}],\n "
            "   "
            "        \"node\" : \"statement\",\n            \"root\" : "
            "\"auto\"\n  "
            "        }, {\n            \"left\" : [[{\n                  "
            "\"left\" "
            ": {\n                    \"node\" : \"lvalue\",\n            "
            "     "
            "   "
            "\"root\" : \"x\"\n                  },\n                  "
            "\"node\" : "
            "\"assignment_expression\",\n                  \"right\" : "
            "{\n     "
            "    "
            "           \"node\" : \"integer_literal\",\n                  "
            "  "
            "\"root\" : 5\n                  },\n                  "
            "\"root\" : "
            "[\"=\", null]\n                }]],\n            \"node\" : "
            "\"statement\",\n            \"root\" : \"rvalue\"\n          "
            "}]\n "
            "}\n ");
    }
    ~Fixture() = default;
};

TEST_CASE("ast_lowering.cc: AST_Lowering::lower_from_node")
{
    credence::util::AST_Node obj;
    using std::get;
    obj["test"] = credence::util::AST_Node::load(
        "[{\n                  \"node\" : \"constant_literal\",\n         "
        "     "
        "    \"root\" : \"x\"\n                }, {\n                  "
        "\"node\" : \"integer_literal\",\n                  \"root\" : "
        "10\n     "
        "           }, {\n                  \"node\" : "
        "\"string_literal\",\n   "
        "               \"root\" : \"\\\"hello world\\\"\"\n              "
        "  }, "
        "{\n                  \"left\" : {\n                    \"node\" "
        ": "
        "\"lvalue\",\n                    \"root\" : \"x\"\n              "
        "    "
        "},\n                  \"node\" : \"assignment_expression\",\n    "
        "     "
        "         \"right\" : {\n                    \"node\" : "
        "\"integer_literal\",\n                    \"root\" : 5\n          "
        "     "
        "   },\n                  \"root\" : [\"=\", null]\n              "
        "  }, "
        "{\n                  \"node\" : \"evaluated_expression\",\n      "
        "     "
        "       \"root\" : {\n                    \"left\" : {\n          "
        "     "
        "       \"node\" : \"lvalue\",\n                      \"root\" : "
        "\"putchar\"\n                    },\n                    "
        "\"node\" : "
        "\"function_expression\",\n                    \"right\" : [{\n   "
        "     "
        "                \"node\" : \"lvalue\",\n                        "
        "\"root\" : \"x\"\n                      }],\n                    "
        "\"root\" : \"putchar\"\n                  }\n                }, "
        "{\n   "
        "               \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"getchar\"\n        "
        "     "
        "     },\n                  \"node\" : \"function_expression\",\n "
        "     "
        "            \"right\" : [{\n                      \"node\" : "
        "\"lvalue\",\n                      \"root\" : \"c\"\n            "
        "     "
        "   }],\n                  \"root\" : \"getchar\"\n               "
        " }, "
        "{\n                  \"left\" : {\n                    \"node\" "
        ": "
        "\"lvalue\",\n                    \"root\" : \"x\"\n              "
        "    "
        "},\n                  \"node\" : \"relation_expression\",\n      "
        "     "
        "       \"right\" : {\n                    \"node\" : "
        "\"integer_literal\",\n                    \"root\" : 5\n          "
        "     "
        "   },\n                  \"root\" : [\"<\"]\n                }, "
        "{\n   "
        "               \"node\" : \"post_inc_dec_expression\",\n         "
        "     "
        "    \"right\" : {\n                    \"node\" : \"lvalue\",\n  "
        "     "
        "             \"root\" : \"x\"\n                  },\n            "
        "     "
        " \"root\" : [\"++\"]\n                }, {\n                  "
        "\"node\" : \"post_inc_dec_expression\",\n                  "
        "\"right\" "
        ": {\n                    \"node\" : \"lvalue\",\n                "
        "    "
        "\"root\" : \"x\"\n                  },\n                  "
        "\"root\" : "
        "[\"--\"]\n                }, {\n                  \"left\" : {\n "
        "     "
        "              \"node\" : \"integer_literal\",\n                   "
        " "
        "\"root\" : 5\n                  },\n                  \"node\" : "
        "\"unary_expression\",\n                  \"root\" : [\"~\"]\n    "
        "     "
        "       }, {\n                  \"left\" : {\n                    "
        "\"node\" : \"evaluated_expression\",\n                    "
        "\"root\" : "
        "{\n                      \"left\" : {\n                        "
        "\"node\" : \"integer_literal\",\n                        \"root\" "
        ": "
        "5\n                      },\n                      \"node\" : "
        "\"unary_expression\",\n                      \"root\" : "
        "[\"~\"]\n     "
        "               }\n                  },\n                  "
        "\"node\" : "
        "\"relation_expression\",\n                  \"right\" : {\n      "
        "     "
        "         \"node\" : \"integer_literal\",\n                    "
        "\"root\" "
        ": 10\n                  },\n                  \"root\" : "
        "[\"^\"]\n    "
        "            }]");
    auto temp = language::AST_Lowering(obj);
    credence::language::literal::Literal null = language::literal::NULL_LITERAL;

    temp.symbols_.table_.emplace("x", null);
    temp.symbols_.table_.emplace("c", null);
    temp.symbols_.table_.emplace("putchar", null);
    temp.symbols_.table_.emplace("getchar", null);
    // check all
    for (auto& rvalue : obj["test"].array_range()) {
        CHECK_NOTHROW(temp.lower_from_node(rvalue));
    }
}

TEST_CASE("ast_lowering.cc: AST_Lowering::function_expression")
{
    credence::util::AST_Node obj;
    using std::get;
    obj["test"] = credence::util::AST_Node::load(
        "{\n                  \"left\" : {\n                    \"node\" "
        ": "
        "\"lvalue\",\n                    \"root\" : \"putchar\"\n        "
        "     "
        "     },\n                  \"node\" : \"function_expression\",\n "
        "     "
        "            \"right\" : [{\n                      \"node\" : "
        "\"lvalue\",\n                      \"root\" : \"x\"\n            "
        "     "
        "   }, {\n                      \"node\" : \"lvalue\",\n          "
        "     "
        "       \"root\" : \"y\"\n                    }, {\n              "
        "     "
        "   \"node\" : \"lvalue\",\n                      \"root\" : "
        "\"z\"\n   "
        "                 }]\n}\n                    ");
    auto temp = language::AST_Lowering(obj);
    credence::language::literal::Literal null = language::literal::NULL_LITERAL;

    temp.symbols_.table_.emplace("x", null);
    temp.symbols_.table_.emplace("y", null);
    temp.symbols_.table_.emplace("putchar", null);
    temp.symbols_.table_.emplace("z", null);

    auto test1 = temp.from_function_expression_node(obj["test"]);
    auto function =
        get<credence::language::hir::Data_Kind::Function>(test1.value);
    // parameters
    CHECK(get<credence::language::hir::Data_Kind::LValue>(
              function.second[0]->value)
              .first == "x");
    CHECK(get<credence::language::hir::Data_Kind::LValue>(
              function.second[1]->value)
              .first == "y");
    CHECK(get<credence::language::hir::Data_Kind::LValue>(
              function.second[2]->value)
              .first == "z");
}

TEST_CASE("ast_lowering.cc: AST_Lowering::evaluated_expression")
{
    credence::util::AST_Node obj;
    using std::get;
    obj["test"] = credence::util::AST_Node::load(
        "[{\n                  \"node\" : \"evaluated_expression\",\n     "
        "     "
        "        \"root\" : {\n                    \"left\" : {\n         "
        "     "
        "        \"node\" : \"integer_literal\",\n                      "
        "\"root\" : 5\n                    },\n                    "
        "\"node\" : "
        "\"relation_expression\",\n                    \"right\" : {\n    "
        "     "
        "             \"node\" : \"integer_literal\",\n                    "
        "  "
        "\"root\" : 5\n                    },\n                    "
        "\"root\" : "
        "[\"*\"]\n                  }\n                }, {\n             "
        "     "
        "\"node\" : \"evaluated_expression\",\n                  \"root\" "
        ": "
        "{\n                    \"left\" : {\n                      "
        "\"node\" : "
        "\"lvalue\",\n                      \"root\" : \"x\"\n            "
        "     "
        "   },\n                    \"node\" : \"indirect_lvalue\",\n     "
        "     "
        "          \"root\" : [\"*\"]\n                  }\n              "
        "  "
        "}]");
    auto temp = language::AST_Lowering(obj);
    credence::language::literal::Literal null = language::literal::NULL_LITERAL;

    temp.symbols_.table_.emplace("x", null);

    auto expressions = obj["test"].to_deque();
    auto test1 = temp.from_evaluated_expression_node(expressions.at(0));
    auto expr1 = get<credence::language::hir::Data_Kind::Pointer>(test1.value);
    CHECK(get<credence::language::hir::Data_Kind::Relation>(
              get<credence::language::hir::Data_Kind::Pointer>(expr1->value)
                  ->value)
              .first == Operator::B_MUL);
    auto test2 = temp.from_evaluated_expression_node(expressions.at(1));
    auto expr2 = get<credence::language::hir::Data_Kind::Pointer>(test2.value);
    CHECK(get<credence::language::hir::Data_Kind::LValue>(expr2->value).first ==
          "*x");
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_relation_expression")
{
    credence::util::AST_Node obj;
    obj["test"] = credence::util::AST_Node::load(
        "[\n  {\n    \"left\": {\n      \"node\": \"lvalue\",\n      "
        "\"root\": "
        "\"x\"\n    },\n    \"node\": \"relation_expression\",\n    "
        "\"right\": "
        "{\n      \"node\": \"integer_literal\",\n      \"root\": 10\n    "
        "},\n  "
        "  \"root\": [\n      \"*\"\n    ]\n  },\n  {\n    \"left\": {\n  "
        " "
        "   "
        "\"node\": \"lvalue\",\n      \"root\": \"x\"\n    },\n    "
        "\"node\": "
        "\"relation_expression\",\n    \"right\": {\n      \"left\": {\n  "
        " "
        "    "
        " \"node\": \"integer_literal\",\n        \"root\": 10\n      },\n "
        " "
        "    "
        "\"node\": \"ternary_expression\",\n      \"right\": {\n        "
        "\"node\": \"integer_literal\",\n        \"root\": 1\n      },\n   "
        " "
        "  "
        "\"root\": {\n        \"node\": \"integer_literal\",\n        "
        "\"root\": "
        "5\n      }\n    },\n    \"root\": [\n      \"<=\"\n    ]\n  },\n "
        " "
        "{\n "
        "   \"left\": {\n      \"node\": \"lvalue\",\n      \"root\": "
        "\"x\"\n  "
        "  },\n    \"node\": \"relation_expression\",\n    \"right\": {\n "
        " "
        "    "
        "\"node\": \"integer_literal\",\n      \"root\": 5\n    },\n    "
        "\"root\": [\n      \"==\"\n    ]\n  },\n  {\n    \"left\": {\n   "
        " "
        "  "
        "\"node\": \"lvalue\",\n      \"root\": \"x\"\n    },\n    "
        "\"node\": "
        "\"relation_expression\",\n    \"right\": {\n      \"node\": "
        "\"integer_literal\",\n      \"root\": 5\n    },\n    \"root\": "
        "[\n "
        "    "
        " \"!=\"\n    ]\n  },\n  {\n    \"left\": {\n      \"node\": "
        "\"lvalue\",\n      \"root\": \"x\"\n    },\n    \"node\": "
        "\"relation_expression\",\n    \"right\": {\n      \"node\": "
        "\"integer_literal\",\n      \"root\": 0\n    },\n    \"root\": "
        "[\n "
        "    "
        " \"^\"\n    ]\n  },\n  {\n    \"left\": {\n      \"node\": "
        "\"lvalue\",\n      \"root\": \"x\"\n    },\n    \"node\": "
        "\"relation_expression\",\n    \"right\": {\n      \"node\": "
        "\"integer_literal\",\n      \"root\": 5\n    },\n    \"root\": "
        "[\n "
        "    "
        " \"<\"\n    ]\n  },\n  {\n    \"left\": {\n      \"node\": "
        "\"lvalue\",\n      \"root\": \"x\"\n    },\n    \"node\": "
        "\"relation_expression\",\n    \"right\": {\n      \"node\": "
        "\"integer_literal\",\n      \"root\": 10\n    },\n    \"root\": "
        "[\n    "
        "  \"<=\"\n    ]\n  }\n]");
    auto temp = language::AST_Lowering(obj);
    credence::language::literal::Literal null = language::literal::NULL_LITERAL;

    std::vector<credence::language::hir::Data_Kind::Pointer> arguments{};
    temp.symbols_.table_.emplace("x", null);

    auto relation_expressions = obj["test"].to_deque();

    // forgive me
    using std::get;

    auto test1 = temp.from_relation_expression_node(relation_expressions.at(0));
    CHECK(std::get<credence::language::hir::Data_Kind::Relation>(test1.value)
              .first == Operator::B_MUL);
    arguments =
        std::get<credence::language::hir::Data_Kind::Relation>(test1.value)
            .second;
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(
              arguments[0]->value)
              .first == "x");
    CHECK(std::get<int>(std::get<credence::language::literal::Literal>(
              arguments[1]->value)
                  .first) == 10);

    // ternary relation test
    auto test2 = temp.from_relation_expression_node(relation_expressions.at(1));
    CHECK(std::get<credence::language::hir::Data_Kind::Relation>(test2.value)
              .first == Operator::R_LE);
    arguments = std::move(
        std::get<credence::language::hir::Data_Kind::Relation>(test2.value)
            .second);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(
              arguments[0]->value)
              .first == "x");
    CHECK(
        get<int>(get<credence::language::literal::Literal>(arguments[1]->value)
                .first) == 5);
    CHECK(
        get<int>(get<credence::language::literal::Literal>(arguments[2]->value)
                .first) == 10);
    CHECK(
        get<int>(get<credence::language::literal::Literal>(arguments[3]->value)
                .first) == 1);

    auto test3 = temp.from_relation_expression_node(relation_expressions.at(2));
    CHECK(std::get<credence::language::hir::Data_Kind::Relation>(test3.value)
              .first == Operator::R_EQUAL);
    arguments = std::move(
        std::get<credence::language::hir::Data_Kind::Relation>(test3.value)
            .second);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(
              arguments[0]->value)
              .first == "x");
    CHECK(std::get<int>(std::get<credence::language::literal::Literal>(
              arguments[1]->value)
                  .first) == 5);

    auto test4 = temp.from_relation_expression_node(relation_expressions.at(3));
    CHECK(std::get<credence::language::hir::Data_Kind::Relation>(test4.value)
              .first == Operator::R_NEQUAL);
    arguments = std::move(
        std::get<credence::language::hir::Data_Kind::Relation>(test4.value)
            .second);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(
              arguments[0]->value)
              .first == "x");
    CHECK(std::get<int>(std::get<credence::language::literal::Literal>(
              arguments[1]->value)
                  .first) == 5);

    auto test5 = temp.from_relation_expression_node(relation_expressions.at(4));
    CHECK(std::get<credence::language::hir::Data_Kind::Relation>(test5.value)
              .first == Operator::XOR);
    arguments = std::move(
        std::get<credence::language::hir::Data_Kind::Relation>(test5.value)
            .second);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(
              arguments[0]->value)
              .first == "x");
    CHECK(std::get<int>(std::get<credence::language::literal::Literal>(
              arguments[1]->value)
                  .first) == 0);

    auto test6 = temp.from_relation_expression_node(relation_expressions.at(5));
    CHECK(std::get<credence::language::hir::Data_Kind::Relation>(test6.value)
              .first == Operator::R_LT);
    arguments = std::move(
        std::get<credence::language::hir::Data_Kind::Relation>(test6.value)
            .second);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(
              arguments[0]->value)
              .first == "x");
    CHECK(std::get<int>(std::get<credence::language::literal::Literal>(
              arguments[1]->value)
                  .first) == 5);

    auto test7 = temp.from_relation_expression_node(relation_expressions.at(6));
    CHECK(std::get<credence::language::hir::Data_Kind::Relation>(test7.value)
              .first == Operator::R_LE);
    arguments = std::move(
        std::get<credence::language::hir::Data_Kind::Relation>(test7.value)
            .second);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(
              arguments[0]->value)
              .first == "x");
    CHECK(std::get<int>(std::get<credence::language::literal::Literal>(
              arguments[1]->value)
                  .first) == 10);
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_unary_expression")
{
    credence::util::AST_Node obj;
    obj["test"] = credence::util::AST_Node::load(
        "[{\n                  \"node\" : \"post_inc_dec_expression\",\n  "
        " "
        "    "
        "           \"right\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n              "
        " "
        "   "
        "},\n                  \"root\" : [\"++\"]\n                }, "
        "{\n "
        "    "
        "             \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n              "
        " "
        "   "
        "},\n                  \"node\" : \"pre_inc_dec_expression\",\n   "
        " "
        "    "
        "          \"root\" : [\"++\"]\n                }, {\n            "
        " "
        "    "
        " \"left\" : {\n                    \"node\" : \"lvalue\",\n      "
        " "
        "    "
        "         \"root\" : \"x\"\n                  },\n                "
        " "
        " "
        "\"node\" : \"address_of_expression\",\n                  "
        "\"root\" "
        ": "
        "[\"&\"]\n                }, {\n                  \"left\" : {\n  "
        " "
        "    "
        "             \"node\" : \"integer_literal\",\n                    "
        "\"root\" : 5\n                  },\n                  \"node\" : "
        "\"unary_expression\",\n                  \"root\" : [\"~\"]\n    "
        " "
        "    "
        "       }, {\n                  \"left\" : {\n                    "
        "\"node\" : \"lvalue\",\n                    \"root\" : \"x\"\n   "
        " "
        "    "
        "          },\n                  \"node\" : "
        "\"indirect_lvalue\",\n "
        "    "
        "             \"root\" : [\"*\"]\n                }, {\n          "
        " "
        "    "
        "     \"left\" : {\n                      \"node\" : "
        "\"integer_literal\",\n                      \"root\" : 5\n        "
        " "
        "    "
        "       },\n                    \"node\" : "
        "\"unary_expression\",\n "
        "    "
        "               \"root\" : [\"-\"]\n                  },\n        "
        " "
        "    "
        "   {\n                  \"left\" : {\n                    "
        "\"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n              "
        " "
        "   "
        "},\n                  \"node\" : \"unary_expression\",\n         "
        " "
        "    "
        "    \"root\" : [\"!\"]\n                }]");
    auto temp = language::AST_Lowering(obj);

    credence::language::literal::Literal null = language::literal::NULL_LITERAL;

    temp.symbols_.table_.emplace("x", null);

    auto unary_expressions = obj["test"].to_deque();
    std::shared_ptr<credence::language::hir::Data_Kind> lvalue, constant;

    auto test1 = temp.from_unary_expression_node(unary_expressions.at(0));
    lvalue =
        std::get<credence::language::hir::Data_Kind::Unary>(test1.value).second;
    CHECK(std::get<credence::language::hir::Data_Kind::Unary>(test1.value)
              .first == Operator::POST_INC);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(lvalue->value)
              .first == "x");

    auto test2 = temp.from_unary_expression_node(unary_expressions.at(1));
    lvalue =
        std::get<credence::language::hir::Data_Kind::Unary>(test2.value).second;
    CHECK(std::get<credence::language::hir::Data_Kind::Unary>(test2.value)
              .first == Operator::PRE_INC);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(lvalue->value)
              .first == "x");

    auto test3 = temp.from_unary_expression_node(unary_expressions.at(2));
    lvalue =
        std::get<credence::language::hir::Data_Kind::Unary>(test3.value).second;
    CHECK(std::get<credence::language::hir::Data_Kind::Unary>(test3.value)
              .first == Operator::U_ADDR_OF);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(lvalue->value)
              .first == "x");

    auto test4 = temp.from_unary_expression_node(unary_expressions.at(3));
    constant =
        std::get<credence::language::hir::Data_Kind::Unary>(test4.value).second;
    CHECK(std::get<credence::language::hir::Data_Kind::Unary>(test4.value)
              .first == Operator::U_ONES_COMPLEMENT);
    CHECK(std::get<int>(
              std::get<credence::language::literal::Literal>(constant->value)
                  .first) == 5);

    auto test5 = temp.from_unary_expression_node(unary_expressions.at(4));
    lvalue =
        std::get<credence::language::hir::Data_Kind::Unary>(test5.value).second;
    CHECK(std::get<credence::language::hir::Data_Kind::Unary>(test5.value)
              .first == Operator::U_INDIRECTION);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(lvalue->value)
              .first == "x");

    auto test6 = temp.from_unary_expression_node(unary_expressions.at(5));
    constant =
        std::get<credence::language::hir::Data_Kind::Unary>(test6.value).second;
    CHECK(std::get<credence::language::hir::Data_Kind::Unary>(test6.value)
              .first == Operator::U_MINUS);
    CHECK(std::get<int>(
              std::get<credence::language::literal::Literal>(constant->value)
                  .first) == 5);

    auto test7 = temp.from_unary_expression_node(unary_expressions.at(6));
    lvalue =
        std::get<credence::language::hir::Data_Kind::Unary>(test7.value).second;
    CHECK(std::get<credence::language::hir::Data_Kind::Unary>(test7.value)
              .first == Operator::U_NOT);
    CHECK(std::get<credence::language::hir::Data_Kind::LValue>(lvalue->value)
              .first == "x");
}

TEST_CASE_FIXTURE(Fixture,
    "ast_lowering.cc: AST_Lowering::from_assignment_expression")
{
    credence::util::AST_Node obj;
    obj["symbols"] = assignment_symbol_table;
    obj["test"] = credence::util::AST_Node::load(
        "{\n                  \"left\" : {\n                    \"node\" "
        ": "
        "\"lvalue\",\n                    \"root\" : \"x\"\n              "
        " "
        "   "
        "},\n                  \"node\" : \"assignment_expression\",\n    "
        " "
        "    "
        "         \"right\" : {\n                    \"node\" : "
        "\"integer_literal\",\n                    \"root\" : 5\n          "
        " "
        "    "
        "   },\n                  \"root\" : [\"=\", null]\n              "
        " "
        " }");
    auto temp = language::AST_Lowering(obj["symbols"]);
    // no declaration with `auto' or `extern', should throw
    CHECK_THROWS(temp.from_assignment_expression_node(obj["test"]));

    credence::language::literal::Literal value_type =
        language::literal::NULL_LITERAL;

    credence::language::literal::Literal assigned_type = { 5,
        language::literal::TYPE_LITERAL.at("int") };

    temp.symbols_.table_.emplace("x", value_type);

    auto expr = temp.from_assignment_expression_node(obj["test"]);

    auto lhs =
        std::get<credence::language::hir::Data_Kind::Symbol>(expr.value).first;
    auto* rhs =
        &std::get<credence::language::hir::Data_Kind::Symbol>(expr.value)
             .second;

    CHECK(lhs.first == "x");
    CHECK(lhs.second == value_type);
    CHECK(std::get<credence::language::literal::Literal>((*rhs)->value) ==
          assigned_type);
}

TEST_CASE_FIXTURE(Fixture, "ast_lowering.cc: AST_Lowering::is_symbol")
{
    credence::util::AST_Node obj;
    obj["symbols"] = assignment_symbol_table;
    obj["test"] = credence::util::AST_Node::load("{\"node\":  \"lvalue\","
                                                 "\"root\": \"x\""
                                                 "}");
    auto temp = language::AST_Lowering(obj["test"]);
    // not declared with `auto' or `extern', should throw
    CHECK(temp.is_symbol(obj["test"]) == false);

    auto temp2 = language::AST_Lowering(obj["symbols"]);
    CHECK(temp2.is_symbol(obj["test"]) == false);

    credence::language::literal::Literal value_type =
        language::literal::NULL_LITERAL;
    temp2.symbols_.set_symbol_by_name("x", value_type);
    CHECK(temp2.is_symbol(obj["test"]) == true);
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_lvalue_expression")
{
    credence::util::AST_Node obj;
    obj["test"] = credence::util::AST_Node::load(
        "[\n               {\n                \"left\" : {\n              "
        " "
        "   "
        "\"node\" : \"integer_literal\",\n                  \"root\" : "
        "50\n "
        "    "
        "           },\n                \"node\" : \"vector_lvalue\",\n   "
        " "
        "    "
        "        \"root\" : \"x\"\n              }, {\n                "
        "\"left\" : {\n                  \"node\" : \"lvalue\",\n         "
        " "
        "    "
        "    \"root\" : \"y\"\n                },\n                "
        "\"node\" : "
        "\"indirect_lvalue\",\n                \"root\" : [\"*\"]\n       "
        " "
        "    "
        "  }, {\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"z\"\n              }]");
    auto temp = language::AST_Lowering(obj);
    credence::language::literal::Literal empty_value =
        std::make_pair('0', std::make_pair("byte", 50));
    auto lvalues = obj["test"].to_deque();
    auto [vector, pointer, normal] =
        std::tie(lvalues.at(0), lvalues.at(1), lvalues.at(2));
    temp.symbols_.table_["x"] = empty_value;
    temp.symbols_.table_["y"] = empty_value;
    temp.symbols_.table_["z"] = empty_value;
    // test vector
    auto test1 = temp.from_lvalue_expression_node(vector);
    CHECK(test1.first == "x[50]");
    // test pointer
    auto test2 = temp.from_lvalue_expression_node(pointer);
    CHECK(test2.first == "*y");
    // normal variable
    auto test3 = temp.from_lvalue_expression_node(normal);
    CHECK(test3.first == "z");
    CHECK(test3.second == empty_value);
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_indirect_identifier")
{
    credence::util::AST_Node obj;
    obj["test"] = credence::util::AST_Node::load(
        "{\n                \"left\" : {\n                  \"node\" : "
        "\"lvalue\",\n                  \"root\" : \"x\"\n                "
        "},\n "
        "               \"node\" : \"indirect_lvalue\",\n                "
        "\"root\" : [\"*\"]\n              }");

    auto temp = language::AST_Lowering(obj["test"]);
    CHECK_THROWS(temp.from_indirect_identifier_node(obj["test"]));
    credence::language::literal::Literal value =
        std::make_pair('0', std::make_pair("byte", 50));
    temp.symbols_.table_["x"] = value;
    auto test = temp.from_indirect_identifier_node(obj["test"]);
    CHECK(test == value);
}
TEST_CASE("ast_lowering.cc: AST_Lowering::from_vector_idenfitier")
{
    credence::util::AST_Node obj;
    obj["test"] = credence::util::AST_Node::load(
        "{\n                \"left\" : {\n                  \"node\" : "
        "\"integer_literal\",\n                  \"root\" : 50\n           "
        " "
        "    "
        "},\n                \"node\" : \"vector_lvalue\",\n              "
        " "
        " "
        "\"root\" : \"x\"\n              }");

    auto temp = language::AST_Lowering(obj["test"]);
    CHECK_THROWS(temp.from_vector_idenfitier_node(obj["test"]));
    credence::language::literal::Literal test =
        std::make_pair('0', std::make_pair("byte", 50));
    temp.symbols_.table_["x"] = test;
    CHECK(temp.from_vector_idenfitier_node(obj["test"]) == test);
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_constant_expression")
{
    credence::util::AST_Node obj;
    obj["test"] =
        credence::util::AST_Node::load("{\"node\":  \"integer_literal\","
                                       "\"root\": 10"
                                       "}");

    auto temp = language::AST_Lowering(obj);
    auto data = temp.from_constant_expression_node(obj["test"]);
    auto [value, type] = data;
    CHECK(std::get<int>(value) == 10);
    CHECK(type.first == "int");
    CHECK(type.second == sizeof(int));
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_integer_literal")
{
    credence::util::AST_Node obj;
    obj["test"] =
        credence::util::AST_Node::load("{\"node\":  \"integer_literal\","
                                       "\"root\": 10"
                                       "}");

    auto temp = language::AST_Lowering(obj);
    auto data = temp.from_integer_literal_node(obj["test"]);
    auto [value, type] = data;
    CHECK(std::get<int>(value) == 10);
    CHECK(type.first == "int");
    CHECK(type.second == sizeof(int));
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_float_literal")
{
    credence::util::AST_Node obj;
    obj["test"] =
        credence::util::AST_Node::load("{\"node\":  \"float_literal\","
                                       "\"root\": 10.0002"
                                       "}");

    auto temp = language::AST_Lowering(obj);
    auto data = temp.from_float_literal_node(obj["test"]);
    auto [value, type] = data;
    CHECK(std::get<float>(value) == 10.0002f);
    CHECK(type.first == "float");
    CHECK(type.second == sizeof(float));
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_double_literal")
{
    credence::util::AST_Node obj;
    obj["test"] =
        credence::util::AST_Node::load("{\"node\":  \"double_literal\","
                                       "\"root\": 10.05"
                                       "}");

    auto temp = language::AST_Lowering(obj);
    auto data = temp.from_double_literal_node(obj["test"]);
    auto [value, type] = data;
    CHECK(std::get<double>(value) == 10.05);
    CHECK(type.first == "double");
    CHECK(type.second == sizeof(double));
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_string_literal")
{
    credence::util::AST_Node obj;
    obj["test"] = credence::util::AST_Node::load(
        "{\n                  \"node\" : \"string_literal\",\n            "
        "     "
        " \"root\" : \"\\\"hello world\\\"\"\n                }");

    auto temp = language::AST_Lowering(obj);
    auto data = temp.from_string_literal_node(obj["test"]);
    auto [value, type] = data;
    CHECK(std::get<std::string>(value) == "hello world");
    CHECK(type.first == "string");
    CHECK(type.second == std::string{ "hello world" }.size());
}

TEST_CASE("ast_lowering.cc: AST_Lowering::from_constant_literal")
{
    credence::util::AST_Node obj;
    obj["test"] =
        credence::util::AST_Node::load("{\"node\":  \"constant_literal\","
                                       "\"root\": \"x\""
                                       "}");

    auto temp = language::AST_Lowering(obj);
    auto data = temp.from_constant_literal_node(obj["test"]);
    auto [value, type] = data;
    CHECK(std::get<char>(value) == 'x');
    CHECK(type.first == "char");
    CHECK(type.second == sizeof(char));
}