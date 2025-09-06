#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase

#include <credence/ir/qaud.h> // for emit_quadruple, build_from_block_state...
#include <credence/ir/temp.h> // for make_temporary
#include <credence/symbol.h>  // for Symbol_Table
#include <credence/types.h>   // for Type_, RValue, Value_Type, Byte
#include <deque>              // for operator==, _Deque_iterator, deque
#include <map>                // for map
#include <optional>           // for optional
#include <simplejson.h>       // for JSON
#include <sstream>            // for basic_ostringstream, ostringstream
#include <string>             // for allocator, basic_string, operator==
#include <tuple>              // for tuple
#include <utility>            // for pair, make_pair, operator==
#include <variant>            // for monostate, get, operator==
#include <vector>             // for vector

TEST_CASE("ir/qaud.cc: build_from_function_definition")
{
    using namespace credence;
    using namespace credence::ir;
    json::JSON obj;
    auto internal_symbols = json::JSON::Load(
        "{\n  \"arg\" : {\n    \"column\" : 6,\n    \"end_column\" : 9,\n    "
        "\"end_pos\" : 8,\n    \"line\" : 1,\n    \"start_pos\" : 5,\n    "
        "\"type\" : \"lvalue\"\n  },\n  \"exp\" : {\n    \"column\" : 1,\n    "
        "\"end_column\" : 4,\n    \"end_pos\" : 52,\n    \"line\" : 6,\n    "
        "\"start_pos\" : 49,\n    \"type\" : \"function_definition\"\n  },\n  "
        "\"main\" : {\n    \"column\" : 1,\n    \"end_column\" : 5,\n    "
        "\"end_pos\" : 4,\n    \"line\" : 1,\n    \"start_pos\" : 0,\n    "
        "\"type\" : \"function_definition\"\n  },\n  \"x\" : {\n    \"column\" "
        ": 8,\n    \"end_column\" : 9,\n    \"end_pos\" : 20,\n    \"line\" : "
        "2,\n    \"start_pos\" : 19,\n    \"type\" : \"lvalue\"\n  },\n  \"y\" "
        ": {\n    \"column\" : 7,\n    \"end_column\" : 8,\n    \"end_pos\" : "
        "56,\n    \"line\" : 6,\n    \"start_pos\" : 55,\n    \"type\" : "
        "\"lvalue\"\n  }\n}");
    obj["test"] = json::JSON::Load(
        "{\n      \"left\" : [null],\n      \"node\" : "
        "\"function_definition\",\n      \"right\" : {\n        \"left\" : "
        "[{\n            \"left\" : [{\n                \"node\" : "
        "\"lvalue\",\n                \"root\" : \"x\"\n              }],\n    "
        "        \"node\" : \"statement\",\n            \"root\" : \"auto\"\n  "
        "        }, {\n            \"left\" : [[{\n                  \"left\" "
        ": {\n                    \"node\" : \"lvalue\",\n                    "
        "\"root\" : \"x\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"left\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 5\n             "
        "       },\n                    \"node\" : \"relation_expression\",\n  "
        "                  \"right\" : {\n                      \"left\" : {\n "
        "                       \"node\" : \"number_literal\",\n               "
        "         \"root\" : 5\n                      },\n                     "
        " \"node\" : \"relation_expression\",\n                      \"right\" "
        ": {\n                        \"left\" : {\n                          "
        "\"node\" : \"lvalue\",\n                          \"root\" : "
        "\"exp\"\n                        },\n                        \"node\" "
        ": \"function_expression\",\n                        \"right\" : [{\n  "
        "                          \"node\" : \"number_literal\",\n            "
        "                \"root\" : 2\n                          }, {\n        "
        "                    \"node\" : \"number_literal\",\n                  "
        "          \"root\" : 5\n                          }],\n               "
        "         \"root\" : \"exp\"\n                      },\n               "
        "       \"root\" : [\"+\"]\n                    },\n                   "
        " \"root\" : [\"*\"]\n                  },\n                  \"root\" "
        ": [\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }],\n    "
        "    \"node\" : \"statement\",\n        \"root\" : \"block\"\n      "
        "},\n      \"root\" : \"main\"\n    }");

    credence::Symbol_Table<> symbols{};
    Instructions test_instructions{};
    std::ostringstream os_test;
    test_instructions = build_from_function_definition(
        symbols, symbols, obj["test"], internal_symbols);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    std::string expected = R"qaud(__main:
 BeginFunc ;
PUSH (5:int:4);
PUSH (2:int:4);
CALL exp;
POP 16;
_t2 = RET;
_t3 = _t2;
_t4 = (5:int:4) + _t3;
x = (5:int:4) * _t4;
LEAVE;
 EndFunc ;
)qaud";
    CHECK(os_test.str() == expected);
}

TEST_CASE("ir/qaud.cc: build_from_block_statement")
{
    using namespace credence;
    using namespace credence::ir;
    json::JSON obj;
    obj["test"] = json::JSON::Load(
        "{\n        \"left\" : [{\n            \"left\" : [{\n                "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n            "
        "  }],\n            \"node\" : \"statement\",\n            \"root\" : "
        "\"auto\"\n          }, {\n            \"left\" : [[{\n                "
        "  \"left\" : {\n                    \"node\" : \"lvalue\",\n          "
        "          \"root\" : \"x\"\n                  },\n                  "
        "\"node\" : \"assignment_expression\",\n                  \"right\" : "
        "{\n                    \"left\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 5\n             "
        "       },\n                    \"node\" : \"relation_expression\",\n  "
        "                  \"right\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 2\n             "
        "       },\n                    \"root\" : [\"||\"]\n                  "
        "},\n                  \"root\" : [\"=\", null]\n                "
        "}]],\n            \"node\" : \"statement\",\n            \"root\" : "
        "\"rvalue\"\n          }],\n        \"node\" : \"statement\",\n        "
        "\"root\" : \"block\"\n      }");

    credence::Symbol_Table<> symbols{};
    Instructions test_instructions{};
    std::ostringstream os_test;
    test_instructions =
        build_from_block_statement(symbols, symbols, obj["test"], obj);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    CHECK(os_test.str() == "_t1 = (5:int:4) || (2:int:4);\nx = _t1;\n");
}

