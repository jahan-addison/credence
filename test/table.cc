// clang-format off
#include <doctest/doctest.h>  // for ResultBuilder, CHECK, TestCase
// clang-format on
#include <credence/ir/table.h> // for Table
#include <credence/json.h>     // for JSON
#include <credence/symbol.h>   // for Symbol_Table
#include <credence/types.h>    // for Value_Type, RValue, Type_, Byte
#include <deque>               // for deque
#include <map>                 // for map
#include <memory>              // for shared_ptr
#include <string>              // for basic_string, string
#include <tuple>               // for get, tie
#include <utility>             // for pair, make_pair, get
#include <variant>             // for get, monostate

using namespace credence;

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

// TEST_CASE_FIXTURE(Fixture, "ir/table.cc:
// Table::parse_node")
// {
//     using namespace ir;
//     json::JSON obj;
//     obj["symbols"] = assignment_symbol_table;

//     obj["test"] = json::JSON::Load(
//         " {\n        \"left\" : [{\n            \"left\" : [{\n "
//         "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n " "  },
//         {\n                \"node\" : \"lvalue\",\n                "
//         "\"root\" : \"y\"\n              }, {\n                \"node\" : "
//         "\"lvalue\",\n                \"root\" :\"z\"\n              }],\n "
//         "        \"node\" : \"statement\",\n            \"root\" : \"auto\"\n
//         " "        }, {\n            \"left\" : [[{\n \"left\" "
//         ": {\n                    \"node\" : \"lvalue\",\n "
//         "\"root\" : \"x\"\n                  },\n                  \"node\" :
//         "
//         "\"assignment_expression\",\n                  \"right\" : {\n " "
//         \"node\" : \"number_literal\",\n                    "
//         "\"root\" : 5\n                  },\n                  \"root\" : "
//         "[\"=\", null]\n                }]],\n            \"node\" : "
//         "\"statement\",\n            \"root\" : \"rvalue\"\n          }]\n "
//         "}\n ");

//     auto temp = Table(obj["symbols"]);
//     temp.parse_node(obj["test"]);
//     Quintuple test = { Operator::EQUAL, "x", "5:int:4", "", "" };
// }

