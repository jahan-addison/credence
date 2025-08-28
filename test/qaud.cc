#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase
#include <iostream>          // for ostringstream, cout
#include <map>               // for map
#include <ostream>           // for basic_ostream, operator<<
#include <roxas/ir/qaud.h>   // for emit_quadruple, build_from_rval...
#include <roxas/json.h>      // for JSON
#include <roxas/symbol.h>    // for Symbol_Table
#include <roxas/types.h>     // for Type_, Value_Type, Byte, RValue
#include <sstream>           // for basic_ostringstream
#include <string>            // for basic_string, allocator, char_t...
#include <utility>           // for pair, make_pair
#include <variant>           // for monostate

TEST_CASE("ir/qaud.cc: build_from_function_definition")
{
    using namespace roxas;
    using namespace roxas::ir;
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

    roxas::Symbol_Table<> symbols{};
    Instructions test_instructions{};
    std::ostringstream os_test;
    test_instructions =
        build_from_function_definition(symbols, obj["test"], internal_symbols);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    std::string expected = R"qaud(__main:
 BeginFunc ;
PUSH (5:int:4);
PUSH (2:int:4);
CALL exp;
POP 16;
_t1 = RET;
_t2 = (5:int:4) + _t1;
x = (5:int:4) * _t2;
 EndFunc ;
)qaud";
    CHECK(os_test.str() == expected);
}

TEST_CASE("ir/qaud.cc: build_from_return_statement")
{
    using namespace roxas;
    using namespace roxas::ir;
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

    roxas::Symbol_Table<> symbols{};
    Instructions test_instructions{};
    std::ostringstream os_test;
    type::RValue::Value null = { std::monostate(), type::Type_["null"] };
    symbols.table_.emplace("x", null);
    symbols.table_.emplace("y", null);
    std::cout << "test: " << obj["test"].dump() << std::endl;
    test_instructions =
        build_from_return_statement(symbols, obj["test"], internal_symbols);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    std::string expected = R"qaud(_t1 = y * y;
_t2 = x * _t1;
 LEAVE ;
)qaud";
    CHECK(os_test.str() == expected);
}

TEST_CASE("ir/qaud.cc: build_from_block_statement")
{
    using namespace roxas;
    using namespace roxas::ir;
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

    roxas::Symbol_Table<> symbols{};
    Instructions test_instructions{};
    std::ostringstream os_test;
    test_instructions = build_from_block_statement(symbols, obj["test"], obj);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    CHECK(os_test.str() == "x = (5:int:4) || (2:int:4);\n");
}

TEST_CASE("ir/qaud.cc: build_from_rvalue_statement")
{
    using namespace roxas;
    using namespace roxas::ir;
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
        "\"lvalue\",\n                      \"root\" : \"y\"\n                 "
        "   },\n                    \"node\" : \"relation_expression\",\n      "
        "              \"right\" : {\n                      \"left\" : {\n     "
        "                   \"node\" : \"number_literal\",\n                   "
        "     \"root\" : 3\n                      },\n                      "
        "\"node\" : \"relation_expression\",\n                      \"right\" "
        ": {\n                        \"left\" : {\n                          "
        "\"node\" : \"lvalue\",\n                          \"root\" : \"y\"\n  "
        "                      },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : {\n     "
        "                     \"node\" : \"number_literal\",\n                 "
        "         \"root\" : 2\n                        },\n                   "
        "     \"root\" : [\">\"]\n                      },\n                   "
        "   \"root\" : [\"&&\"]\n                    },\n                    "
        "\"root\" : [\"==\"]\n                  },\n                  \"root\" "
        ": [\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }");
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

    roxas::Symbol_Table<> symbols{};
    type::RValue::Value null = { std::monostate(), type::Type_["null"] };
    Instructions test_instructions{};
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
_t2 = (5:int:4) + _t1;
_t3 = (5:int:4) * _t2;
_t4 = (2:int:4) ^ _t3;
_t5 = ~ (4:int:4);
_t6 = _t4 / _t5;
)qaud";
    test_instructions = build_from_rvalue_statement(symbols, obj["test"], obj);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    CHECK(expected_1 == os_test.str());
    os_test.str("");
    os_test.clear();
    test_instructions.clear();
    test_instructions = build_from_rvalue_statement(symbols, obj["nested_binary"], obj);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    std::string expected_2 = R"qaud(y = (3:int:4);