TEST_CASE("ir/qaud.cc: build_from_extrn_statement")
{
    using namespace credence;
    using namespace credence::ir;
    json::JSON obj;

    obj["test"] = json::JSON::Load(
        "{\n            \"left\" : [{\n                \"node\" : "
        "\"lvalue\",\n                \"root\" : \"a\"\n              }, {\n   "
        "             \"node\" : \"lvalue\",\n                \"root\" : "
        "\"b\"\n              }, {\n                \"node\" : \"lvalue\",\n   "
        "             \"root\" : \"c\"\n              }],\n            "
        "\"node\" : \"statement\",\n            \"root\" : \"extrn\"\n         "
        " }");

    credence::Symbol_Table<> symbols{};
    credence::Symbol_Table<> globals{};
    type::RValue::Value null = type::NULL_LITERAL;

    CHECK_THROWS(build_from_extrn_statement(symbols, globals, obj["test"]));
    globals.table_.emplace("a", null);
    globals.table_.emplace("b", null);
    globals.table_.emplace("c", null);
    CHECK_NOTHROW(build_from_extrn_statement(symbols, globals, obj["test"]));
    CHECK_EQ(symbols.is_defined("a"), true);
    CHECK_EQ(symbols.is_defined("b"), true);
    CHECK_EQ(symbols.is_defined("c"), true);
}

TEST_CASE("ir/qaud.cc: build_from_vector_definition")
{
    using namespace credence;
    using namespace credence::ir;
    json::JSON obj;
    obj["symbols"] = json::JSON::Load(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"main\": "
        "{\"type\": \"function_definition\", \"line\": 1, \"start_pos\": 0, "
        "\"column\": 1, \"end_pos\": 4, \"end_column\": 5}, \"a\": {\"type\": "
        "\"vector_definition\", \"line\": 11, \"start_pos\": 93, \"column\": "
        "1, \"end_pos\": 94, \"end_column\": 2}, \"b\": {\"type\": "
        "\"vector_definition\", \"line\": 12, \"start_pos\": 103, \"column\": "
        "1, \"end_pos\": 104, \"end_column\": 2}, \"add\": {\"type\": "
        "\"function_definition\", \"line\": 6, \"start_pos\": 39, \"column\": "
        "1, \"end_pos\": 42, \"end_column\": 4}, \"c\": {\"type\": "
        "\"vector_definition\", \"line\": 13, \"start_pos\": 113, \"column\": "
        "1, \"end_pos\": 114, \"end_column\": 2}, \"mess\": {\"type\": "
        "\"vector_definition\", \"line\": 15, \"start_pos\": 124, \"column\": "
        "1, \"end_pos\": 128, \"end_column\": 5}}");

    obj["test"] = json::JSON::Load(
        "[{\n      \"left\" : {\n        \"node\" : \"string_literal\",\n      "
        "  \"root\" : \"\\\"orld\\\"\"\n      },\n      \"node\" : "
        "\"vector_definition\",\n      \"right\" : [],\n      \"root\" : "
        "\"c\"\n    }, {\n      \"left\" : {\n        \"node\" : "
        "\"number_literal\",\n        \"root\" : 2\n      },\n      \"node\" : "
        "\"vector_definition\",\n      \"right\" : [{\n          \"node\" : "
        "\"string_literal\",\n          \"root\" : \"\\\"too bad\\\"\"\n       "
        " }, {\n          \"node\" : \"string_literal\",\n          \"root\" : "
        "\"\\\"tough luck\\\"\"\n        }]\n    }]");

    credence::Symbol_Table<> symbols{};
    auto vectors = obj["test"].ArrayRange().get();
    build_from_vector_definition(symbols, vectors->at(0), obj["symbols"]);
    CHECK(symbols.is_defined(vectors->at(0)["root"].ToString()) == true);
    auto symbol = symbols.get_symbol_by_name(vectors->at(0)["root"].ToString());
    auto [value, type] = symbol;
    CHECK(std::get<std::string>(value) == "orld");
    CHECK(type.first == "string");
    CHECK(type.second == sizeof(char) * 4);
    build_from_vector_definition(symbols, vectors->at(1), obj["symbols"]);
    CHECK(symbols.is_defined(vectors->at(1)["root"].ToString()) == true);
    CHECK(symbols.is_pointer(vectors->at(1)["root"].ToString()) == true);
    auto vector_of_strings =
        symbols.get_pointer_by_name(vectors->at(1)["root"].ToString());
    CHECK(vector_of_strings.size() == 2);
    CHECK(std::get<std::string>(vector_of_strings.at(0).first) == "too bad");
    CHECK(std::get<std::string>(vector_of_strings.at(1).first) == "tough luck");
}