TEST_CASE("ir/table.cc: Table::rvalue_expression")
{
    json::JSON obj;
    using namespace credence::ir;
    using std::get;
    obj["test"] = json::JSON::Load(
        "[{\n                  \"node\" : \"constant_literal\",\n              "
        "    \"root\" : \"x\"\n                }, {\n                  "
        "\"node\" : \"number_literal\",\n                  \"root\" : 10\n     "
        "           }, {\n                  \"node\" : \"string_literal\",\n   "
        "               \"root\" : \"\\\"hello world\\\"\"\n                }, "
        "{\n                  \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n                  "
        "},\n                  \"node\" : \"assignment_expression\",\n         "
        "         \"right\" : {\n                    \"node\" : "
        "\"number_literal\",\n                    \"root\" : 5\n               "
        "   },\n                  \"root\" : [\"=\", null]\n                }, "
        "{\n                  \"node\" : \"evaluated_expression\",\n           "
        "       \"root\" : {\n                    \"left\" : {\n               "
        "       \"node\" : \"lvalue\",\n                      \"root\" : "
        "\"putchar\"\n                    },\n                    \"node\" : "
        "\"function_expression\",\n                    \"right\" : [{\n        "
        "                \"node\" : \"lvalue\",\n                        "
        "\"root\" : \"x\"\n                      }],\n                    "
        "\"root\" : \"putchar\"\n                  }\n                }, {\n   "
        "               \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"getchar\"\n             "
        "     },\n                  \"node\" : \"function_expression\",\n      "
        "            \"right\" : [{\n                      \"node\" : "
        "\"lvalue\",\n                      \"root\" : \"c\"\n                 "
        "   }],\n                  \"root\" : \"getchar\"\n                }, "
        "{\n                  \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n                  "
        "},\n                  \"node\" : \"relation_expression\",\n           "
        "       \"right\" : {\n                    \"node\" : "
        "\"number_literal\",\n                    \"root\" : 5\n               "
        "   },\n                  \"root\" : [\"<\"]\n                }, {\n   "
        "               \"node\" : \"post_inc_dec_expression\",\n              "
        "    \"right\" : {\n                    \"node\" : \"lvalue\",\n       "
        "             \"root\" : \"x\"\n                  },\n                 "
        " \"root\" : [\"++\"]\n                }, {\n                  "
        "\"node\" : \"post_inc_dec_expression\",\n                  \"right\" "
        ": {\n                    \"node\" : \"lvalue\",\n                    "
        "\"root\" : \"x\"\n                  },\n                  \"root\" : "
        "[\"--\"]\n                }, {\n                  \"left\" : {\n      "
        "              \"node\" : \"number_literal\",\n                    "
        "\"root\" : 5\n                  },\n                  \"node\" : "
        "\"unary_expression\",\n                  \"root\" : [\"~\"]\n         "
        "       }, {\n                  \"left\" : {\n                    "
        "\"node\" : \"evaluated_expression\",\n                    \"root\" : "
        "{\n                      \"left\" : {\n                        "
        "\"node\" : \"number_literal\",\n                        \"root\" : "
        "5\n                      },\n                      \"node\" : "
        "\"unary_expression\",\n                      \"root\" : [\"~\"]\n     "
        "               }\n                  },\n                  \"node\" : "
        "\"relation_expression\",\n                  \"right\" : {\n           "
        "         \"node\" : \"number_literal\",\n                    \"root\" "
        ": 10\n                  },\n                  \"root\" : [\"^\"]\n    "
        "            }]");
    auto temp = Table(obj);
    RValue::Value null = { std::monostate(), type::Type_["null"] };
    temp.symbols_.table_.emplace("x", null);
    temp.symbols_.table_.emplace("c", null);
    temp.symbols_.table_.emplace("putchar", null);
    temp.symbols_.table_.emplace("getchar", null);

    // check all
    for (auto& rvalue : obj["test"].ArrayRange()) {
        CHECK_NOTHROW(temp.from_rvalue(rvalue));
    }
}

TEST_CASE("ir/table.cc: Table::function_expression")
{
    using namespace credence::ir;
    json::JSON obj;
    using std::get;
    obj["test"] = json::JSON::Load(
        "{\n                  \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"putchar\"\n             "
        "     },\n                  \"node\" : \"function_expression\",\n      "
        "            \"right\" : [{\n                      \"node\" : "
        "\"lvalue\",\n                      \"root\" : \"x\"\n                 "
        "   }, {\n                      \"node\" : \"lvalue\",\n               "
        "       \"root\" : \"y\"\n                    }, {\n                   "
        "   \"node\" : \"lvalue\",\n                      \"root\" : \"z\"\n   "
        "                 }]\n}\n                    ");
    auto temp = Table(obj);
    RValue::Value null = { std::monostate(), type::Type_["null"] };
    temp.symbols_.table_.emplace("x", null);
    temp.symbols_.table_.emplace("y", null);
    temp.symbols_.table_.emplace("putchar", null);
    temp.symbols_.table_.emplace("z", null);

    auto test1 = temp.from_function_expression(obj["test"]);
    auto function = get<RValue::Function>(test1.value);
    // parameters
    CHECK(get<RValue::LValue>(function.second[0]->value).first == "x");
    CHECK(get<RValue::LValue>(function.second[1]->value).first == "y");
    CHECK(get<RValue::LValue>(function.second[2]->value).first == "z");
}