_t1 = y > (2:int:4);
_t2 = (3:int:4) && _t1;
x = y == _t2;
)qaud";
    CHECK(expected_2 == os_test.str());
    os_test.str("");
    os_test.clear();
    test_instructions.clear();
    test_instructions = build_from_rvalue_statement(symbols, obj["nested_or"], obj);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    std::string expected_3 = R"qaud(y = (3:int:4);
_t1 = (2:int:4) || (3:int:4);
x = (1:int:4) || _t1;
)qaud";
    CHECK(expected_3 == os_test.str());
    os_test.str("");
    os_test.clear();
    test_instructions.clear();
    test_instructions = build_from_rvalue_statement(symbols, obj["complex_or"], obj);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    std::string expected_4 = R"qaud(y = (3:int:4);
_t1 = (3:int:4) + (3:int:4);
_t2 = (2:int:4) || _t1;
_t3 = (2:int:4) + _t2;
_t4 = (1:int:4) || _t3;
x = (1:int:4) + _t4;
)qaud";
    CHECK(expected_4 == os_test.str());
    os_test.str("");
    os_test.clear();
    test_instructions.clear();
    test_instructions = build_from_rvalue_statement(symbols, obj["or_with_call"], obj);
    for (auto const& inst : test_instructions) {
        emit_quadruple(os_test, inst);
    }
    std::string expected_5 = R"qaud(y = (3:int:4);
PUSH (5:int:4);
CALL putchar;
POP 8;
PUSH (1:int:4);
CALL getchar;
POP 8;
_t1 = RET;
_t2 = (1:int:4) || _t1;
_t3 = (1:int:4) + _t2;
_t4 = (3:int:4) + _t3;
x = (3:int:4) || _t4;
)qaud";
    CHECK(expected_5 == os_test.str());
    os_test.str("");
    os_test.clear();
    test_instructions.clear();
    // clang-format on
}

TEST_CASE("ir/qaud.cc: build_from_auto_statement")
{
    using namespace roxas;
    using namespace roxas::ir;
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

    roxas::Symbol_Table<> symbols{};
    build_from_auto_statement(symbols, obj["test"]);
    CHECK(symbols.table_.size() == 3);

    CHECK(symbols.table_.contains("x") == true);
    CHECK(symbols.table_.contains("y") == true);
    CHECK(symbols.table_.contains("z") == true);

    type::Value_Type empty_value =
        std::make_pair(std::monostate(), type::Type_["null"]);
    type::Value_Type word_value =
        std::make_pair("__WORD__", type::Type_["word"]);
    type::Value_Type byte_value = std::make_pair(static_cast<type::Byte>('0'),
                                                 std::make_pair("byte", 50));

    CHECK(symbols.table_["x"] == byte_value);
    CHECK(symbols.table_["y"] == word_value);
    CHECK(symbols.table_["z"] == empty_value);
}

TEST_CASE("ir/qaud.cc: deep-evaluated rvalue")
{
    using namespace roxas;
    using namespace roxas::ir;
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

    roxas::Symbol_Table<> symbols{};
    Instructions test_instructions{};
    std::ostringstream os_test;
    type::RValue::Value null = { std::monostate(), type::Type_["null"] };
    symbols.table_.emplace("x", null);
    test_instructions =
        build_from_rvalue_statement(symbols, obj["test"], internal_symbols);
    for (auto const& inst : test_instructions) {
        emit_quadruple(std::cout, inst);
    }
}