TEST_CASE("ir/qaud.cc: build_from_return_statement")
{
    using namespace credence;
    using namespace credence::ir;
    json::JSON obj;
    auto internal_symbols = json::JSON::Load(
        "{\n  \"arg\" : {\n    \"column\" : 6,\n    \"end_column\" : 9,\n    "
        "\"end_pos\" : 8,\n    \"line\" : 1,\n    \"start_pos\" : 5,\n    "
        "\"type\" : \"lvalue\"\n  },\n  \"exp\" : {\n    \"column\" : 1,\n    "
        "\"end_column\" : 4,\n    \"end_pos\" : 52,\n    \"line\" : 6,\n    "
        "\"start_pos\" : 49,\n    \"type\" : \"function_definition\"\n  },\n  "
        "\"main\" : {\n    \"column\" : 1,\n    \"end_column\" : 5,\n    "
        "\"end_pos\" : 4,\n    \"line\" : 1,\n    \"start_pos\" : 0,\n    "
        "\"type\" : \"function_definition\"\n  },\n  \"x\" : {\n    \"column\" "
        ": 8,\n    \"end_column\" : 9,\n    \"end_pos\" : 20,\n    \"line\" : "
        "2,\n    \"start_pos\" : 19,\n    \"type\" : \"lvalue\"\n  },\n  \"y\" "
        ": {\n    \"column\" : 7,\n    \"end_column\" : 8,\n    \"end_pos\" : "
        "56,\n    \"line\" : 6,\n    \"start_pos\" : 55,\n    \"type\" : "
        "\"lvalue\"\n  }\n}");
    obj["test"] = json::JSON::Load(
        "{\n            \"left\" : [{\n                \"left\" : {\n          "
        "        \"node\" : \"lvalue\",\n                  \"root\" : \"x\"\n  "
        "              },\n                \"node\" : "
        "\"relation_expression\",\n                \"right\" : {\n             "
        "     \"left\" : {\n                    \"node\" : \"lvalue\",\n       "
        "             \"root\" : \"y\"\n                  },\n                 "
        " \"node\" : \"relation_expression\",\n                  \"right\" : "
        "{\n                    \"node\" : \"lvalue\",\n                    "
        "\"root\" : \"y\"\n                  },\n                  \"root\" : "
        "[\"*\"]\n                },\n                \"root\" : [\"*\"]\n     "
        "         }],\n            \"node\" : \"statement\",\n            "
        "\"root\" : \"return\"\n          }");

    credence::Symbol_Table<> symbols{};
    Instructions test_instructions{};
    std::ostringstream os_test;
    int temporary{ 0 };
    type::RValue::Value null = type::NULL_LITERAL;
    symbols.table_.emplace("x", null);
    symbols.table_.emplace("y", null);
    test_instructions = build_from_return_statement(
        symbols, obj["test"], internal_symbols, &temporary);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    std::string expected = R"qaud(_t1 = y * y;
_t2 = x * _t1;
RET _t2;
)qaud";
    CHECK(os_test.str() == expected);
}

TEST_CASE("ir/qaud.cc: build_from_block_statement")
{
    using namespace credence;
    using namespace credence::ir;
    json::JSON obj;
    obj["test"] = json::JSON::Load(
        "{\n        \"left\" : [{\n            \"left\" : [{\n                "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n            "
        "  }],\n            \"node\" : \"statement\",\n            \"root\" : "
        "\"auto\"\n          }, {\n            \"left\" : [[{\n                "
        "  \"left\" : {\n                    \"node\" : \"lvalue\",\n          "
        "          \"root\" : \"x\"\n                  },\n                  "
        "\"node\" : \"assignment_expression\",\n                  \"right\" : "
        "{\n                    \"left\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 5\n             "
        "       },\n                    \"node\" : \"relation_expression\",\n  "
        "                  \"right\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 2\n             "
        "       },\n                    \"root\" : [\"||\"]\n                  "
        "},\n                  \"root\" : [\"=\", null]\n                "
        "}]],\n            \"node\" : \"statement\",\n            \"root\" : "
        "\"rvalue\"\n          }],\n        \"node\" : \"statement\",\n        "
        "\"root\" : \"block\"\n      }");

    credence::Symbol_Table<> symbols{};
    Instructions test_instructions{};
    std::ostringstream os_test;
    test_instructions =
        build_from_block_statement(symbols, symbols, obj["test"], obj);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    CHECK(os_test.str() == "_t1 = (5:int:4) || (2:int:4);\nx = _t1;\n");
}