TEST_CASE("ir/table.cc: Table::evaluated_expression")
{
    using namespace credence::ir;
    json::JSON obj;
    using std::get;
    obj["test"] = json::JSON::Load(
        "[{\n                  \"node\" : \"evaluated_expression\",\n          "
        "        \"root\" : {\n                    \"left\" : {\n              "
        "        \"node\" : \"number_literal\",\n                      "
        "\"root\" : 5\n                    },\n                    \"node\" : "
        "\"relation_expression\",\n                    \"right\" : {\n         "
        "             \"node\" : \"number_literal\",\n                      "
        "\"root\" : 5\n                    },\n                    \"root\" : "
        "[\"*\"]\n                  }\n                }, {\n                  "
        "\"node\" : \"evaluated_expression\",\n                  \"root\" : "
        "{\n                    \"left\" : {\n                      \"node\" : "
        "\"lvalue\",\n                      \"root\" : \"x\"\n                 "
        "   },\n                    \"node\" : \"indirect_lvalue\",\n          "
        "          \"root\" : [\"*\"]\n                  }\n                "
        "}]");
    auto temp = Table(obj);
    RValue::Value null = { std::monostate(), type::Type_["null"] };
    temp.symbols_.table_.emplace("x", null);

    auto expressions = obj["test"].ArrayRange().get();
    auto test1 = temp.from_evaluated_expression(expressions->at(0));
    auto expr1 = get<RValue::RValue_Pointer>(test1.value);
    CHECK(
        get<RValue::Relation>(get<RValue::RValue_Pointer>(expr1->value)->value)
            .first == Operator::B_MUL);
    auto test2 = temp.from_evaluated_expression(expressions->at(1));
    auto expr2 = get<RValue::RValue_Pointer>(test2.value);
    CHECK(get<RValue::LValue>(expr2->value).first == "x");
}

