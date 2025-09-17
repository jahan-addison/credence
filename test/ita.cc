#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase

#include <credence/ir/ita.h>  // for ITA
#include <credence/symbol.h>  // for Symbol_Table
#include <credence/types.h>   // for Value_Type, LITERAL_TYPE, Byte, NULL_L...
#include <credence/util.h>    // for AST_Node
#include <map>                // for map
#include <mapbox/eternal.hpp> // for element, map
#include <simplejson.h>       // for JSON
#include <sstream>            // for basic_ostringstream, ostringstream
#include <string>             // for basic_string, allocator, char_traits
#include <string_view>        // for basic_string_view
#include <tuple>              // for tuple
#include <utility>            // for pair, make_pair
#include <variant>            // for monostate

#define EMIT(os, inst)                    \
  do {                                    \
    credence::ir::ITA::emit_to(os, inst); \
} while(false)

struct ITA_Fixture
{
    static inline credence::ir::ITA ITA_hoisted(
        credence::util::AST_Node const& node)
    {
        return credence::ir::ITA{ node };
    }
    static inline credence::ir::ITA ITA_with_tail_branch(
        credence::util::AST_Node const& node)
    {
        auto ita = credence::ir::ITA{ node };
        auto tail_branch = credence::ir::ITA::make_temporary(&ita.temporary);
        ita.tail_branch = tail_branch;
        return ita;
    }

    credence::ir::ITA ita_;
};

/*
TEST_CASE("ir/ita.cc: build_from_function_definition")
{
    using namespace credence;
    using namespace credence::ir;
    credence::util::AST_Node obj;
    auto internal_symbols = json::JSON::load(
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
    obj["test"] = json::JSON::load(
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
    std::string expected = R"ita(__main:
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
)ita";
    CHECK(os_test.str() == expected);
}

TEST_CASE("ir/ita.cc: build_from_block_statement")
{
    using namespace credence;
    using namespace credence::ir;
    credence::util::AST_Node obj;
    obj["test"] = json::JSON::load(
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

TEST_CASE("ir/ita.cc: build_from_extrn_statement")
{
    using namespace credence;
    using namespace credence::ir;
    credence::util::AST_Node obj;

    obj["test"] = json::JSON::load(
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

TEST_CASE("ir/ita.cc: build_from_vector_definition")
{
    using namespace credence;
    using namespace credence::ir;
    credence::util::AST_Node obj;
    obj["symbols"] = json::JSON::load(
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

    obj["test"] = json::JSON::load(
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
    auto vectors = obj["test"].to_deque();
    build_from_vector_definition(symbols, vectors.at(0), obj["symbols"]);
    CHECK(symbols.is_defined(vectors.at(0)["root"].to_string()) == true);
    auto symbol = symbols.get_symbol_by_name(vectors.at(0)["root"].to_string());
    auto [value, type] = symbol;
    CHECK(std::get<std::string>(value) == "orld");
    CHECK(type.first == "string");
    CHECK(type.second == sizeof(char) * 4);
    build_from_vector_definition(symbols, vectors.at(1), obj["symbols"]);
    CHECK(symbols.is_defined(vectors.at(1)["root"].to_string()) == true);
    CHECK(symbols.is_pointer(vectors.at(1)["root"].to_string()) == true);
    auto vector_of_strings =
        symbols.get_pointer_by_name(vectors.at(1)["root"].to_string());
    CHECK(vector_of_strings.size() == 2);
    CHECK(std::get<std::string>(vector_of_strings.at(0).first) == "too bad");
    CHECK(std::get<std::string>(vector_of_strings.at(1).first) == "tough luck");
}

TEST_CASE("ir/ita.cc: build_from_return_statement")
{
    using namespace credence;
    using namespace credence::ir;
    credence::util::AST_Node obj;
    auto internal_symbols = json::JSON::load(
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
    obj["test"] = json::JSON::load(
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
    std::string expected = R"ita(_t1 = y * y;
_t2 = x * _t1;
RET _t2;
)ita";
    CHECK(os_test.str() == expected);
}

TEST_CASE("ir/ita.cc: build_from_block_statement")
{
    using namespace credence;
    using namespace credence::ir;
    credence::util::AST_Node obj;
    obj["test"] = json::JSON::load(
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

*/