TEST_CASE("ir/qaud.cc: while statement branching")
{
    using namespace credence;
    using namespace credence::ir;
    json::JSON obj;
    obj["symbols"] = json::JSON::Load(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"main\": "
        "{\"type\": \"function_definition\", \"line\": 1, \"start_pos\": 0, "
        "\"column\": 1, \"end_pos\": 4, \"end_column\": 5}}");

    obj["test"] = json::JSON::Load(
        "{\n        \"left\" : [{\n            \"left\" : [{\n                "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n            "
        "  }],\n            \"node\" : \"statement\",\n            \"root\" : "
        "\"auto\"\n          }, {\n            \"left\" : [[{\n                "
        "  \"left\" : {\n                    \"node\" : \"lvalue\",\n          "
        "          \"root\" : \"x\"\n                  },\n                  "
        "\"node\" : \"assignment_expression\",\n                  \"right\" : "
        "{\n                    \"left\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n       "
        "                 \"left\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 5\n         "
        "               },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : {\n     "
        "                     \"node\" : \"number_literal\",\n                 "
        "         \"root\" : 5\n                        },\n                   "
        "     \"root\" : [\"+\"]\n                      }\n                    "
        "},\n                    \"node\" : \"relation_expression\",\n         "
        "           \"right\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n       "
        "                 \"left\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 3\n         "
        "               },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : {\n     "
        "                     \"node\" : \"number_literal\",\n                 "
        "         \"root\" : 3\n                        },\n                   "
        "     \"root\" : [\"+\"]\n                      }\n                    "
        "},\n                    \"root\" : [\"*\"]\n                  },\n    "
        "              \"root\" : [\"=\", null]\n                }]],\n        "
        "    \"node\" : \"statement\",\n            \"root\" : \"rvalue\"\n    "
        "      }, {\n            \"left\" : {\n              \"left\" : {\n    "
        "            \"node\" : \"lvalue\",\n                \"root\" : "
        "\"x\"\n              },\n              \"node\" : "
        "\"relation_expression\",\n              \"right\" : {\n               "
        " \"node\" : \"number_literal\",\n                \"root\" : 5\n       "
        "       },\n              \"root\" : [\"<=\"]\n            },\n        "
        "    \"node\" : \"statement\",\n            \"right\" : [{\n           "
        "     \"left\" : [{\n                    \"left\" : [[{\n              "
        "            \"left\" : {\n                            \"node\" : "
        "\"lvalue\",\n                            \"root\" : \"x\"\n           "
        "               },\n                          \"node\" : "
        "\"assignment_expression\",\n                          \"right\" : {\n "
        "                           \"node\" : \"number_literal\",\n           "
        "                 \"root\" : 1\n                          },\n         "
        "                 \"root\" : [\"=\", null]\n                        "
        "}]],\n                    \"node\" : \"statement\",\n                 "
        "   \"root\" : \"rvalue\"\n                  }],\n                "
        "\"node\" : \"statement\",\n                \"root\" : \"block\"\n     "
        "         }, {\n                \"left\" : [{\n                    "
        "\"left\" : [[{\n                          \"left\" : {\n              "
        "              \"node\" : \"lvalue\",\n                            "
        "\"root\" : \"x\"\n                          },\n                      "
        "    \"node\" : \"assignment_expression\",\n                          "
        "\"right\" : {\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 8\n       "
        "                   },\n                          \"root\" : [\"=\", "
        "null]\n                        }]],\n                    \"node\" : "
        "\"statement\",\n                    \"root\" : \"rvalue\"\n           "
        "       }],\n                \"node\" : \"statement\",\n               "
        " \"root\" : \"block\"\n              }],\n            \"root\" : "
        "\"if\"\n          }],\n        \"node\" : \"statement\",\n        "
        "\"root\" : \"block\"\n      }");

    credence::Symbol_Table<> symbols{};
    Instructions test_instructions{};
    std::ostringstream os_test;
    type::RValue::Value null = type::NULL_LITERAL;

    symbols.table_.emplace("add", null);
    int temporary{ 0 };
    auto tail_branch = detail::make_temporary(&temporary);
    test_instructions = build_from_block_statement(symbols,
                                                   symbols,
                                                   obj["test"],
                                                   obj["symbols"],
                                                   true,
                                                   tail_branch,
                                                   &temporary);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    std::string expected = R"qaud(_t2 = (5:int:4) + (5:int:4);
_t3 = (3:int:4) + (3:int:4);
_t4 = _t2 * _t3;
x = _t4;
_t5 = x <= (5:int:4);
IF _t5 GOTO _L6;
GOTO _L7;
_L1:
_L8:
LEAVE;
_L6:
x = (1:int:4);
GOTO _L1;
_L7:
x = (8:int:4);
GOTO _L1;
)qaud";
    CHECK(os_test.str() == expected);
}

TEST_CASE("ir/qaud.cc: if and else branching")
{
    using namespace credence;
    using namespace credence::ir;
    json::JSON obj;
    obj["symbols"] = json::JSON::Load(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"main\": "
        "{\"type\": \"function_definition\", \"line\": 1, \"start_pos\": 0, "
        "\"column\": 1, \"end_pos\": 4, \"end_column\": 5}}");

    obj["test"] = json::JSON::Load(
        "{\n        \"left\" : [{\n            \"left\" : [{\n                "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n            "
        "  }],\n            \"node\" : \"statement\",\n            \"root\" : "
        "\"auto\"\n          }, {\n            \"left\" : [[{\n                "
        "  \"left\" : {\n                    \"node\" : \"lvalue\",\n          "
        "          \"root\" : \"x\"\n                  },\n                  "
        "\"node\" : \"assignment_expression\",\n                  \"right\" : "
        "{\n                    \"left\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n       "
        "                 \"left\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 5\n         "
        "               },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : {\n     "
        "                     \"node\" : \"number_literal\",\n                 "
        "         \"root\" : 5\n                        },\n                   "
        "     \"root\" : [\"+\"]\n                      }\n                    "
        "},\n                    \"node\" : \"relation_expression\",\n         "
        "           \"right\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n       "
        "                 \"left\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 3\n         "
        "               },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : {\n     "
        "                     \"node\" : \"number_literal\",\n                 "
        "         \"root\" : 3\n                        },\n                   "
        "     \"root\" : [\"+\"]\n                      }\n                    "
        "},\n                    \"root\" : [\"*\"]\n                  },\n    "
        "              \"root\" : [\"=\", null]\n                }]],\n        "
        "    \"node\" : \"statement\",\n            \"root\" : \"rvalue\"\n    "
        "      }, {\n            \"left\" : {\n              \"left\" : {\n    "
        "            \"node\" : \"lvalue\",\n                \"root\" : "
        "\"x\"\n              },\n              \"node\" : "
        "\"relation_expression\",\n              \"right\" : {\n               "
        " \"node\" : \"number_literal\",\n                \"root\" : 5\n       "
        "       },\n              \"root\" : [\"<=\"]\n            },\n        "
        "    \"node\" : \"statement\",\n            \"right\" : [{\n           "
        "     \"left\" : [{\n                    \"left\" : [[{\n              "
        "            \"left\" : {\n                            \"node\" : "
        "\"lvalue\",\n                            \"root\" : \"x\"\n           "
        "               },\n                          \"node\" : "
        "\"assignment_expression\",\n                          \"right\" : {\n "
        "                           \"node\" : \"number_literal\",\n           "
        "                 \"root\" : 1\n                          },\n         "
        "                 \"root\" : [\"=\", null]\n                        "
        "}]],\n                    \"node\" : \"statement\",\n                 "
        "   \"root\" : \"rvalue\"\n                  }],\n                "
        "\"node\" : \"statement\",\n                \"root\" : \"block\"\n     "
        "         }, {\n                \"left\" : [{\n                    "
        "\"left\" : [[{\n                          \"left\" : {\n              "
        "              \"node\" : \"lvalue\",\n                            "
        "\"root\" : \"x\"\n                          },\n                      "
        "    \"node\" : \"assignment_expression\",\n                          "
        "\"right\" : {\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 8\n       "
        "                   },\n                          \"root\" : [\"=\", "
        "null]\n                        }]],\n                    \"node\" : "
        "\"statement\",\n                    \"root\" : \"rvalue\"\n           "
        "       }],\n                \"node\" : \"statement\",\n               "
        " \"root\" : \"block\"\n              }],\n            \"root\" : "
        "\"if\"\n          }],\n        \"node\" : \"statement\",\n        "
        "\"root\" : \"block\"\n      }");

    credence::Symbol_Table<> symbols{};
    Instructions test_instructions{};
    std::ostringstream os_test;
    type::RValue::Value null = type::NULL_LITERAL;

    symbols.table_.emplace("add", null);
    int temporary{ 0 };
    auto tail_branch = detail::make_temporary(&temporary);
    test_instructions = build_from_block_statement(symbols,
                                                   symbols,
                                                   obj["test"],
                                                   obj["symbols"],
                                                   true,
                                                   tail_branch,
                                                   &temporary);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    std::string expected = R"qaud(_t2 = (5:int:4) + (5:int:4);
_t3 = (3:int:4) + (3:int:4);
_t4 = _t2 * _t3;
x = _t4;
_t5 = x <= (5:int:4);
IF _t5 GOTO _L6;
GOTO _L7;
_L1:
_L8:
LEAVE;
_L6:
x = (1:int:4);
GOTO _L1;
_L7:
x = (8:int:4);
GOTO _L1;
)qaud";
    CHECK(os_test.str() == expected);
}