TEST_CASE("ir/table.cc: Table::from_relation_expression")
{
    using namespace credence::ir;
    json::JSON obj;
    obj["test"] = json::JSON::Load(
        "[\n  {\n    \"left\": {\n      \"node\": \"lvalue\",\n      "
        "\"root\": "
        "\"x\"\n    },\n    \"node\": \"relation_expression\",\n    "
        "\"right\": "
        "{\n      \"node\": \"number_literal\",\n      \"root\": 10\n    "
        "},\n  "
        "  \"root\": [\n      \"*\"\n    ]\n  },\n  {\n    \"left\": {\n   "
        "   "
        "\"node\": \"lvalue\",\n      \"root\": \"x\"\n    },\n    "
        "\"node\": "
        "\"relation_expression\",\n    \"right\": {\n      \"left\": {\n   "
        "    "
        " \"node\": \"number_literal\",\n        \"root\": 10\n      },\n  "
        "    "
        "\"node\": \"ternary_expression\",\n      \"right\": {\n        "
        "\"node\": \"number_literal\",\n        \"root\": 1\n      },\n    "
        "  "
        "\"root\": {\n        \"node\": \"number_literal\",\n        "
        "\"root\": "
        "5\n      }\n    },\n    \"root\": [\n      \"<=\"\n    ]\n  },\n  "
        "{\n "
        "   \"left\": {\n      \"node\": \"lvalue\",\n      \"root\": "
        "\"x\"\n  "
        "  },\n    \"node\": \"relation_expression\",\n    \"right\": {\n  "
        "    "
        "\"node\": \"number_literal\",\n      \"root\": 5\n    },\n    "
        "\"root\": [\n      \"==\"\n    ]\n  },\n  {\n    \"left\": {\n    "
        "  "
        "\"node\": \"lvalue\",\n      \"root\": \"x\"\n    },\n    "
        "\"node\": "
        "\"relation_expression\",\n    \"right\": {\n      \"node\": "
        "\"number_literal\",\n      \"root\": 5\n    },\n    \"root\": [\n "
        "    "
        " \"!=\"\n    ]\n  },\n  {\n    \"left\": {\n      \"node\": "
        "\"lvalue\",\n      \"root\": \"x\"\n    },\n    \"node\": "
        "\"relation_expression\",\n    \"right\": {\n      \"node\": "
        "\"number_literal\",\n      \"root\": 0\n    },\n    \"root\": [\n "
        "    "
        " \"^\"\n    ]\n  },\n  {\n    \"left\": {\n      \"node\": "
        "\"lvalue\",\n      \"root\": \"x\"\n    },\n    \"node\": "
        "\"relation_expression\",\n    \"right\": {\n      \"node\": "
        "\"number_literal\",\n      \"root\": 5\n    },\n    \"root\": [\n "
        "    "
        " \"<\"\n    ]\n  },\n  {\n    \"left\": {\n      \"node\": "
        "\"lvalue\",\n      \"root\": \"x\"\n    },\n    \"node\": "
        "\"relation_expression\",\n    \"right\": {\n      \"node\": "
        "\"number_literal\",\n      \"root\": 10\n    },\n    \"root\": "
        "[\n    "
        "  \"<=\"\n    ]\n  }\n]");
    auto temp = Table(obj);
    RValue::Value null = { std::monostate(), type::Type_["null"] };
    std::vector<RValue::RValue_Pointer> arguments{};
    temp.symbols_.table_.emplace("x", null);

    auto relation_expressions = obj["test"].ArrayRange().get();

    // forgive me
    using std::get;

    auto test1 = temp.from_relation_expression(relation_expressions->at(0));
    CHECK(std::get<RValue::Relation>(test1.value).first == Operator::B_MUL);
    arguments = std::get<RValue::Relation>(test1.value).second;
    CHECK(std::get<RValue::LValue>(arguments[0]->value).first == "x");
    CHECK(std::get<int>(std::get<RValue::Value>(arguments[1]->value).first) ==
          10);

    // ternary relation test
    auto test2 = temp.from_relation_expression(relation_expressions->at(1));
    CHECK(std::get<RValue::Relation>(test2.value).first == Operator::R_LE);
    arguments = std::move(std::get<RValue::Relation>(test2.value).second);
    CHECK(std::get<RValue::LValue>(arguments[0]->value).first == "x");
    CHECK(get<int>(get<RValue::Value>(arguments[1]->value).first) == 5);
    CHECK(get<int>(get<RValue::Value>(arguments[2]->value).first) == 10);
    CHECK(get<int>(get<RValue::Value>(arguments[3]->value).first) == 1);

    auto test3 = temp.from_relation_expression(relation_expressions->at(2));
    CHECK(std::get<RValue::Relation>(test3.value).first == Operator::R_EQUAL);
    arguments = std::move(std::get<RValue::Relation>(test3.value).second);
    CHECK(std::get<RValue::LValue>(arguments[0]->value).first == "x");
    CHECK(std::get<int>(std::get<RValue::Value>(arguments[1]->value).first) ==
          5);

    auto test4 = temp.from_relation_expression(relation_expressions->at(3));
    CHECK(std::get<RValue::Relation>(test4.value).first == Operator::R_NEQUAL);
    arguments = std::move(std::get<RValue::Relation>(test4.value).second);
    CHECK(std::get<RValue::LValue>(arguments[0]->value).first == "x");
    CHECK(std::get<int>(std::get<RValue::Value>(arguments[1]->value).first) ==
          5);

    auto test5 = temp.from_relation_expression(relation_expressions->at(4));
    CHECK(std::get<RValue::Relation>(test5.value).first == Operator::XOR);
    arguments = std::move(std::get<RValue::Relation>(test5.value).second);
    CHECK(std::get<RValue::LValue>(arguments[0]->value).first == "x");
    CHECK(std::get<int>(std::get<RValue::Value>(arguments[1]->value).first) ==
          0);

    auto test6 = temp.from_relation_expression(relation_expressions->at(5));
    CHECK(std::get<RValue::Relation>(test6.value).first == Operator::R_LT);
    arguments = std::move(std::get<RValue::Relation>(test6.value).second);
    CHECK(std::get<RValue::LValue>(arguments[0]->value).first == "x");
    CHECK(std::get<int>(std::get<RValue::Value>(arguments[1]->value).first) ==
          5);

    auto test7 = temp.from_relation_expression(relation_expressions->at(6));
    CHECK(std::get<RValue::Relation>(test7.value).first == Operator::R_LE);
    arguments = std::move(std::get<RValue::Relation>(test7.value).second);
    CHECK(std::get<RValue::LValue>(arguments[0]->value).first == "x");
    CHECK(std::get<int>(std::get<RValue::Value>(arguments[1]->value).first) ==
          10);
}