TEST_CASE_FIXTURE(
    ITA_Fixture,
    "ir/ita.cc: while statement branching and nested while and if branching")
{
    using namespace credence;
    credence::util::AST_Node obj;
    obj["symbols"] = json::JSON::load(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"main\": "
        "{\"type\": \"function_definition\", \"line\": 1, \"start_pos\": 0, "
        "\"column\": 1, \"end_pos\": 4, \"end_column\": 5}}");

    obj["while_4"] = json::JSON::load(
        "      {\n        \"left\" : [{\n            \"left\" : [{\n           "
        "     \"node\" : \"lvalue\",\n                \"root\" : \"x\"\n       "
        "       }, {\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"y\"\n              }],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"auto\"\n          }, {\n     "
        "       \"left\" : [[{\n                  \"left\" : {\n               "
        "     \"node\" : \"lvalue\",\n                    \"root\" : \"x\"\n   "
        "               },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 100\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }], [{\n                  \"left\" : "
        "{\n                    \"node\" : \"lvalue\",\n                    "
        "\"root\" : \"y\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 100\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }, {\n   "
        "         \"left\" : {\n              \"left\" : {\n                "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n            "
        "  },\n              \"node\" : \"relation_expression\",\n             "
        " \"right\" : {\n                \"node\" : \"number_literal\",\n      "
        "          \"root\" : 5\n              },\n              \"root\" : "
        "[\">\"]\n            },\n            \"node\" : \"statement\",\n      "
        "      \"right\" : [{\n                \"left\" : [{\n                 "
        "   \"left\" : {\n                      \"left\" : {\n                 "
        "       \"node\" : \"lvalue\",\n                        \"root\" : "
        "\"x\"\n                      },\n                      \"node\" : "
        "\"relation_expression\",\n                      \"right\" : {\n       "
        "                 \"node\" : \"number_literal\",\n                     "
        "   \"root\" : 0\n                      },\n                      "
        "\"root\" : [\">=\"]\n                    },\n                    "
        "\"node\" : \"statement\",\n                    \"right\" : [{\n       "
        "                 \"left\" : [{\n                            \"left\" "
        ": [[{\n                                  \"node\" : "
        "\"post_inc_dec_expression\",\n                                  "
        "\"right\" : {\n                                    \"node\" : "
        "\"lvalue\",\n                                    \"root\" : \"x\"\n   "
        "                               },\n                                  "
        "\"root\" : [\"--\"]\n                                }], [{\n         "
        "                         \"left\" : {\n                               "
        "     \"node\" : \"lvalue\",\n                                    "
        "\"root\" : \"x\"\n                                  },\n              "
        "                    \"node\" : \"assignment_expression\",\n           "
        "                       \"right\" : {\n                                "
        "    \"node\" : \"post_inc_dec_expression\",\n                         "
        "           \"right\" : {\n                                      "
        "\"node\" : \"lvalue\",\n                                      "
        "\"root\" : \"y\"\n                                    },\n            "
        "                        \"root\" : [\"--\"]\n                         "
        "         },\n                                  \"root\" : [\"=\", "
        "null]\n                                }]],\n                         "
        "   \"node\" : \"statement\",\n                            \"root\" : "
        "\"rvalue\"\n                          }],\n                        "
        "\"node\" : \"statement\",\n                        \"root\" : "
        "\"block\"\n                      }],\n                    \"root\" : "
        "\"while\"\n                  }],\n                \"node\" : "
        "\"statement\",\n                \"root\" : \"block\"\n              "
        "}, null],\n            \"root\" : \"if\"\n          }, {\n            "
        "\"left\" : [[{\n                  \"node\" : "
        "\"post_inc_dec_expression\",\n                  \"right\" : {\n       "
        "             \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"x\"\n                  },\n                  \"root\" : [\"++\"]\n  "
        "              }], [{\n                  \"left\" : {\n                "
        "    \"node\" : \"lvalue\",\n                    \"root\" : \"y\"\n    "
        "              },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 5\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }], [{\n                  \"left\" : "
        "{\n                    \"node\" : \"lvalue\",\n                    "
        "\"root\" : \"x\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"left\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n       "
        "                 \"left\" : {\n                          \"node\" : "
        "\"lvalue\",\n                          \"root\" : \"x\"\n             "
        "           },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : {\n     "
        "                     \"node\" : \"lvalue\",\n                         "
        " \"root\" : \"y\"\n                        },\n                       "
        " \"root\" : [\"+\"]\n                      }\n                    "
        "},\n                    \"node\" : \"relation_expression\",\n         "
        "           \"right\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n       "
        "                 \"left\" : {\n                          \"node\" : "
        "\"lvalue\",\n                          \"root\" : \"x\"\n             "
        "           },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : {\n     "
        "                     \"node\" : \"lvalue\",\n                         "
        " \"root\" : \"x\"\n                        },\n                       "
        " \"root\" : [\"+\"]\n                      }\n                    "
        "},\n                    \"root\" : [\"*\"]\n                  },\n    "
        "              \"root\" : [\"=\", null]\n                }]],\n        "
        "    \"node\" : \"statement\",\n            \"root\" : \"rvalue\"\n    "
        "      }],\n        \"node\" : \"statement\",\n        \"root\" : "
        "\"block\"\n      }");

    obj["while_2"] = json::JSON::load(
        "       {\n        \"left\" : [{\n            \"left\" : [{\n          "
        "      \"node\" : \"lvalue\",\n                \"root\" : \"x\"\n      "
        "        }, {\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"y\"\n              }],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"auto\"\n          }, {\n     "
        "       \"left\" : [[{\n                  \"left\" : {\n               "
        "     \"node\" : \"lvalue\",\n                    \"root\" : \"x\"\n   "
        "               },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 1\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }], [{\n                  \"left\" : "
        "{\n                    \"node\" : \"lvalue\",\n                    "
        "\"root\" : \"y\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 10\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }, {\n   "
        "         \"left\" : {\n              \"left\" : {\n                "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n            "
        "  },\n              \"node\" : \"relation_expression\",\n             "
        " \"right\" : {\n                \"node\" : \"number_literal\",\n      "
        "          \"root\" : 0\n              },\n              \"root\" : "
        "[\">=\"]\n            },\n            \"node\" : \"statement\",\n     "
        "       \"right\" : [{\n                \"left\" : [{\n                "
        "    \"left\" : [[{\n                          \"node\" : "
        "\"post_inc_dec_expression\",\n                          \"right\" : "
        "{\n                            \"node\" : \"lvalue\",\n               "
        "             \"root\" : \"x\"\n                          },\n         "
        "                 \"root\" : [\"--\"]\n                        }], "
        "[{\n                          \"left\" : {\n                          "
        "  \"node\" : \"lvalue\",\n                            \"root\" : "
        "\"x\"\n                          },\n                          "
        "\"node\" : \"assignment_expression\",\n                          "
        "\"right\" : {\n                            \"node\" : "
        "\"post_inc_dec_expression\",\n                            \"right\" : "
        "{\n                              \"node\" : \"lvalue\",\n             "
        "                 \"root\" : \"y\"\n                            },\n   "
        "                         \"root\" : [\"--\"]\n                        "
        "  },\n                          \"root\" : [\"=\", null]\n            "
        "            }]],\n                    \"node\" : \"statement\",\n     "
        "               \"root\" : \"rvalue\"\n                  }],\n         "
        "       \"node\" : \"statement\",\n                \"root\" : "
        "\"block\"\n              }],\n            \"root\" : \"while\"\n      "
        "    }, {\n            \"left\" : {\n              \"left\" : {\n      "
        "          \"node\" : \"lvalue\",\n                \"root\" : \"x\"\n  "
        "            },\n              \"node\" : \"relation_expression\",\n   "
        "           \"right\" : {\n                \"node\" : "
        "\"number_literal\",\n                \"root\" : 100\n              "
        "},\n              \"root\" : [\"<=\"]\n            },\n            "
        "\"node\" : \"statement\",\n            \"right\" : [{\n               "
        " \"left\" : [{\n                    \"left\" : [[{\n                  "
        "        \"node\" : \"post_inc_dec_expression\",\n                     "
        "     \"right\" : {\n                            \"node\" : "
        "\"lvalue\",\n                            \"root\" : \"x\"\n           "
        "               },\n                          \"root\" : [\"++\"]\n    "
        "                    }]],\n                    \"node\" : "
        "\"statement\",\n                    \"root\" : \"rvalue\"\n           "
        "       }],\n                \"node\" : \"statement\",\n               "
        " \"root\" : \"block\"\n              }],\n            \"root\" : "
        "\"while\"\n          }, {\n            \"left\" : [[{\n               "
        "   \"left\" : {\n                    \"node\" : \"lvalue\",\n         "
        "           \"root\" : \"x\"\n                  },\n                  "
        "\"node\" : \"assignment_expression\",\n                  \"right\" : "
        "{\n                    \"node\" : \"number_literal\",\n               "
        "     \"root\" : 2\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }],\n    "
        "    \"node\" : \"statement\",\n        \"root\" : \"block\"\n      }");

    obj["while"] = json::JSON::load(
        "{\n        \"left\" : [{\n            \"left\" : [{\n                "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n            "
        "  }, {\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"y\"\n              }],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"auto\"\n          }, {\n     "
        "       \"left\" : [[{\n                  \"left\" : {\n               "
        "     \"node\" : \"lvalue\",\n                    \"root\" : \"x\"\n   "
        "               },\n                  \"node\" : "
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
        "\"number_literal\",\n                          \"root\" : 3\n         "
        "               },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : {\n     "
        "                     \"node\" : \"number_literal\",\n                 "
        "         \"root\" : 3\n                        },\n                   "
        "     \"root\" : [\"+\"]\n                      }\n                    "
        "},\n                    \"root\" : [\"*\"]\n                  },\n    "
        "              \"root\" : [\"=\", null]\n                }], [{\n      "
        "            \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"y\"\n                  "
        "},\n                  \"node\" : \"assignment_expression\",\n         "
        "         \"right\" : {\n                    \"node\" : "
        "\"number_literal\",\n                    \"root\" : 100\n             "
        "     },\n                  \"root\" : [\"=\", null]\n                "
        "}]],\n            \"node\" : \"statement\",\n            \"root\" : "
        "\"rvalue\"\n          }, {\n            \"left\" : {\n              "
        "\"left\" : {\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"x\"\n              },\n              \"node\" : "
        "\"relation_expression\",\n              \"right\" : {\n               "
        " \"node\" : \"number_literal\",\n                \"root\" : 0\n       "
        "       },\n              \"root\" : [\">\"]\n            },\n         "
        "   \"node\" : \"statement\",\n            \"right\" : [{\n            "
        "    \"left\" : [{\n                    \"left\" : {\n                 "
        "     \"left\" : {\n                        \"node\" : \"lvalue\",\n   "
        "                     \"root\" : \"x\"\n                      },\n     "
        "                 \"node\" : \"relation_expression\",\n                "
        "      \"right\" : {\n                        \"node\" : "
        "\"number_literal\",\n                        \"root\" : 0\n           "
        "           },\n                      \"root\" : [\">=\"]\n            "
        "        },\n                    \"node\" : \"statement\",\n           "
        "         \"right\" : [{\n                        \"left\" : [{\n      "
        "                      \"left\" : [[{\n                                "
        "  \"node\" : \"post_inc_dec_expression\",\n                           "
        "       \"right\" : {\n                                    \"node\" : "
        "\"lvalue\",\n                                    \"root\" : \"x\"\n   "
        "                               },\n                                  "
        "\"root\" : [\"--\"]\n                                }], [{\n         "
        "                         \"left\" : {\n                               "
        "     \"node\" : \"lvalue\",\n                                    "
        "\"root\" : \"x\"\n                                  },\n              "
        "                    \"node\" : \"assignment_expression\",\n           "
        "                       \"right\" : {\n                                "
        "    \"node\" : \"post_inc_dec_expression\",\n                         "
        "           \"right\" : {\n                                      "
        "\"node\" : \"lvalue\",\n                                      "
        "\"root\" : \"y\"\n                                    },\n            "
        "                        \"root\" : [\"--\"]\n                         "
        "         },\n                                  \"root\" : [\"=\", "
        "null]\n                                }]],\n                         "
        "   \"node\" : \"statement\",\n                            \"root\" : "
        "\"rvalue\"\n                          }],\n                        "
        "\"node\" : \"statement\",\n                        \"root\" : "
        "\"block\"\n                      }],\n                    \"root\" : "
        "\"while\"\n                  }],\n                \"node\" : "
        "\"statement\",\n                \"root\" : \"block\"\n              "
        "}, {\n                \"left\" : [{\n                    \"left\" : "
        "[[{\n                          \"node\" : "
        "\"post_inc_dec_expression\",\n                          \"right\" : "
        "{\n                            \"node\" : \"lvalue\",\n               "
        "             \"root\" : \"x\"\n                          },\n         "
        "                 \"root\" : [\"++\"]\n                        }], "
        "[{\n                          \"left\" : {\n                          "
        "  \"node\" : \"lvalue\",\n                            \"root\" : "
        "\"y\"\n                          },\n                          "
        "\"node\" : \"assignment_expression\",\n                          "
        "\"right\" : {\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 5\n       "
        "                   },\n                          \"root\" : [\"=\", "
        "null]\n                        }], [{\n                          "
        "\"left\" : {\n                            \"node\" : \"lvalue\",\n    "
        "                        \"root\" : \"x\"\n                          "
        "},\n                          \"node\" : \"assignment_expression\",\n "
        "                         \"right\" : {\n                            "
        "\"left\" : {\n                              \"node\" : "
        "\"evaluated_expression\",\n                              \"root\" : "
        "{\n                                \"left\" : {\n                     "
        "             \"node\" : \"lvalue\",\n                                 "
        " \"root\" : \"x\"\n                                },\n               "
        "                 \"node\" : \"relation_expression\",\n                "
        "                \"right\" : {\n                                  "
        "\"node\" : \"lvalue\",\n                                  \"root\" : "
        "\"y\"\n                                },\n                           "
        "     \"root\" : [\"+\"]\n                              }\n            "
        "                },\n                            \"node\" : "
        "\"relation_expression\",\n                            \"right\" : {\n "
        "                             \"node\" : \"evaluated_expression\",\n   "
        "                           \"root\" : {\n                             "
        "   \"left\" : {\n                                  \"node\" : "
        "\"lvalue\",\n                                  \"root\" : \"x\"\n     "
        "                           },\n                                "
        "\"node\" : \"relation_expression\",\n                                "
        "\"right\" : {\n                                  \"node\" : "
        "\"lvalue\",\n                                  \"root\" : \"x\"\n     "
        "                           },\n                                "
        "\"root\" : [\"+\"]\n                              }\n                 "
        "           },\n                            \"root\" : [\"*\"]\n       "
        "                   },\n                          \"root\" : [\"=\", "
        "null]\n                        }]],\n                    \"node\" : "
        "\"statement\",\n                    \"root\" : \"rvalue\"\n           "
        "       }],\n                \"node\" : \"statement\",\n               "
        " \"root\" : \"block\"\n              }],\n            \"root\" : "
        "\"if\"\n          }, {\n            \"left\" : [[{\n                  "
        "\"left\" : {\n                    \"node\" : \"lvalue\",\n            "
        "        \"root\" : \"x\"\n                  },\n                  "
        "\"node\" : \"assignment_expression\",\n                  \"right\" : "
        "{\n                    \"node\" : \"number_literal\",\n               "
        "     \"root\" : 2\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }],\n    "
        "    \"node\" : \"statement\",\n        \"root\" : \"block\"\n      }");

    std::string expected = R"ita(x = (100:int:4);
y = (100:int:4);
_t2 = x > (5:int:4);
IF _t2 GOTO _L3;
_L1:
_t10 = ++ x;
y = (5:int:4);
_t11 = x + y;
_t12 = x + x;
_t13 = _t11 * _t12;
x = _t13;
_L9:
LEAVE;
_L3:
_t6 = x >= (0:int:4);
_L4:
IF _t6 GOTO _L5;
GOTO _L1;
_L5:
_t7 = -- x;
_t8 = -- y;
x = _t8;
GOTO _L4;
)ita";
    std::string expected_2 = R"ita(x = (1:int:4);