TEST_CASE("ir/qaud.cc: truthy type coercion")
{
    using namespace credence;
    using namespace credence::ir;
    json::JSON obj;
    obj["symbols"] = json::JSON::Load(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"y\": {\"type\": "
        "\"lvalue\", \"line\": 2, \"start_pos\": 19, \"column\": 11, "
        "\"end_pos\": 20, \"end_column\": 12}, \"main\": {\"type\": "
        "\"function_definition\", \"line\": 1, \"start_pos\": 0, \"column\": "
        "1, \"end_pos\": 4, \"end_column\": 5}}");

    obj["test"] = json::JSON::Load(
        "{\n        \"left\" : [{\n            \"left\" : [{\n                "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n            "
        "  }, {\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"y\"\n              }],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"auto\"\n          }, {\n     "
        "       \"left\" : [[{\n                  \"left\" : {\n               "
        "     \"node\" : \"lvalue\",\n                    \"root\" : \"x\"\n   "
        "               },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 5\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }, {\n   "
        "         \"left\" : {\n              \"node\" : \"lvalue\",\n         "
        "     \"root\" : \"x\"\n            },\n            \"node\" : "
        "\"statement\",\n            \"right\" : [{\n                \"left\" "
        ": [{\n                    \"left\" : [[{\n                          "
        "\"left\" : {\n                            \"node\" : \"lvalue\",\n    "
        "                        \"root\" : \"y\"\n                          "
        "},\n                          \"node\" : \"assignment_expression\",\n "
        "                         \"right\" : {\n                            "
        "\"node\" : \"number_literal\",\n                            \"root\" "
        ": 10\n                          },\n                          "
        "\"root\" : [\"=\", null]\n                        }]],\n              "
        "      \"node\" : \"statement\",\n                    \"root\" : "
        "\"rvalue\"\n                  }],\n                \"node\" : "
        "\"statement\",\n                \"root\" : \"block\"\n              "
        "}, null],\n            \"root\" : \"if\"\n          }],\n        "
        "\"node\" : \"statement\",\n        \"root\" : \"block\"\n      }");

    credence::Symbol_Table<> symbols{};
    Instructions test_instructions{};
    std::ostringstream os_test;
    type::RValue::Value null = type::NULL_LITERAL;

    symbols.table_.emplace("x", null);
    symbols.table_.emplace("y", null);
    int temporary{ 0 };
    auto tail_branch = detail::make_temporary(&temporary);
    test_instructions = build_from_block_statement(symbols,
                                                   symbols,
                                                   obj["test"],
                                                   obj["symbols"],
                                                   true,
                                                   tail_branch,
                                                   &temporary);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    std::string expected = R"qaud(x = (5:int:4);
_t2 = CMP x;
IF _t2 GOTO _L3;
_L1:
_L4:
LEAVE;
_L3:
y = (10:int:4);
GOTO _L1;
)qaud";
    CHECK(os_test.str() == expected);
}