TEST_CASE("ir/table.cc: Table::from_unary_expression")
{
    using namespace credence::ir;
    json::JSON obj;
    obj["test"] = json::JSON::Load(
        "[{\n                  \"node\" : \"post_inc_dec_expression\",\n   "
        "    "
        "           \"right\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n               "
        "   "
        "},\n                  \"root\" : [\"++\"]\n                }, {\n "
        "    "
        "             \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n               "
        "   "
        "},\n                  \"node\" : \"pre_inc_dec_expression\",\n    "
        "    "
        "          \"root\" : [\"++\"]\n                }, {\n             "
        "    "
        " \"left\" : {\n                    \"node\" : \"lvalue\",\n       "
        "    "
        "         \"root\" : \"x\"\n                  },\n                 "
        " "
        "\"node\" : \"address_of_expression\",\n                  \"root\" "
        ": "
        "[\"&\"]\n                }, {\n                  \"left\" : {\n   "
        "    "
        "             \"node\" : \"number_literal\",\n                    "
        "\"root\" : 5\n                  },\n                  \"node\" : "
        "\"unary_expression\",\n                  \"root\" : [\"~\"]\n     "
        "    "
        "       }, {\n                  \"left\" : {\n                    "
        "\"node\" : \"lvalue\",\n                    \"root\" : \"x\"\n    "
        "    "
        "          },\n                  \"node\" : \"indirect_lvalue\",\n "
        "    "
        "             \"root\" : [\"*\"]\n                }, {\n           "
        "    "
        "     \"left\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 5\n         "
        "    "
        "       },\n                    \"node\" : \"unary_expression\",\n "
        "    "
        "               \"root\" : [\"-\"]\n                  },\n         "
        "    "
        "   {\n                  \"left\" : {\n                    "
        "\"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n               "
        "   "
        "},\n                  \"node\" : \"unary_expression\",\n          "
        "    "
        "    \"root\" : [\"!\"]\n                }]");
    auto temp = Table(obj);

    RValue::Value null = { std::monostate(), type::Type_["null"] };

    temp.symbols_.table_.emplace("x", null);

    auto unary_expressions = obj["test"].ArrayRange().get();
    std::shared_ptr<RValue> lvalue, constant;

    auto test1 = temp.from_unary_expression(unary_expressions->at(0));
    lvalue = std::get<RValue::Unary>(test1.value).second;
    CHECK(std::get<RValue::Unary>(test1.value).first == Operator::POST_INC);
    CHECK(std::get<RValue::LValue>(lvalue->value).first == "x");

    auto test2 = temp.from_unary_expression(unary_expressions->at(1));
    lvalue = std::get<RValue::Unary>(test2.value).second;
    CHECK(std::get<RValue::Unary>(test2.value).first == Operator::PRE_INC);
    CHECK(std::get<RValue::LValue>(lvalue->value).first == "x");

    auto test3 = temp.from_unary_expression(unary_expressions->at(2));
    lvalue = std::get<RValue::Unary>(test3.value).second;
    CHECK(std::get<RValue::Unary>(test3.value).first == Operator::U_ADDR_OF);
    CHECK(std::get<RValue::LValue>(lvalue->value).first == "x");

    auto test4 = temp.from_unary_expression(unary_expressions->at(3));
    constant = std::get<RValue::Unary>(test4.value).second;
    CHECK(std::get<RValue::Unary>(test4.value).first ==
          Operator::U_ONES_COMPLEMENT);
    CHECK(std::get<int>(std::get<RValue::Value>(constant->value).first) == 5);

    auto test5 = temp.from_unary_expression(unary_expressions->at(4));
    lvalue = std::get<RValue::Unary>(test5.value).second;
    CHECK(std::get<RValue::Unary>(test5.value).first ==
          Operator::U_INDIRECTION);
    CHECK(std::get<RValue::LValue>(lvalue->value).first == "x");

    auto test6 = temp.from_unary_expression(unary_expressions->at(5));
    constant = std::get<RValue::Unary>(test6.value).second;
    CHECK(std::get<RValue::Unary>(test6.value).first == Operator::U_MINUS);
    CHECK(std::get<int>(std::get<RValue::Value>(constant->value).first) == 5);

    auto test7 = temp.from_unary_expression(unary_expressions->at(6));
    lvalue = std::get<RValue::Unary>(test7.value).second;
    CHECK(std::get<RValue::Unary>(test7.value).first == Operator::U_NOT);
    CHECK(std::get<RValue::LValue>(lvalue->value).first == "x");
}