y = (10:int:4);
_t3 = x >= (0:int:4);
_L1:
IF _t3 GOTO _L2;
GOTO _L1;
_L1:
_L6:
_t9 = x <= (100:int:4);
_L7:
IF _t9 GOTO _L8;
GOTO _L6;
_L6:
_L11:
x = (2:int:4);
LEAVE;
_L2:
_t4 = -- x;
_t5 = -- y;
x = _t5;
GOTO _L1;
_L8:
_t10 = ++ x;
GOTO _L7;
)ita";
    std::string expected_3 = R"ita(_t1 = (5:int:4) + (5:int:4);
_t2 = (3:int:4) + (3:int:4);
_t3 = _t1 * _t2;
x = _t3;
y = (100:int:4);
_t4 = x > (0:int:4);
IF _t4 GOTO _L5;
GOTO _L12;
_L1:
_L17:
x = (2:int:4);
LEAVE;
_L5:
_t8 = x >= (0:int:4);
_L6:
IF _t8 GOTO _L7;
GOTO _L1;
_L1:
_L11:
_L7:
_t9 = -- x;
_t10 = -- y;
x = _t10;
GOTO _L6;
GOTO _L1;
_L12:
_t13 = ++ x;
y = (5:int:4);
_t14 = x + y;
_t15 = x + x;
_t16 = _t14 * _t15;
x = _t16;
GOTO _L1;
)ita";
    std::ostringstream os_test;
    ir::ITA::Instructions test_instructions{};
    auto hoisted = ITA_with_tail_branch(obj["symbols"]);

    hoisted.symbols_.table_.emplace("add", type::NULL_LITERAL);

    test_instructions =
        hoisted.build_from_block_statement(obj["while_4"], true);
    for (auto const& inst : test_instructions) {
        EMIT(os_test, inst);
    }

    CHECK(os_test.str() == expected);
    os_test.str("");
    hoisted.temporary = 0;
    os_test.clear();

    test_instructions =
        hoisted.build_from_block_statement(obj["while_2"], true);
    for (auto const& inst : test_instructions) {
        EMIT(os_test, inst);
    }

    // CHECK(os_test.str() == expected_2);
    //  os_test.str("");
    //  hoisted.temporary = 0;
    //  os_test.clear();

    // test_instructions = hoisted.build_from_block_statement(obj["while"],
    // true); for (auto const& inst : test_instructions) {
    //     EMIT(os_test, inst);
    // }

    // CHECK(os_test.str() == expected_3);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: if and else branching")
{
    using namespace credence;
    credence::util::AST_Node obj;
    obj["symbols"] = json::JSON::load(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"main\": "
        "{\"type\": \"function_definition\", \"line\": 1, \"start_pos\": 0, "
        "\"column\": 1, \"end_pos\": 4, \"end_column\": 5}}");

    obj["test"] = json::JSON::load(
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

    std::ostringstream os_test;

    auto hoisted = ITA_with_tail_branch(obj["symbols"]);
    hoisted.symbols_.table_.emplace("add", type::NULL_LITERAL);
    auto test_instructions =
        hoisted.build_from_block_statement(obj["test"], true);
    for (auto const& inst : test_instructions) {
        EMIT(os_test, inst);
    }
    std::string expected = R"ita(_t2 = (5:int:4) + (5:int:4);
_t3 = (3:int:4) + (3:int:4);
_t4 = _t2 * _t3;
x = _t4;
_t5 = x <= (5:int:4);
IF _t5 GOTO _L6;
GOTO _L7;
_L1:
LEAVE;
_L6:
x = (1:int:4);
GOTO _L1;
_L7:
x = (8:int:4);
GOTO _L1;
)ita";
    CHECK(os_test.str() == expected);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: truthy type coercion")
{
    using namespace credence;
    util::AST_Node obj;
    obj["symbols"] = json::JSON::load(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"y\": {\"type\": "
        "\"lvalue\", \"line\": 2, \"start_pos\": 19, \"column\": 11, "
        "\"end_pos\": 20, \"end_column\": 12}, \"main\": {\"type\": "
        "\"function_definition\", \"line\": 1, \"start_pos\": 0, \"column\": "
        "1, \"end_pos\": 4, \"end_column\": 5}}");

    obj["test"] = json::JSON::load(
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

    std::ostringstream os_test;
    auto hoisted = ITA_with_tail_branch(obj["symbols"]);
    hoisted.symbols_.table_.emplace("x", type::NULL_LITERAL);
    hoisted.symbols_.table_.emplace("y", type::NULL_LITERAL);
    auto test_instructions =
        hoisted.build_from_block_statement(obj["test"], true);
    for (auto const& inst : test_instructions) {
        EMIT(os_test, inst);
    }
    std::string expected = R"ita(x = (5:int:4);
_t2 = CMP x;
IF _t2 GOTO _L3;
_L1:
LEAVE;
_L3:
y = (10:int:4);
GOTO _L1;
)ita";
    CHECK(os_test.str() == expected);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: label and goto")
{
    using namespace credence;
    util::AST_Node obj;
    obj["symbols"] = json::JSON::load(
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

    obj["test"] = json::JSON::load(
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

    std::ostringstream os_test;
    type::RValue::Value null = type::NULL_LITERAL;
    auto hoisted = ITA_hoisted(obj["symbols"]);
    hoisted.symbols_.table_.emplace("add", null);
    auto test_instructions = hoisted.build_from_block_statement(obj["test"]);
    for (auto const& inst : test_instructions) {
        EMIT(os_test, inst);
    }
    std::string expected = R"ita(_L_ADD:
PUSH (5:int:4);
PUSH (2:int:4);
CALL add;
POP 16;
_t1 = RET;
x = _t1;
y = (10:int:4);
GOTO ADD;
)ita";
    CHECK(os_test.str() == expected);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_rvalue_statement")
{
    using namespace credence;
    util::AST_Node obj;
    obj["test"] = json::JSON::load(
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
    obj["nested_binary"] = json::JSON::load(
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
    obj["nested_or"] = json::JSON::load(
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
    obj["complex_or"] = json::JSON::load(
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
    obj["or_with_call"] = json::JSON::load(
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

    ir::ITA::Instructions test_instructions;
    type::RValue::Value null = type::NULL_LITERAL;
    ita_.symbols_.table_.emplace("x", null);
    ita_.symbols_.table_.emplace("putchar", null);
    ita_.symbols_.table_.emplace("getchar", null);
    ita_.symbols_.table_.emplace("double", null);
    ita_.symbols_.table_.emplace("exp", null);
    ita_.symbols_.table_.emplace("puts", null);
    ita_.symbols_.table_.emplace("y", null);
    // clang-format off
        std::ostringstream os_test;
    std::string expected_1 = R"ita(PUSH (5:int:4);
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
)ita";
    test_instructions = ita_.build_from_rvalue_statement(obj["test"]);
    for (auto const& inst : test_instructions) {
        EMIT(os_test, inst);
    }
    CHECK(expected_1 == os_test.str());
    os_test.str("");
    os_test.clear();
    ita_.temporary = 0;
    test_instructions.clear();
    test_instructions = ita_.build_from_rvalue_statement(
        obj["nested_binary"]);
    for (auto const& inst : test_instructions) {
        EMIT(os_test, inst);
    }
    std::string expected_2 = R"ita(y = (3:int:4);
_t1 = y == (3:int:4);
_t2 = y > (2:int:4);
_t3 = _t1 && _t2;
x = _t3;
)ita";
    CHECK(expected_2 == os_test.str());
    os_test.str("");
    os_test.clear();
    test_instructions.clear();
    ita_.temporary = 0;
    test_instructions = ita_.build_from_rvalue_statement(
        obj["nested_or"]);
    for (auto const& inst : test_instructions) {
        EMIT(os_test, inst);
    }
    std::string expected_3 = R"ita(y = (3:int:4);
_t1 = (2:int:4) || (3:int:4);
_t2 = (1:int:4) || _t1;
x = _t2;
)ita";
    CHECK(expected_3 == os_test.str());
    os_test.str("");
    os_test.clear();
    test_instructions.clear();
    ita_.temporary = 0;
    test_instructions = ita_.build_from_rvalue_statement(obj["complex_or"]);
    for (auto const& inst : test_instructions) {
        EMIT(os_test, inst);
    }
    std::string expected_4 = R"ita(y = (3:int:4);
_t1 = (3:int:4) + (3:int:4);
_t2 = (2:int:4) || _t1;
_t3 = (2:int:4) + _t2;
_t4 = (1:int:4) || _t3;
_t5 = (1:int:4) + _t4;
x = _t5;
)ita";
    CHECK(expected_4 == os_test.str());
    os_test.str("");
    os_test.clear();
    test_instructions.clear();
    ita_.temporary = 0;
    test_instructions = ita_.build_from_rvalue_statement(
        obj["or_with_call"]);
    for (auto const& inst : test_instructions) {
        EMIT(os_test, inst);
    }
    std::string expected_5 = R"ita(y = (3:int:4);
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
)ita";
    CHECK(expected_5 == os_test.str());
    os_test.str("");
    os_test.clear();
    test_instructions.clear();
    // clang-format on
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_auto_statement")
{
    using namespace credence;
    util::AST_Node obj;
    obj["test"] = json::JSON::load(
        "{\n  \"left\" : [{\n      \"left\" : {\n        \"node\" : "
        "\"number_literal\",\n        \"root\" : 50\n      },\n      \"node\" "
        ": \"vector_lvalue\",\n      \"root\" : \"x\"\n    }, {\n      "
        "\"left\" : {\n        \"node\" : \"lvalue\",\n        \"root\" : "
        "\"y\"\n      },\n      \"node\" : \"indirect_lvalue\",\n      "
        "\"root\" : [\"*\"]\n    }, {\n      \"node\" : \"lvalue\",\n      "
        "\"root\" : \"z\"\n    }],\n  \"node\" : \"statement\",\n  \"root\" : "
        "\"auto\"\n}");

    ita_.build_from_auto_statement(obj["test"]);
    CHECK(ita_.symbols_.table_.size() == 3);
    CHECK(ita_.symbols_.table_.contains("x") == true);
    CHECK(ita_.symbols_.table_.contains("y") == true);
    CHECK(ita_.symbols_.table_.contains("z") == true);

    type::Value_Type empty_value =
        std::make_pair(std::monostate(), type::LITERAL_TYPE.at("null"));
    type::Value_Type word_value =
        std::make_pair("__WORD__", type::LITERAL_TYPE.at("word"));
    type::Value_Type byte_value = std::make_pair(
        static_cast<type::Byte>('0'), std::make_pair("byte", 50));

    CHECK(ita_.symbols_.table_["x"] == byte_value);
    CHECK(ita_.symbols_.table_["y"] == word_value);
    CHECK(ita_.symbols_.table_["z"] == empty_value);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: deep-evaluated rvalue")
{
    using namespace credence;
    util::AST_Node obj;
    auto internal_symbols = json::JSON::load(
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
    obj["test"] = json::JSON::load(
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

    ita_.symbols_.set_symbol_by_name("x", credence::type::NULL_LITERAL);
    std::ostringstream os_test;
    auto test_instructions = ita_.build_from_rvalue_statement(obj["test"]);
    std::string expected = R"ita(_t1 = (5:int:4) + (5:int:4);
_t2 = (6:int:4) + (6:int:4);
_t3 = _t1 * _t2;
x = _t3;
)ita";
    for (auto const& inst : test_instructions) {
        EMIT(os_test, inst);
    }
    CHECK(os_test.str() == expected);
}