TEST_CASE("ir/qaud.cc: label and goto")
{
    using namespace credence;
    using namespace credence::ir;
    json::JSON obj;
    obj["symbols"] = json::JSON::Load(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"y\": {\"type\": "
        "\"lvalue\", \"line\": 2, \"start_pos\": 18, \"column\": 10, "
        "\"end_pos\": 19, \"end_column\": 11}, \"ADD\": {\"type\": \"label\", "
        "\"line\": 3, \"start_pos\": 21, \"column\": 1, \"end_pos\": 25, "
        "\"end_column\": 5}, \"add\": {\"type\": \"function_definition\", "
        "\"line\": 9, \"start_pos\": 67, \"column\": 1, \"end_pos\": 70, "
        "\"end_column\": 4}, \"main\": {\"type\": \"function_definition\", "
        "\"line\": 1, \"start_pos\": 0, \"column\": 1, \"end_pos\": 4, "
        "\"end_column\": 5}, \"a\": {\"type\": \"lvalue\", \"line\": 9, "
        "\"start_pos\": 71, \"column\": 5, \"end_pos\": 72, \"end_column\": "
        "6}, \"b\": {\"type\": \"lvalue\", \"line\": 9, \"start_pos\": 73, "
        "\"column\": 7, \"end_pos\": 74, \"end_column\": 8}}");

    obj["test"] = json::JSON::Load(
        "{\n        \"left\" : [{\n            \"left\" : [{\n                "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n            "
        "  }, {\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"y\"\n              }],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"auto\"\n          }, {\n     "
        "       \"left\" : [\"ADD\"],\n            \"node\" : \"statement\",\n "
        "           \"root\" : \"label\"\n          }, {\n            \"left\" "
        ": [[{\n                  \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n                  "
        "},\n                  \"node\" : \"assignment_expression\",\n         "
        "         \"right\" : {\n                    \"left\" : {\n            "
        "          \"node\" : \"lvalue\",\n                      \"root\" : "
        "\"add\"\n                    },\n                    \"node\" : "
        "\"function_expression\",\n                    \"right\" : [{\n        "
        "                \"node\" : \"number_literal\",\n                      "
        "  \"root\" : 2\n                      }, {\n                        "
        "\"node\" : \"number_literal\",\n                        \"root\" : "
        "5\n                      }],\n                    \"root\" : "
        "\"add\"\n                  },\n                  \"root\" : [\"=\", "
        "null]\n                }], [{\n                  \"left\" : {\n       "
        "             \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"y\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 10\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }, {\n   "
        "         \"left\" : [\"ADD\"],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"goto\"\n          }],\n      "
        "  \"node\" : \"statement\",\n        \"root\" : \"block\"\n      }");

    credence::Symbol_Table<> symbols{};
    Instructions test_instructions{};
    std::ostringstream os_test;
    type::RValue::Value null = type::NULL_LITERAL;

    symbols.table_.emplace("add", null);
    test_instructions = build_from_block_statement(
        symbols, symbols, obj["test"], obj["symbols"]);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    std::string expected = R"qaud(_L_ADD:
PUSH (5:int:4);
PUSH (2:int:4);
CALL add;
POP 16;
_t1 = RET;
x = _t1;
y = (10:int:4);
GOTO ADD;
)qaud";
    CHECK(os_test.str() == expected);
}