TEST_CASE_FIXTURE(Fixture, "ir/table.cc: Table::from_assignment_expression")
{
    using namespace credence::ir;
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
    auto temp = Table(obj["symbols"]);
    // no declaration with `auto' or `extern', should throw
    CHECK_THROWS(temp.from_assignment_expression(obj["test"]));

    type::Value_Type value_type = { std::monostate(), type::Type_["null"] };
    type::Value_Type assigned_type = { 5, type::Type_["int"] };

    temp.symbols_.table_.emplace("x", value_type);

    auto expr = temp.from_assignment_expression(obj["test"]);

    auto lhs = std::get<type::RValue::Symbol>(expr.value).first;
    auto* rhs = &std::get<type::RValue::Symbol>(expr.value).second;

    CHECK(lhs.first == "x");
    CHECK(lhs.second == value_type);
    CHECK(std::get<type::Value_Type>((*rhs)->value) == assigned_type);
}

TEST_CASE_FIXTURE(Fixture, "ir/table.cc: Table::is_symbol")
{
    using namespace credence::ir;
    json::JSON obj;
    obj["symbols"] = assignment_symbol_table;
    obj["test"] = json::JSON::Load("{\"node\":  \"lvalue\","
                                   "\"root\": \"x\""
                                   "}");
    auto temp = Table(obj["test"]);
    // not declared with `auto' or `extern', should throw
    CHECK(temp.is_symbol(obj["test"]) == false);

    auto temp2 = Table(obj["symbols"]);
    CHECK(temp2.is_symbol(obj["test"]) == false);

    type::Value_Type value_type = { std::monostate(), type::Type_["null"] };

    temp2.symbols_.set_symbol_by_name("x", value_type);
    CHECK(temp2.is_symbol(obj["test"]) == true);
}

TEST_CASE("ir/table.cc: Table::from_lvalue_expression")
{
    using namespace credence::ir;
    json::JSON obj;
    obj["test"] = json::JSON::Load(
        "[\n               {\n                \"left\" : {\n               "
        "   "
        "\"node\" : \"number_literal\",\n                  \"root\" : 50\n "
        "    "
        "           },\n                \"node\" : \"vector_lvalue\",\n    "
        "    "
        "        \"root\" : \"x\"\n              }, {\n                "
        "\"left\" : {\n                  \"node\" : \"lvalue\",\n          "
        "    "
        "    \"root\" : \"y\"\n                },\n                "
        "\"node\" : "
        "\"indirect_lvalue\",\n                \"root\" : [\"*\"]\n        "
        "    "
        "  }, {\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"z\"\n              }]");
    auto temp = Table(obj);
    type::RValue::Value empty_value =
        std::make_pair('0', std::make_pair("byte", 50));
    auto* lvalues = obj["test"].ArrayRange().get();
    auto [vector, pointer, normal] =
        std::tie(lvalues->at(0), lvalues->at(1), lvalues->at(2));
    temp.symbols_.table_["x"] = empty_value;
    temp.symbols_.table_["y"] = empty_value;
    temp.symbols_.table_["z"] = empty_value;
    // test vector
    auto test1 = temp.from_lvalue_expression(vector);
    CHECK(test1.first == "x");
    CHECK(test1.second == empty_value);
    // test pointer
    auto test2 = temp.from_lvalue_expression(pointer);
    CHECK(test2.first == "y");
    CHECK(test2.second == empty_value);
    // normal variable
    auto test3 = temp.from_lvalue_expression(normal);
    CHECK(test3.first == "z");
    CHECK(test3.second == empty_value);
}