TEST_CASE("ir/qaud.cc: build_from_rvalue_statement")
{
    using namespace credence;
    using namespace credence::ir;
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
        "\"exp\"\n                        },\n                        \"node\" "
        ": \"function_expression\",\n                        \"right\" : [{\n  "
        "                          \"node\" : \"number_literal\",\n            "
        "                \"root\" : 2\n                          }, {\n        "
        "                    \"node\" : \"number_literal\",\n                  "
        "          \"root\" : 5\n                          }],\n               "
        "         \"root\" : \"exp\"\n                      },\n               "
        "       \"node\" : \"relation_expression\",\n                      "
        "\"right\" : {\n                        \"left\" : {\n                 "
        "         \"left\" : {\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 4\n       "
        "                   },\n                          \"node\" : "
        "\"unary_expression\",\n                          \"root\" : [\"~\"]\n "
        "                       },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : {\n     "
        "                     \"node\" : \"number_literal\",\n                 "
        "         \"root\" : 2\n                        },\n                   "
        "     \"root\" : [\"^\"]\n                      },\n                   "
        "   \"root\" : [\"/\"]\n                    },\n                    "
        "\"root\" : [\"+\"]\n                  },\n                  \"root\" "
        ": [\"*\"]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }");
    obj["nested_binary"] = json::JSON::Load(
        "{\n            \"left\" : [[{\n                  \"left\" : {\n       "
        "             \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"y\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 3\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }], [{\n                  \"left\" : "
        "{\n                    \"node\" : \"lvalue\",\n                    "
        "\"root\" : \"x\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"left\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n       "
        "                 \"left\" : {\n                          \"node\" : "
        "\"lvalue\",\n                          \"root\" : \"y\"\n             "
        "           },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : {\n     "
        "                     \"node\" : \"number_literal\",\n                 "
        "         \"root\" : 3\n                        },\n                   "
        "     \"root\" : [\"==\"]\n                      }\n                   "
        " },\n                    \"node\" : \"relation_expression\",\n        "
        "            \"right\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n       "
        "                 \"left\" : {\n                          \"node\" : "
        "\"lvalue\",\n                          \"root\" : \"y\"\n             "
        "           },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : {\n     "
        "                     \"node\" : \"number_literal\",\n                 "
        "         \"root\" : 2\n                        },\n                   "
        "     \"root\" : [\">\"]\n                      }\n                    "
        "},\n                    \"root\" : [\"&&\"]\n                  },\n   "
        "               \"root\" : [\"=\", null]\n                }]],\n       "
        "     \"node\" : \"statement\",\n            \"root\" : \"rvalue\"\n   "
        "       }");
    obj["nested_or"] = json::JSON::Load(
        "{\n            \"left\" : [[{\n                  \"left\" : {\n       "
        "             \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"y\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 3\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }], [{\n                  \"left\" : "
        "{\n                    \"node\" : \"lvalue\",\n                    "
        "\"root\" : \"x\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"left\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 1\n             "
        "       },\n                    \"node\" : \"relation_expression\",\n  "
        "                  \"right\" : {\n                      \"left\" : {\n "
        "                       \"node\" : \"number_literal\",\n               "
        "         \"root\" : 2\n                      },\n                     "
        " \"node\" : \"relation_expression\",\n                      \"right\" "
        ": {\n                        \"node\" : \"number_literal\",\n         "
        "               \"root\" : 3\n                      },\n               "
        "       \"root\" : [\"||\"]\n                    },\n                  "
        "  \"root\" : [\"||\"]\n                  },\n                  "
        "\"root\" : [\"=\", null]\n                }]],\n            \"node\" "
        ": \"statement\",\n            \"root\" : \"rvalue\"\n          }");
    obj["complex_or"] = json::JSON::Load(
        "{\n            \"left\" : [[{\n                  \"left\" : {\n       "
        "             \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"y\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 3\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }], [{\n                  \"left\" : "
        "{\n                    \"node\" : \"lvalue\",\n                    "
        "\"root\" : \"x\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"left\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 1\n             "
        "       },\n                    \"node\" : \"relation_expression\",\n  "
        "                  \"right\" : {\n                      \"left\" : {\n "
        "                       \"node\" : \"number_literal\",\n               "
        "         \"root\" : 1\n                      },\n                     "
        " \"node\" : \"relation_expression\",\n                      \"right\" "
        ": {\n                        \"left\" : {\n                          "
        "\"node\" : \"number_literal\",\n                          \"root\" : "
        "2\n                        },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : {\n     "
        "                     \"left\" : {\n                            "
        "\"node\" : \"number_literal\",\n                            \"root\" "
        ": 2\n                          },\n                          \"node\" "
        ": \"relation_expression\",\n                          \"right\" : {\n "
        "                           \"left\" : {\n                             "
        " \"node\" : \"number_literal\",\n                              "
        "\"root\" : 3\n                            },\n                        "
        "    \"node\" : \"relation_expression\",\n                            "
        "\"right\" : {\n                              \"node\" : "
        "\"number_literal\",\n                              \"root\" : 3\n     "
        "                       },\n                            \"root\" : "
        "[\"+\"]\n                          },\n                          "
        "\"root\" : [\"||\"]\n                        },\n                     "
        "   \"root\" : [\"+\"]\n                      },\n                     "
        " \"root\" : [\"||\"]\n                    },\n                    "
        "\"root\" : [\"+\"]\n                  },\n                  \"root\" "
        ": [\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n}");
    obj["or_with_call"] = json::JSON::Load(
        "{\n            \"left\" : [[{\n                  \"left\" : {\n       "
        "             \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"y\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 3\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }], [{\n                  \"left\" : "
        "{\n                    \"node\" : \"lvalue\",\n                    "
        "\"root\" : \"putchar\"\n                  },\n                  "
        "\"node\" : \"function_expression\",\n                  \"right\" : "
        "[{\n                      \"node\" : \"number_literal\",\n            "
        "          \"root\" : 5\n                    }],\n                  "
        "\"root\" : \"putchar\"\n                }], [{\n                  "
        "\"left\" : {\n                    \"node\" : \"lvalue\",\n            "
        "        \"root\" : \"x\"\n                  },\n                  "
        "\"node\" : \"assignment_expression\",\n                  \"right\" : "
        "{\n                    \"left\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 1\n             "
        "       },\n                    \"node\" : \"relation_expression\",\n  "
        "                  \"right\" : {\n                      \"left\" : {\n "
        "                       \"node\" : \"number_literal\",\n               "
        "         \"root\" : 1\n                      },\n                     "
        " \"node\" : \"relation_expression\",\n                      \"right\" "
        ": {\n                        \"left\" : {\n                          "
        "\"left\" : {\n                            \"node\" : \"lvalue\",\n    "
        "                        \"root\" : \"getchar\"\n                      "
        "    },\n                          \"node\" : "
        "\"function_expression\",\n                          \"right\" : [{\n  "
        "                            \"node\" : \"number_literal\",\n          "
        "                    \"root\" : 1\n                            }],\n   "
        "                       \"root\" : \"getchar\"\n                       "
        " },\n                        \"node\" : \"relation_expression\",\n    "
        "                    \"right\" : {\n                          \"left\" "
        ": {\n                            \"node\" : \"number_literal\",\n     "
        "                       \"root\" : 3\n                          },\n   "
        "                       \"node\" : \"relation_expression\",\n          "
        "                \"right\" : {\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 3\n       "
        "                   },\n                          \"root\" : [\"+\"]\n "
        "                       },\n                        \"root\" : "
        "[\"||\"]\n                      },\n                      \"root\" : "
        "[\"||\"]\n                    },\n                    \"root\" : "
        "[\"+\"]\n                  },\n                  \"root\" : [\"=\", "
        "null]\n                }]],\n            \"node\" : \"statement\",\n  "
        "          \"root\" : \"rvalue\"\n          }");

    credence::Symbol_Table<> symbols{};
    type::RValue::Value null = type::NULL_LITERAL;

    Instructions test_instructions{};
    int temporary{ 0 };
    symbols.table_.emplace("x", null);
    symbols.table_.emplace("putchar", null);
    symbols.table_.emplace("getchar", null);
    symbols.table_.emplace("double", null);
    symbols.table_.emplace("exp", null);
    symbols.table_.emplace("puts", null);
    symbols.table_.emplace("y", null);
    // clang-format off
        std::ostringstream os_test;
    std::string expected_1 = R"qaud(PUSH (5:int:4);
PUSH (2:int:4);
CALL exp;
POP 16;
_t1 = RET;
_t2 = _t1;
_t3 = (5:int:4) + _t2;
_t4 = (5:int:4) * _t3;
_t5 = (2:int:4) ^ _t4;
_t6 = ~ (4:int:4);
_t7 = _t5 / _t6;
)qaud";
    test_instructions = build_from_rvalue_statement(symbols, obj["test"], obj, &temporary);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    CHECK(expected_1 == os_test.str());
    os_test.str("");
    os_test.clear();
    temporary = 0;
    test_instructions.clear();
    test_instructions = build_from_rvalue_statement(symbols, obj["nested_binary"], obj, &temporary);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    std::string expected_2 = R"qaud(y = (3:int:4);
_t1 = y == (3:int:4);
_t2 = y > (2:int:4);
_t3 = _t1 && _t2;
x = _t3;
)qaud";
    CHECK(expected_2 == os_test.str());
    os_test.str("");
    os_test.clear();
    test_instructions.clear();
    temporary = 0;
    test_instructions = build_from_rvalue_statement(symbols, obj["nested_or"], obj, &temporary);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    std::string expected_3 = R"qaud(y = (3:int:4);
_t1 = (2:int:4) || (3:int:4);
_t2 = (1:int:4) || _t1;
x = _t2;
)qaud";
    CHECK(expected_3 == os_test.str());
    os_test.str("");
    os_test.clear();
    test_instructions.clear();
    temporary = 0;
    test_instructions = build_from_rvalue_statement(symbols, obj["complex_or"], obj, &temporary);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    std::string expected_4 = R"qaud(y = (3:int:4);
_t1 = (3:int:4) + (3:int:4);
_t2 = (2:int:4) || _t1;
_t3 = (2:int:4) + _t2;
_t4 = (1:int:4) || _t3;
_t5 = (1:int:4) + _t4;
x = _t5;
)qaud";
    CHECK(expected_4 == os_test.str());
    os_test.str("");
    os_test.clear();
    test_instructions.clear();
    temporary = 0;
    test_instructions = build_from_rvalue_statement(symbols, obj["or_with_call"], obj, &temporary);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    std::string expected_5 = R"qaud(y = (3:int:4);
PUSH (5:int:4);
CALL putchar;
POP 8;
_t1 = RET;
PUSH (1:int:4);
CALL getchar;
POP 8;
_t2 = RET;
_t3 = _t2;
_t4 = (1:int:4) || _t3;
_t5 = (1:int:4) + _t4;
_t6 = (3:int:4) + _t5;
x = (3:int:4) || _t6;
)qaud";
    CHECK(expected_5 == os_test.str());
    os_test.str("");
    os_test.clear();
    test_instructions.clear();
    // clang-format on
}

TEST_CASE("ir/qaud.cc: build_from_auto_statement")
{
    using namespace credence;
    using namespace credence::ir;
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

    credence::Symbol_Table<> symbols{};
    build_from_auto_statement(symbols, obj["test"]);
    CHECK(symbols.table_.size() == 3);

    CHECK(symbols.table_.contains("x") == true);
    CHECK(symbols.table_.contains("y") == true);
    CHECK(symbols.table_.contains("z") == true);

    type::Value_Type empty_value =
        std::make_pair(std::monostate(), type::LITERAL_TYPE.at("null"));
    type::Value_Type word_value =
        std::make_pair("__WORD__", type::LITERAL_TYPE.at("word"));
    type::Value_Type byte_value = std::make_pair(static_cast<type::Byte>('0'),
                                                 std::make_pair("byte", 50));

    CHECK(symbols.table_["x"] == byte_value);
    CHECK(symbols.table_["y"] == word_value);
    CHECK(symbols.table_["z"] == empty_value);
}

TEST_CASE("ir/qaud.cc: deep-evaluated rvalue")
{
    using namespace credence;
    using namespace credence::ir;
    json::JSON obj;
    auto internal_symbols = json::JSON::Load(
        "{\n  \"arg\" : {\n    \"column\" : 6,\n    \"end_column\" : 9,\n    "
        "\"end_pos\" : 8,\n    \"line\" : 1,\n    \"start_pos\" : 5,\n    "
        "\"type\" : \"lvalue\"\n  },\n  \"exp\" : {\n    \"column\" : 1,\n    "
        "\"end_column\" : 4,\n    \"end_pos\" : 52,\n    \"line\" : 6,\n    "
        "\"start_pos\" : 49,\n    \"type\" : \"function_definition\"\n  },\n  "
        "\"main\" : {\n    \"column\" : 1,\n    \"end_column\" : 5,\n    "
        "\"end_pos\" : 4,\n    \"line\" : 1,\n    \"start_pos\" : 0,\n    "
        "\"type\" : \"function_definition\"\n  },\n  \"x\" : {\n    \"column\" "
        ": 8,\n    \"end_column\" : 9,\n    \"end_pos\" : 20,\n    \"line\" : "
        "2,\n    \"start_pos\" : 19,\n    \"type\" : \"lvalue\"\n  },\n  \"y\" "
        ": {\n    \"column\" : 7,\n    \"end_column\" : 8,\n    \"end_pos\" : "
        "56,\n    \"line\" : 6,\n    \"start_pos\" : 55,\n    \"type\" : "
        "\"lvalue\"\n  }\n}");
    obj["test"] = json::JSON::Load(
        "{\n            \"left\" : [[{\n                  \"left\" : {\n       "
        "             \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"x\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"left\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n       "
        "                 \"left\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 5\n         "
        "               },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : {\n     "
        "                     \"node\" : \"number_literal\",\n                 "
        "         \"root\" : 5\n                        },\n                   "
        "     \"root\" : [\"+\"]\n                      }\n                    "
        "},\n                    \"node\" : \"relation_expression\",\n         "
        "           \"right\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n       "
        "                 \"left\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 6\n         "
        "               },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : {\n     "
        "                     \"node\" : \"number_literal\",\n                 "
        "         \"root\" : 6\n                        },\n                   "
        "     \"root\" : [\"+\"]\n                      }\n                    "
        "},\n                    \"root\" : [\"*\"]\n                  },\n    "
        "              \"root\" : [\"=\", null]\n                }]],\n        "
        "    \"node\" : \"statement\",\n            \"root\" : \"rvalue\"\n    "
        "      }");

    credence::Symbol_Table<> symbols{};
    Instructions test_instructions{};
    std::ostringstream os_test;
    int temporary{ 0 };
    type::RValue::Value null = type::NULL_LITERAL;

    symbols.table_.emplace("x", null);
    test_instructions = build_from_rvalue_statement(
        symbols, obj["test"], internal_symbols, &temporary);
    std::string expected = R"qaud(_t1 = (5:int:4) + (5:int:4);
_t2 = (6:int:4) + (6:int:4);
_t3 = _t1 * _t2;
x = _t3;
)qaud";
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    CHECK(os_test.str() == expected);
}