TEST_CASE("ir/table.cc: Table::from_indirect_identifier")
{
    using namespace credence::ir;
    json::JSON obj;
    obj["test"] = json::JSON::Load(
        "{\n                \"left\" : {\n                  \"node\" : "
        "\"lvalue\",\n                  \"root\" : \"x\"\n                "
        "},\n "
        "               \"node\" : \"indirect_lvalue\",\n                "
        "\"root\" : [\"*\"]\n              }");

    auto temp = Table(obj["test"]);
    CHECK_THROWS(temp.from_indirect_identifier(obj["test"]));
    type::RValue::Value value = std::make_pair('0', std::make_pair("byte", 50));
    temp.symbols_.table_["x"] = value;
    auto test = temp.from_indirect_identifier(obj["test"]);
    CHECK(test == value);
}
TEST_CASE("ir/tble.cc: Table::from_vector_idenfitier")
{
    using namespace credence::ir;
    json::JSON obj;
    obj["test"] = json::JSON::Load(
        "{\n                \"left\" : {\n                  \"node\" : "
        "\"number_literal\",\n                  \"root\" : 50\n            "
        "    "
        "},\n                \"node\" : \"vector_lvalue\",\n               "
        " "
        "\"root\" : \"x\"\n              }");

    auto temp = Table(obj["test"]);
    CHECK_THROWS(temp.from_vector_idenfitier(obj["test"]));
    type::Value_Type test = std::make_pair('0', std::make_pair("byte", 50));
    temp.symbols_.table_["x"] = test;
    CHECK(temp.from_vector_idenfitier(obj["test"]) == test);
}

TEST_CASE("ir/table.cc: Table::from_constant_expression")
{
    using namespace credence::ir;
    json::JSON obj;
    obj["test"] = json::JSON::Load("{\"node\":  \"number_literal\","
                                   "\"root\": 10"
                                   "}");

    auto temp = Table(obj);
    auto data = temp.from_constant_expression(obj["test"]);
    auto [value, type] = data;
    CHECK(std::get<int>(value) == 10);
    CHECK(type.first == "int");
    CHECK(type.second == sizeof(int));
}

TEST_CASE("ir/table.cc: Table::from_number_literal")
{
    using namespace credence::ir;
    json::JSON obj;
    obj["test"] = json::JSON::Load("{\"node\":  \"number_literal\","
                                   "\"root\": 10"
                                   "}");

    auto temp = Table(obj);
    auto data = temp.from_number_literal(obj["test"]);
    auto [value, type] = data;
    CHECK(std::get<int>(value) == 10);
    CHECK(type.first == "int");
    CHECK(type.second == sizeof(int));
}

TEST_CASE("ir/table.cc: Table::from_string_literal")
{
    using namespace credence::ir;
    json::JSON obj;
    obj["test"] = json::JSON::Load("{\"node\":  \"string_literal\","
                                   "\"root\": \"test string\""
                                   "}");

    auto temp = Table(obj);
    auto data = temp.from_string_literal(obj["test"]);
    auto [value, type] = data;
    CHECK(std::get<std::string>(value) == "test string");
    CHECK(type.first == "string");
    CHECK(type.second == sizeof(char) * 11);
}

TEST_CASE("ir/table.cc: Table::from_constant_literal")
{
    using namespace credence::ir;
    json::JSON obj;
    obj["test"] = json::JSON::Load("{\"node\":  \"constant_literal\","
                                   "\"root\": \"x\""
                                   "}");

    auto temp = Table(obj);
    auto data = temp.from_constant_literal(obj["test"]);
    auto [value, type] = data;
    CHECK(std::get<char>(value) == 'x');
    CHECK(type.first == "char");
    CHECK(type.second == sizeof(char));
}