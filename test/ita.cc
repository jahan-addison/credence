#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase

#include <credence/ir/ita.h>  // for ITA
#include <credence/symbol.h>  // for Symbol_Table
#include <credence/types.h>   // for Value_Type, LITERAL_TYPE, Byte, NULL_L...
#include <credence/util.h>    // for AST_Node
#include <deque>              // for deque
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
    using NULL_symbols = std::deque<std::string>;
    using Node = credence::util::AST_Node;
    static inline credence::ir::ITA ITA_hoisted(Node const& node)
    {
        return credence::ir::ITA{ node };
    }
    static inline credence::ir::ITA ITA_with_tail_branch(Node const& node)
    {
        auto ita = credence::ir::ITA{ node };
        auto tail_branch = credence::ir::ITA::make_temporary(&ita.temporary);
        return ita;
    }

    void TEST_BLOCK_STATEMENT_NODE_WITH(
        Node const& symbols,
        Node const& node,
        std::string_view test,
        bool tail = true,
        bool ret = true)
    {
        std::ostringstream os_test;
        auto hoisted =
            tail ? ITA_with_tail_branch(symbols) : ITA_hoisted(symbols);
        hoisted.make_root_branch();
        auto test_instructions = hoisted.build_from_block_statement(node, ret);
        for (auto const& inst : test_instructions) {
            EMIT(os_test, inst);
        }
        REQUIRE(os_test.str() == test);
    }

    void TEST_BLOCK_STATEMENT_NODE_WITH(
        Node const& symbols,
        Node const& node,
        std::string_view test,
        NULL_symbols const& nulls,
        bool tail = true,
        bool ret = true)
    {
        std::ostringstream os_test;
        auto hoisted =
            tail ? ITA_with_tail_branch(symbols) : ITA_hoisted(symbols);
        hoisted.make_root_branch();
        for (auto const& s : nulls)
            hoisted.symbols_.table_.emplace(s, credence::type::NULL_LITERAL);
        auto test_instructions = hoisted.build_from_block_statement(node, ret);
        for (auto const& inst : test_instructions) {
            EMIT(os_test, inst);
        }
        REQUIRE(os_test.str() == test);
    }

    void TEST_RETURN_STATEMENT_NODE_WITH(
        Node const& symbols,
        NULL_symbols const& nulls,
        Node const& node,
        std::string_view test)
    {
        std::ostringstream os_test;
        auto hoisted = ITA_hoisted(symbols);
        for (auto const& s : nulls)
            hoisted.symbols_.table_.emplace(s, credence::type::NULL_LITERAL);
        auto test_instructions = hoisted.build_from_return_statement(node);
        for (auto const& inst : test_instructions) {
            EMIT(os_test, inst);
        }
        REQUIRE(os_test.str() == test);
    }

    void TEST_RVALUE_STATEMENT_NODE_WITH(
        Node const& symbols,
        NULL_symbols const& nulls,
        Node const& node,
        std::string_view test)
    {
        std::ostringstream os_test;
        auto hoisted = ITA_hoisted(symbols);
        for (auto const& s : nulls)
            hoisted.symbols_.table_.emplace(s, credence::type::NULL_LITERAL);
        auto test_instructions = hoisted.build_from_rvalue_statement(node);
        for (auto const& inst : test_instructions) {
            EMIT(os_test, inst);
        }
        REQUIRE(os_test.str() == test);
    }

    credence::ir::ITA ita_;
};

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_function_definition")
{
    using namespace credence;
    credence::util::AST_Node obj;
    auto internal_symbols = credence::util::AST_Node::load(
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
    obj["function_2"] = credence::util::AST_Node::load(
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

    auto os_test = std::ostringstream();
    auto ita = ITA_hoisted(internal_symbols);
    auto test_instructions =
        ita.build_from_function_definition(obj["function_2"]);
    for (auto const& inst : test_instructions) {
        EMIT(os_test, inst);
    }
    std::string expected = R"ita(__main:
 BeginFunc ;
_p1 = (2:int:4);
_p2 = (5:int:4);
PUSH _p2;
PUSH _p1;
CALL exp;
POP 16;
_t2 = RET;
_t3 = _t2;
_t4 = (5:int:4) + _t3;
x = (5:int:4) * _t4;
_L1:
LEAVE;
 EndFunc ;
)ita";
    CHECK(os_test.str() == expected);
}

TEST_CASE_FIXTURE(
    ITA_Fixture,
    "ir/ita.cc: function recursion and tail function calls")
{
    using namespace credence;
    credence::util::AST_Node obj;
    obj["symbols"] = credence::util::AST_Node::load(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"exp\": "
        "{\"type\": \"function_definition\", \"line\": 6, \"start_pos\": 38, "
        "\"column\": 1, \"end_pos\": 41, \"end_column\": 4}, \"main\": "
        "{\"type\": \"function_definition\", \"line\": 1, \"start_pos\": 0, "
        "\"column\": 1, \"end_pos\": 4, \"end_column\": 5}, \"y\": {\"type\": "
        "\"lvalue\", \"line\": 6, \"start_pos\": 44, \"column\": 7, "
        "\"end_pos\": 45, \"end_column\": 8}}");

    obj["recursion"] = credence::util::AST_Node::load(
        "  {\n      \"left\" : [{\n          \"node\" : \"lvalue\",\n          "
        "\"root\" : \"x\"\n        }, {\n          \"node\" : \"lvalue\",\n    "
        "      \"root\" : \"y\"\n        }],\n      \"node\" : "
        "\"function_definition\",\n      \"right\" : {\n        \"left\" : "
        "[{\n            \"left\" : {\n              \"left\" : {\n            "
        "    \"node\" : \"lvalue\",\n                \"root\" : \"x\"\n        "
        "      },\n              \"node\" : \"relation_expression\",\n         "
        "     \"right\" : {\n                \"left\" : {\n                  "
        "\"node\" : \"number_literal\",\n                  \"root\" : 1\n      "
        "          },\n                \"node\" : \"relation_expression\",\n   "
        "             \"right\" : {\n                  \"left\" : {\n          "
        "          \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"y\"\n                  },\n                  \"node\" : "
        "\"relation_expression\",\n                  \"right\" : {\n           "
        "         \"node\" : \"number_literal\",\n                    \"root\" "
        ": 1\n                  },\n                  \"root\" : [\"==\"]\n    "
        "            },\n                \"root\" : [\"||\"]\n              "
        "},\n              \"root\" : [\"==\"]\n            },\n            "
        "\"node\" : \"statement\",\n            \"right\" : [{\n               "
        " \"left\" : [{\n                    \"left\" : {\n                    "
        "  \"node\" : \"lvalue\",\n                      \"root\" : \"x\"\n    "
        "                },\n                    \"node\" : "
        "\"relation_expression\",\n                    \"right\" : {\n         "
        "             \"node\" : \"lvalue\",\n                      \"root\" : "
        "\"y\"\n                    },\n                    \"root\" : "
        "[\"*\"]\n                  }],\n                \"node\" : "
        "\"statement\",\n                \"root\" : \"return\"\n              "
        "}, null],\n            \"root\" : \"if\"\n          }, {\n            "
        "\"left\" : [{\n                \"left\" : {\n                  "
        "\"node\" : \"lvalue\",\n                  \"root\" : \"exp\"\n        "
        "        },\n                \"node\" : \"function_expression\",\n     "
        "           \"right\" : [{\n                    \"left\" : {\n         "
        "             \"node\" : \"lvalue\",\n                      \"root\" : "
        "\"x\"\n                    },\n                    \"node\" : "
        "\"relation_expression\",\n                    \"right\" : {\n         "
        "             \"node\" : \"number_literal\",\n                      "
        "\"root\" : 1\n                    },\n                    \"root\" : "
        "[\"-\"]\n                  }, {\n                    \"left\" : {\n   "
        "                   \"node\" : \"lvalue\",\n                      "
        "\"root\" : \"y\"\n                    },\n                    "
        "\"node\" : \"relation_expression\",\n                    \"right\" : "
        "{\n                      \"node\" : \"number_literal\",\n             "
        "         \"root\" : 1\n                    },\n                    "
        "\"root\" : [\"-\"]\n                  }],\n                \"root\" : "
        "\"exp\"\n              }],\n            \"node\" : \"statement\",\n   "
        "         \"root\" : \"return\"\n          }],\n        \"node\" : "
        "\"statement\",\n        \"root\" : \"block\"\n      },\n      "
        "\"root\" : \"exp\"\n    }");

    std::string expected = R"ita(__exp:
 BeginFunc ;
_L2:
_t5 = y == (1:int:4);
_t6 = (1:int:4) || _t5;
_t7 = x == _t6;
IF _t7 GOTO _L4;
_L3:
_t9 = x - (1:int:4);
_p1 = _t9;
_t10 = y - (1:int:4);
_p2 = _t10;
PUSH _p2;
PUSH _p1;
CALL exp;
POP 16;
_t11 = RET;
RET _t11;
_L1:
LEAVE;
_L4:
_t8 = x * y;
RET _t8;
GOTO _L3;
 EndFunc ;
)ita";
    auto os_test = std::ostringstream();
    auto ita = ITA_hoisted(obj["symbols"]);
    auto test_instructions =
        ita.build_from_function_definition(obj["recursion"]);
    for (auto const& inst : test_instructions) {
        EMIT(os_test, inst);
    }
    CHECK(os_test.str() == expected);
}

TEST_CASE_FIXTURE(
    ITA_Fixture,
    "ir/ita.cc: nested function call and return rvalues")
{
    using namespace credence;
    credence::util::AST_Node obj;
    obj["symbols"] = credence::util::AST_Node::load(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"exp\": "
        "{\"type\": \"function_definition\", \"line\": 6, \"start_pos\": 46, "
        "\"column\": 1, \"end_pos\": 49, \"end_column\": 4}, \"main\": "
        "{\"type\": \"function_definition\", \"line\": 1, \"start_pos\": 0, "
        "\"column\": 1, \"end_pos\": 4, \"end_column\": 5}, \"y\": {\"type\": "
        "\"lvalue\", \"line\": 6, \"start_pos\": 52, \"column\": 7, "
        "\"end_pos\": 53, \"end_column\": 8}, \"sub\": {\"type\": "
        "\"function_definition\", \"line\": 11, \"start_pos\": 81, \"column\": "
        "1, \"end_pos\": 84, \"end_column\": 4}}\n");

    obj["test"] = credence::util::AST_Node::load(
        "\n{\n  \"left\" : [{\n      \"left\" : [null],\n      \"node\" : "
        "\"function_definition\",\n      \"right\" : {\n        \"left\" : "
        "[{\n            \"left\" : [{\n                \"node\" : "
        "\"lvalue\",\n                \"root\" : \"x\"\n              }],\n    "
        "        \"node\" : \"statement\",\n            \"root\" : \"auto\"\n  "
        "        }, {\n            \"left\" : [[{\n                  \"left\" "
        ": {\n                    \"node\" : \"lvalue\",\n                    "
        "\"root\" : \"x\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"left\" : {\n                      \"node\" : "
        "\"lvalue\",\n                      \"root\" : \"exp\"\n               "
        "     },\n                    \"node\" : \"function_expression\",\n    "
        "                \"right\" : [{\n                        \"left\" : "
        "{\n                          \"node\" : \"lvalue\",\n                 "
        "         \"root\" : \"exp\"\n                        },\n             "
        "           \"node\" : \"function_expression\",\n                      "
        "  \"right\" : [{\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 2\n       "
        "                   }, {\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 5\n       "
        "                   }],\n                        \"root\" : \"exp\"\n  "
        "                    }, {\n                        \"node\" : "
        "\"number_literal\",\n                        \"root\" : 2\n           "
        "           }],\n                    \"root\" : \"exp\"\n              "
        "    },\n                  \"root\" : [\"=\", null]\n                "
        "}]],\n            \"node\" : \"statement\",\n            \"root\" : "
        "\"rvalue\"\n          }],\n        \"node\" : \"statement\",\n        "
        "\"root\" : \"block\"\n      },\n      \"root\" : \"main\"\n    }, {\n "
        "     \"left\" : [{\n          \"node\" : \"lvalue\",\n          "
        "\"root\" : \"x\"\n        }, {\n          \"node\" : \"lvalue\",\n    "
        "      \"root\" : \"y\"\n        }],\n      \"node\" : "
        "\"function_definition\",\n      \"right\" : {\n        \"left\" : "
        "[{\n            \"left\" : [{\n                \"node\" : "
        "\"lvalue\",\n                \"root\" : \"x\"\n              }, {\n   "
        "             \"node\" : \"lvalue\",\n                \"root\" : "
        "\"y\"\n              }],\n            \"node\" : \"statement\",\n     "
        "       \"root\" : \"auto\"\n          }, {\n            \"left\" : "
        "[[{\n                  \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n                  "
        "},\n                  \"node\" : \"relation_expression\",\n           "
        "       \"right\" : {\n                    \"node\" : \"lvalue\",\n    "
        "                \"root\" : \"y\"\n                  },\n              "
        "    \"root\" : [\"*\"]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }],\n    "
        "    \"node\" : \"statement\",\n        \"root\" : \"block\"\n      "
        "},\n      \"root\" : \"exp\"\n    }, {\n      \"left\" : [{\n         "
        " \"node\" : \"lvalue\",\n          \"root\" : \"x\"\n        }],\n    "
        "  \"node\" : \"function_definition\",\n      \"right\" : {\n        "
        "\"left\" : [{\n            \"left\" : {\n              \"left\" : {\n "
        "               \"node\" : \"lvalue\",\n                \"root\" : "
        "\"x\"\n              },\n              \"node\" : "
        "\"relation_expression\",\n              \"right\" : {\n               "
        " \"node\" : \"number_literal\",\n                \"root\" : 0\n       "
        "       },\n              \"root\" : [\"==\"]\n            },\n        "
        "    \"node\" : \"statement\",\n            \"right\" : [{\n           "
        "     \"left\" : [{\n                    \"left\" : [{\n               "
        "         \"node\" : \"lvalue\",\n                        \"root\" : "
        "\"x\"\n                      }],\n                    \"node\" : "
        "\"statement\",\n                    \"root\" : \"return\"\n           "
        "       }],\n                \"node\" : \"statement\",\n               "
        " \"root\" : \"block\"\n              }, null],\n            \"root\" "
        ": \"if\"\n          }, {\n            \"left\" : [{\n                "
        "\"left\" : {\n                  \"node\" : \"lvalue\",\n              "
        "    \"root\" : \"sub\"\n                },\n                \"node\" "
        ": \"function_expression\",\n                \"right\" : [{\n          "
        "          \"left\" : {\n                      \"node\" : "
        "\"lvalue\",\n                      \"root\" : \"x\"\n                 "
        "   },\n                    \"node\" : \"relation_expression\",\n      "
        "              \"right\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 1\n             "
        "       },\n                    \"root\" : [\"-\"]\n                  "
        "}],\n                \"root\" : \"sub\"\n              }],\n          "
        "  \"node\" : \"statement\",\n            \"root\" : \"return\"\n      "
        "    }],\n        \"node\" : \"statement\",\n        \"root\" : "
        "\"block\"\n      },\n      \"root\" : \"sub\"\n    }],\n  \"node\" : "
        "\"program\",\n  \"root\" : \"definitions\"\n}\n");

    auto ita = ITA_hoisted(obj["symbols"]);
    auto definitions = obj["test"]["left"].to_deque();
    auto expected = R"ita(_p2 = (2:int:4);
_p3 = (5:int:4);
PUSH _p3;
PUSH _p2;
CALL exp;
POP 16;
_t2 = RET;
_p1 = _t2;
_p4 = (2:int:4);
PUSH _p4;
PUSH _p1;
CALL exp;
POP 16;
_t3 = RET;
x = _t3;
)ita";
    TEST_BLOCK_STATEMENT_NODE_WITH(
        obj["symbols"], definitions[0]["right"], expected, false, false);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_block_statement")
{
    using namespace credence;
    credence::util::AST_Node obj;
    obj["symbols"] = credence::util::AST_Node::load(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"exp\": "
        "{\"type\": \"function_definition\", \"line\": 6, \"start_pos\": 46, "
        "\"column\": 1, \"end_pos\": 49, \"end_column\": 4}, \"main\": "
        "{\"type\": \"function_definition\", \"line\": 1, \"start_pos\": 0, "
        "\"column\": 1, \"end_pos\": 4, \"end_column\": 5}, \"y\": {\"type\": "
        "\"lvalue\", \"line\": 6, \"start_pos\": 52, \"column\": 7, "
        "\"end_pos\": 53, \"end_column\": 8}, \"sub\": {\"type\": "
        "\"function_definition\", \"line\": 11, \"start_pos\": 81, \"column\": "
        "1, \"end_pos\": 84, \"end_column\": 4}}\n");
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
    TEST_BLOCK_STATEMENT_NODE_WITH(
        obj,
        obj["test"],
        "_t2 = (5:int:4) || (2:int:4);\nx = _t2;\n",
        false,
        false);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_extrn_statement")
{
    using namespace credence;
    credence::util::AST_Node obj;

    obj["test"] = credence::util::AST_Node::load(
        "{\n            \"left\" : [{\n                \"node\" : "
        "\"lvalue\",\n                \"root\" : \"a\"\n              }, {\n   "
        "             \"node\" : \"lvalue\",\n                \"root\" : "
        "\"b\"\n              }, {\n                \"node\" : \"lvalue\",\n   "
        "             \"root\" : \"c\"\n              }],\n            "
        "\"node\" : \"statement\",\n            \"root\" : \"extrn\"\n         "
        " }");

    auto vectors = obj["test"].to_deque();
    auto ita = ITA_hoisted(obj["symbols"]);

    CHECK_THROWS(ita.build_from_extrn_statement(obj["test"]));

    ita.globals_.table_.emplace("a", type::NULL_LITERAL);
    ita.globals_.table_.emplace("b", type::NULL_LITERAL);
    ita.globals_.table_.emplace("c", type::NULL_LITERAL);

    CHECK_NOTHROW(ita.build_from_extrn_statement(obj["test"]));

    CHECK_EQ(ita.symbols_.is_defined("a"), true);
    CHECK_EQ(ita.symbols_.is_defined("b"), true);
    CHECK_EQ(ita.symbols_.is_defined("c"), true);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_vector_definition")
{
    using namespace credence;
    credence::util::AST_Node obj;
    obj["symbols"] = credence::util::AST_Node::load(
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

    obj["test"] = credence::util::AST_Node::load(
        "[{\n      \"left\" : {\n        \"node\" : \"string_literal\",\n      "
        "  \"root\" : \"\\\"orld\\\"\"\n      },\n      \"node\" : "
        "\"vector_definition\",\n      \"right\" : [],\n      \"root\" : "
        "\"c\"\n    }, {\n      \"left\" : {\n        \"node\" : "
        "\"number_literal\",\n        \"root\" : 2\n      },\n      \"node\" : "
        "\"vector_definition\",\n      \"right\" : [{\n          \"node\" : "
        "\"string_literal\",\n          \"root\" : \"\\\"too bad\\\"\"\n       "
        " }, {\n          \"node\" : \"string_literal\",\n          \"root\" : "
        "\"\\\"tough luck\\\"\"\n        }]\n    }]");

    auto vectors = obj["test"].to_deque();
    auto ita = ITA_hoisted(obj["symbols"]);
    ita.build_from_vector_definition(vectors.at(0));
    CHECK(ita.symbols_.is_defined(vectors.at(0)["root"].to_string()) == true);
    auto symbol =
        ita.symbols_.get_symbol_by_name(vectors.at(0)["root"].to_string());
    auto [value, type] = symbol;
    CHECK(std::get<std::string>(value) == "orld");
    CHECK(type.first == "string");
    CHECK(type.second == sizeof(char) * 4);
    ita.build_from_vector_definition(vectors.at(1));
    CHECK(ita.symbols_.is_defined(vectors.at(1)["root"].to_string()) == true);
    CHECK(ita.symbols_.is_pointer(vectors.at(1)["root"].to_string()) == true);
    auto vector_of_strings =
        ita.symbols_.get_pointer_by_name(vectors.at(1)["root"].to_string());
    CHECK(vector_of_strings.size() == 2);
    CHECK(std::get<std::string>(vector_of_strings.at(0).first) == "too bad");
    CHECK(std::get<std::string>(vector_of_strings.at(1).first) == "tough luck");
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_return_statement")
{
    using namespace credence;
    credence::util::AST_Node obj;
    auto internal_symbols = credence::util::AST_Node::load(
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
    obj["test"] = credence::util::AST_Node::load(
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

    std::string expected = R"ita(_t1 = y * y;
_t2 = x * _t1;
RET _t2;
)ita";
    TEST_RETURN_STATEMENT_NODE_WITH(
        internal_symbols, { "x", "y" }, obj["test"], expected);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_block_statement")
{
    using namespace credence;
    credence::util::AST_Node obj;
    obj["test"] = credence::util::AST_Node::load(
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

    TEST_BLOCK_STATEMENT_NODE_WITH(
        obj,
        obj["test"],
        "_t2 = (5:int:4) || (2:int:4);\nx = _t2;\n",
        false,
        false);
}

TEST_CASE_FIXTURE(
    ITA_Fixture,
    "ir/ita.cc: while statement branching and nested while and if branching")
{
    using namespace credence;
    credence::util::AST_Node obj;
    obj["symbols"] = credence::util::AST_Node::load(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"main\": "
        "{\"type\": \"function_definition\", \"line\": 1, \"start_pos\": 0, "
        "\"column\": 1, \"end_pos\": 4, \"end_column\": 5}}");

    obj["while_4"] = credence::util::AST_Node::load(
        "{\n        \"left\" : [ {\n            \"left\" : [[{\n              "
        "    \"left\" : {\n                    \"node\" : \"lvalue\",\n        "
        "            \"root\" : \"x\"\n                  },\n                  "
        "\"node\" : \"assignment_expression\",\n                  \"right\" : "
        "{\n                    \"node\" : \"number_literal\",\n               "
        "     \"root\" : 100\n                  },\n                  \"root\" "
        ": [\"=\", null]\n                }], [{\n                  \"left\" : "
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
        "          \"root\" : 100\n              },\n              \"root\" : "
        "[\"==\"]\n            },\n            \"node\" : \"statement\",\n     "
        "       \"right\" : [{\n                \"left\" : [{\n                "
        "    \"left\" : {\n                      \"left\" : {\n                "
        "        \"node\" : \"lvalue\",\n                        \"root\" : "
        "\"x\"\n                      },\n                      \"node\" : "
        "\"relation_expression\",\n                      \"right\" : {\n       "
        "                 \"node\" : \"number_literal\",\n                     "
        "   \"root\" : 5\n                      },\n                      "
        "\"root\" : [\">\"]\n                    },\n                    "
        "\"node\" : \"statement\",\n                    \"right\" : [{\n       "
        "                 \"left\" : [{\n                            \"left\" "
        ": {\n                              \"left\" : {\n                     "
        "           \"node\" : \"lvalue\",\n                                "
        "\"root\" : \"x\"\n                              },\n                  "
        "            \"node\" : \"relation_expression\",\n                     "
        "         \"right\" : {\n                                \"node\" : "
        "\"number_literal\",\n                                \"root\" : 0\n   "
        "                           },\n                              \"root\" "
        ": [\">=\"]\n                            },\n                          "
        "  \"node\" : \"statement\",\n                            \"right\" : "
        "[{\n                                \"left\" : [{\n                   "
        "                 \"left\" : [[{\n                                     "
        "     \"node\" : \"post_inc_dec_expression\",\n                        "
        "                  \"right\" : {\n                                     "
        "       \"node\" : \"lvalue\",\n                                       "
        "     \"root\" : \"x\"\n                                          },\n "
        "                                         \"root\" : [\"--\"]\n        "
        "                                }], [{\n                              "
        "            \"left\" : {\n                                            "
        "\"node\" : \"lvalue\",\n                                            "
        "\"root\" : \"x\"\n                                          },\n      "
        "                                    \"node\" : "
        "\"assignment_expression\",\n                                          "
        "\"right\" : {\n                                            \"node\" : "
        "\"post_inc_dec_expression\",\n                                        "
        "    \"right\" : {\n                                              "
        "\"node\" : \"lvalue\",\n                                              "
        "\"root\" : \"y\"\n                                            },\n    "
        "                                        \"root\" : [\"--\"]\n         "
        "                                 },\n                                 "
        "         \"root\" : [\"=\", null]\n                                   "
        "     }]],\n                                    \"node\" : "
        "\"statement\",\n                                    \"root\" : "
        "\"rvalue\"\n                                  }],\n                   "
        "             \"node\" : \"statement\",\n                              "
        "  \"root\" : \"block\"\n                              }],\n           "
        "                 \"root\" : \"while\"\n                          "
        "}],\n                        \"node\" : \"statement\",\n              "
        "          \"root\" : \"block\"\n                      }, null],\n     "
        "               \"root\" : \"if\"\n                  }],\n             "
        "   \"node\" : \"statement\",\n                \"root\" : \"block\"\n  "
        "            }, null],\n            \"root\" : \"if\"\n          }, "
        "{\n            \"left\" : [[{\n                  \"node\" : "
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

    obj["while_2"] = credence::util::AST_Node::load(
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

    obj["while"] = credence::util::AST_Node::load(
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

    auto expected = R"ita(_t3 = (5:int:4) + (5:int:4);
_t4 = (3:int:4) + (3:int:4);
_t5 = _t3 * _t4;
x = _t5;
y = (100:int:4);
_L6:
_t9 = x > (0:int:4);
IF _t9 GOTO _L8;
GOTO _L16;
_L7:
x = (2:int:4);
_L2:
LEAVE;
_L8:
_L10:
_t13 = x >= (0:int:4);
IF _t13 GOTO _L11;
GOTO _L7;
_L11:
_t14 = -- x;
_t15 = -- y;
x = _t15;
GOTO _L10;
_L16:
_t17 = ++ x;
y = (5:int:4);
_t18 = x + y;
_t19 = x + x;
_t20 = _t18 * _t19;
x = _t20;
GOTO _L7;
)ita";
    auto expected_2 = R"ita(x = (1:int:4);
y = (10:int:4);
_L3:
_t6 = x >= (0:int:4);
IF _t6 GOTO _L4;
_L9:
_t12 = x <= (100:int:4);
IF _t12 GOTO _L10;
x = (2:int:4);
_L2:
LEAVE;
_L4:
_t7 = -- x;
_t8 = -- y;
x = _t8;
GOTO _L3;
_L10:
_t13 = ++ x;
GOTO _L9;
)ita";
    std::string expected_3 = R"ita(x = (100:int:4);
y = (100:int:4);
_L3:
_t6 = x == (100:int:4);
IF _t6 GOTO _L5;
_L4:
_t17 = ++ x;
y = (5:int:4);
_t18 = x + y;
_t19 = x + x;
_t20 = _t18 * _t19;
x = _t20;
_L2:
LEAVE;
_L5:
_L7:
_t10 = x > (5:int:4);
IF _t10 GOTO _L9;
_L8:
GOTO _L4;
_L9:
_L11:
_t14 = x >= (0:int:4);
IF _t14 GOTO _L12;
GOTO _L8;
_L12:
_t15 = -- x;
_t16 = -- y;
x = _t16;
GOTO _L11;
)ita";
    TEST_BLOCK_STATEMENT_NODE_WITH(obj["symbols"], obj["while"], expected);
    TEST_BLOCK_STATEMENT_NODE_WITH(obj["symbols"], obj["while_2"], expected_2);
    TEST_BLOCK_STATEMENT_NODE_WITH(
        obj["symbols"], obj["while_4"], expected_3, { "x", "y" });
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: if and else branching")
{
    using namespace credence;
    credence::util::AST_Node obj;
    obj["symbols"] = credence::util::AST_Node::load(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"y\": {\"type\": "
        "\"lvalue\", \"line\": 2, \"start_pos\": 19, \"column\": 11, "
        "\"end_pos\": 20, \"end_column\": 12}, \"main\": {\"type\": "
        "\"function_definition\", \"line\": 1, \"start_pos\": 0, \"column\": "
        "1, \"end_pos\": 4, \"end_column\": 5}}\n\n");

    obj["if"] = credence::util::AST_Node::load(
        "{\n        \"left\" : [{\n            \"left\" : [{\n                "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n            "
        "  }],\n            \"node\" : \"statement\",\n            \"root\" : "
        "\"auto\"\n          }, \n          {\n            \"left\" : [[{\n    "
        "              \"left\" : {\n                    \"node\" : "
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
        "           \"root\" : 3\n                        },\n                 "
        "       \"node\" : \"relation_expression\",\n                        "
        "\"right\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 3\n         "
        "               },\n                        \"root\" : [\"+\"]\n       "
        "               }\n                    },\n                    "
        "\"root\" : [\"*\"]\n                  },\n                  \"root\" "
        ": [\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }, {\n   "
        "         \"left\" : {\n              \"left\" : {\n                "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n            "
        "  },\n              \"node\" : \"relation_expression\",\n             "
        " \"right\" : {\n                \"node\" : \"number_literal\",\n      "
        "          \"root\" : 5\n              },\n              \"root\" : "
        "[\"<=\"]\n            },\n            \"node\" : \"statement\",\n     "
        "       \"right\" : [{\n                \"left\" : [{\n                "
        "    \"left\" : [[{\n                          \"left\" : {\n          "
        "                  \"node\" : \"lvalue\",\n                            "
        "\"root\" : \"x\"\n                          },\n                      "
        "    \"node\" : \"assignment_expression\",\n                          "
        "\"right\" : {\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 1\n       "
        "                   },\n                          \"root\" : [\"=\", "
        "null]\n                        }]],\n                    \"node\" : "
        "\"statement\",\n                    \"root\" : \"rvalue\"\n           "
        "       }],\n                \"node\" : \"statement\",\n               "
        " \"root\" : \"block\"\n              }, {\n                \"left\" : "
        "[{\n                    \"left\" : [[{\n                          "
        "\"left\" : {\n                            \"node\" : \"lvalue\",\n    "
        "                        \"root\" : \"x\"\n                          "
        "},\n                          \"node\" : \"assignment_expression\",\n "
        "                         \"right\" : {\n                            "
        "\"node\" : \"number_literal\",\n                            \"root\" "
        ": 8\n                          },\n                          \"root\" "
        ": [\"=\", null]\n                        }]],\n                    "
        "\"node\" : \"statement\",\n                    \"root\" : "
        "\"rvalue\"\n                  }],\n                \"node\" : "
        "\"statement\",\n                \"root\" : \"block\"\n              "
        "}],\n            \"root\" : \"if\"\n          }, {\n            "
        "\"left\" : [[{\n                  \"left\" : {\n                    "
        "\"node\" : \"lvalue\",\n                    \"root\" : \"x\"\n        "
        "          },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"left\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 5\n             "
        "       },\n                    \"node\" : \"relation_expression\",\n  "
        "                  \"right\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 2\n             "
        "       },\n                    \"root\" : [\"||\"]\n                  "
        "},\n                  \"root\" : [\"=\", null]\n                "
        "}]],\n            \"node\" : \"statement\",\n            \"root\" : "
        "\"rvalue\"\n          }],\n        \"node\" : \"statement\",\n        "
        "\"root\" : \"block\"\n      }");

    obj["if_2"] = credence::util::AST_Node::load(
        "      {\n        \"left\" : [{\n            \"left\" : [{\n           "
        "     \"node\" : \"lvalue\",\n                \"root\" : \"x\"\n       "
        "       }],\n            \"node\" : \"statement\",\n            "
        "\"root\" : \"auto\"\n          }, {\n            \"left\" : [[{\n     "
        "             \"left\" : {\n                    \"node\" : "
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
        "           \"root\" : 3\n                        },\n                 "
        "       \"node\" : \"relation_expression\",\n                        "
        "\"right\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 3\n         "
        "               },\n                        \"root\" : [\"+\"]\n       "
        "               }\n                    },\n                    "
        "\"root\" : [\"*\"]\n                  },\n                  \"root\" "
        ": [\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }, {\n   "
        "         \"left\" : {\n              \"left\" : {\n                "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n            "
        "  },\n              \"node\" : \"relation_expression\",\n             "
        " \"right\" : {\n                \"node\" : \"number_literal\",\n      "
        "          \"root\" : 5\n              },\n              \"root\" : "
        "[\"<=\"]\n            },\n            \"node\" : \"statement\",\n     "
        "       \"right\" : [{\n                \"left\" : [{\n                "
        "    \"left\" : [[{\n                          \"left\" : {\n          "
        "                  \"node\" : \"lvalue\",\n                            "
        "\"root\" : \"x\"\n                          },\n                      "
        "    \"node\" : \"assignment_expression\",\n                          "
        "\"right\" : {\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 1\n       "
        "                   },\n                          \"root\" : [\"=\", "
        "null]\n                        }]],\n                    \"node\" : "
        "\"statement\",\n                    \"root\" : \"rvalue\"\n           "
        "       }],\n                \"node\" : \"statement\",\n               "
        " \"root\" : \"block\"\n              }, null],\n            \"root\" "
        ": \"if\"\n          }, {\n            \"left\" : {\n              "
        "\"left\" : {\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"x\"\n              },\n              \"node\" : "
        "\"relation_expression\",\n              \"right\" : {\n               "
        " \"node\" : \"number_literal\",\n                \"root\" : 5\n       "
        "       },\n              \"root\" : [\">\"]\n            },\n         "
        "   \"node\" : \"statement\",\n            \"right\" : [{\n            "
        "    \"left\" : [{\n                    \"left\" : [[{\n               "
        "           \"left\" : {\n                            \"node\" : "
        "\"lvalue\",\n                            \"root\" : \"x\"\n           "
        "               },\n                          \"node\" : "
        "\"assignment_expression\",\n                          \"right\" : {\n "
        "                           \"node\" : \"number_literal\",\n           "
        "                 \"root\" : 10\n                          },\n        "
        "                  \"root\" : [\"=\", null]\n                        "
        "}]],\n                    \"node\" : \"statement\",\n                 "
        "   \"root\" : \"rvalue\"\n                  }],\n                "
        "\"node\" : \"statement\",\n                \"root\" : \"block\"\n     "
        "         }, null],\n            \"root\" : \"if\"\n          }, {\n   "
        "         \"left\" : [[{\n                  \"left\" : {\n             "
        "       \"node\" : \"lvalue\",\n                    \"root\" : \"x\"\n "
        "                 },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"left\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 5\n             "
        "       },\n                    \"node\" : \"relation_expression\",\n  "
        "                  \"right\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 2\n             "
        "       },\n                    \"root\" : [\"||\"]\n                  "
        "},\n                  \"root\" : [\"=\", null]\n                "
        "}]],\n            \"node\" : \"statement\",\n            \"root\" : "
        "\"rvalue\"\n          }],\n        \"node\" : \"statement\",\n        "
        "\"root\" : \"block\"\n      }");
    obj["if_5"] = credence::util::AST_Node::load(
        "{\n        \"left\" : [ \n          {\n            \"left\" : [[{\n   "
        "               \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n                  "
        "},\n                  \"node\" : \"assignment_expression\",\n         "
        "         \"right\" : {\n                    \"node\" : "
        "\"number_literal\",\n                    \"root\" : 5\n               "
        "   },\n                  \"root\" : [\"=\", null]\n                "
        "}]],\n            \"node\" : \"statement\",\n            \"root\" : "
        "\"rvalue\"\n          }, {\n            \"left\" : {\n              "
        "\"node\" : \"lvalue\",\n              \"root\" : \"x\"\n            "
        "},\n            \"node\" : \"statement\",\n            \"right\" : "
        "[{\n                \"left\" : [{\n                    \"left\" : "
        "[[{\n                          \"left\" : {\n                         "
        "   \"node\" : \"lvalue\",\n                            \"root\" : "
        "\"y\"\n                          },\n                          "
        "\"node\" : \"assignment_expression\",\n                          "
        "\"right\" : {\n                            \"node\" : \"lvalue\",\n   "
        "                         \"root\" : \"x\"\n                          "
        "},\n                          \"root\" : [\"=\", null]\n              "
        "          }]],\n                    \"node\" : \"statement\",\n       "
        "             \"root\" : \"rvalue\"\n                  }, {\n          "
        "          \"left\" : {\n                      \"left\" : {\n          "
        "              \"node\" : \"lvalue\",\n                        "
        "\"root\" : \"y\"\n                      },\n                      "
        "\"node\" : \"unary_expression\",\n                      \"root\" : "
        "[\"!\"]\n                    },\n                    \"node\" : "
        "\"statement\",\n                    \"right\" : [{\n                  "
        "      \"left\" : [{\n                            \"left\" : {\n       "
        "                       \"left\" : {\n                                "
        "\"node\" : \"lvalue\",\n                                \"root\" : "
        "\"x\"\n                              },\n                             "
        " \"node\" : \"relation_expression\",\n                              "
        "\"right\" : {\n                                \"node\" : "
        "\"lvalue\",\n                                \"root\" : \"y\"\n       "
        "                       },\n                              \"root\" : "
        "[\">\"]\n                            },\n                            "
        "\"node\" : \"statement\",\n                            \"right\" : "
        "[{\n                                \"left\" : [{\n                   "
        "                 \"left\" : {\n                                      "
        "\"left\" : {\n                                        \"node\" : "
        "\"lvalue\",\n                                        \"root\" : "
        "\"x\"\n                                      },\n                     "
        "                 \"node\" : \"unary_expression\",\n                   "
        "                   \"root\" : [\"!\"]\n                               "
        "     },\n                                    \"node\" : "
        "\"statement\",\n                                    \"right\" : [{\n  "
        "                                      \"left\" : [{\n                 "
        "                           \"left\" : [[{\n                           "
        "                       \"left\" : {\n                                 "
        "                   \"node\" : \"lvalue\",\n                           "
        "                         \"root\" : \"x\"\n                           "
        "                       },\n                                           "
        "       \"node\" : \"assignment_expression\",\n                        "
        "                          \"right\" : {\n                             "
        "                       \"node\" : \"lvalue\",\n                       "
        "                             \"root\" : \"y\"\n                       "
        "                           },\n                                       "
        "           \"root\" : [\"=\", null]\n                                 "
        "               }]],\n                                            "
        "\"node\" : \"statement\",\n                                           "
        " \"root\" : \"rvalue\"\n                                          "
        "}],\n                                        \"node\" : "
        "\"statement\",\n                                        \"root\" : "
        "\"block\"\n                                      }, null],\n          "
        "                          \"root\" : \"if\"\n                         "
        "         }, {\n                                    \"left\" : [[{\n   "
        "                                       \"left\" : {\n                 "
        "                           \"node\" : \"lvalue\",\n                   "
        "                         \"root\" : \"x\"\n                           "
        "               },\n                                          \"node\" "
        ": \"assignment_expression\",\n                                        "
        "  \"right\" : {\n                                            \"left\" "
        ": {\n                                              \"node\" : "
        "\"number_literal\",\n                                              "
        "\"root\" : 3\n                                            },\n        "
        "                                    \"node\" : "
        "\"relation_expression\",\n                                            "
        "\"right\" : {\n                                              \"node\" "
        ": \"number_literal\",\n                                              "
        "\"root\" : 3\n                                            },\n        "
        "                                    \"root\" : [\"+\"]\n              "
        "                            },\n                                      "
        "    \"root\" : [\"=\", null]\n                                        "
        "}]],\n                                    \"node\" : \"statement\",\n "
        "                                   \"root\" : \"rvalue\"\n            "
        "                      }],\n                                \"node\" : "
        "\"statement\",\n                                \"root\" : "
        "\"block\"\n                              }, null],\n                  "
        "          \"root\" : \"if\"\n                          }, {\n         "
        "                   \"left\" : [[{\n                                  "
        "\"left\" : {\n                                    \"node\" : "
        "\"lvalue\",\n                                    \"root\" : \"x\"\n   "
        "                               },\n                                  "
        "\"node\" : \"assignment_expression\",\n                               "
        "   \"right\" : {\n                                    \"left\" : {\n  "
        "                                    \"node\" : \"number_literal\",\n  "
        "                                    \"root\" : 1\n                    "
        "                },\n                                    \"node\" : "
        "\"relation_expression\",\n                                    "
        "\"right\" : {\n                                      \"node\" : "
        "\"number_literal\",\n                                      \"root\" : "
        "1\n                                    },\n                           "
        "         \"root\" : [\"+\"]\n                                  },\n   "
        "                               \"root\" : [\"=\", null]\n             "
        "                   }]],\n                            \"node\" : "
        "\"statement\",\n                            \"root\" : \"rvalue\"\n   "
        "                       }],\n                        \"node\" : "
        "\"statement\",\n                        \"root\" : \"block\"\n        "
        "              }, null],\n                    \"root\" : \"if\"\n      "
        "            }, {\n                    \"left\" : [[{\n                "
        "          \"left\" : {\n                            \"node\" : "
        "\"lvalue\",\n                            \"root\" : \"x\"\n           "
        "               },\n                          \"node\" : "
        "\"assignment_expression\",\n                          \"right\" : {\n "
        "                           \"left\" : {\n                             "
        " \"node\" : \"number_literal\",\n                              "
        "\"root\" : 2\n                            },\n                        "
        "    \"node\" : \"relation_expression\",\n                            "
        "\"right\" : {\n                              \"node\" : "
        "\"number_literal\",\n                              \"root\" : 2\n     "
        "                       },\n                            \"root\" : "
        "[\"+\"]\n                          },\n                          "
        "\"root\" : [\"=\", null]\n                        }]],\n              "
        "      \"node\" : \"statement\",\n                    \"root\" : "
        "\"rvalue\"\n                  }],\n                \"node\" : "
        "\"statement\",\n                \"root\" : \"block\"\n              "
        "}, null],\n            \"root\" : \"if\"\n          }],\n        "
        "\"node\" : \"statement\",\n        \"root\" : \"block\"\n      }");

    auto expected = R"ita(_t3 = (5:int:4) + (5:int:4);
_t4 = (3:int:4) + (3:int:4);
_t5 = _t3 * _t4;
x = _t5;
_L6:
_t9 = x <= (5:int:4);
IF _t9 GOTO _L8;
GOTO _L10;
_L7:
_t11 = (5:int:4) || (2:int:4);
x = _t11;
_L2:
LEAVE;
_L8:
x = (1:int:4);
GOTO _L7;
_L10:
x = (8:int:4);
GOTO _L7;
)ita";

    auto expected_2 = R"ita(_t3 = (5:int:4) + (5:int:4);
_t4 = (3:int:4) + (3:int:4);
_t5 = _t3 * _t4;
x = _t5;
_L6:
_t9 = x <= (5:int:4);
IF _t9 GOTO _L8;
_L7:
_L10:
_t13 = x > (5:int:4);
IF _t13 GOTO _L12;
_L11:
_t14 = (5:int:4) || (2:int:4);
x = _t14;
_L2:
LEAVE;
_L8:
x = (1:int:4);
GOTO _L7;
_L12:
x = (10:int:4);
GOTO _L11;
)ita";

    auto expected_3 = R"ita(x = (5:int:4);
_L3:
_t6 = CMP x;
IF _t6 GOTO _L5;
_L4:
_L2:
LEAVE;
_L5:
y = x;
_L7:
_t10 = ! y;
IF _t10 GOTO _L9;
_L8:
_t21 = (2:int:4) + (2:int:4);
x = _t21;
GOTO _L4;
_L9:
_L11:
_t14 = x > y;
IF _t14 GOTO _L13;
_L12:
_t20 = (1:int:4) + (1:int:4);
x = _t20;
GOTO _L8;
_L13:
_L15:
_t18 = ! x;
IF _t18 GOTO _L17;
_L16:
_t19 = (3:int:4) + (3:int:4);
x = _t19;
GOTO _L12;
_L17:
x = y;
GOTO _L16;
)ita";

    TEST_BLOCK_STATEMENT_NODE_WITH(obj["symbols"], obj["if"], expected);
    TEST_BLOCK_STATEMENT_NODE_WITH(obj["symbols"], obj["if_2"], expected_2);
    TEST_BLOCK_STATEMENT_NODE_WITH(
        obj["symbols"], obj["if_5"], expected_3, { "x", "y" });
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: truthy type coercion")
{
    using namespace credence;
    util::AST_Node obj;
    obj["symbols"] = credence::util::AST_Node::load(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"y\": {\"type\": "
        "\"lvalue\", \"line\": 2, \"start_pos\": 19, \"column\": 11, "
        "\"end_pos\": 20, \"end_column\": 12}, \"main\": {\"type\": "
        "\"function_definition\", \"line\": 1, \"start_pos\": 0, \"column\": "
        "1, \"end_pos\": 4, \"end_column\": 5}}");

    obj["if_4"] = credence::util::AST_Node::load(
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
    hoisted.make_root_branch();
    hoisted.symbols_.table_.emplace("x", type::NULL_LITERAL);
    hoisted.symbols_.table_.emplace("y", type::NULL_LITERAL);
    auto test_instructions =
        hoisted.build_from_block_statement(obj["if_4"], true);
    for (auto const& inst : test_instructions) {
        EMIT(os_test, inst);
    }
    std::string expected = R"ita(x = (5:int:4);
_L3:
_t6 = CMP x;
IF _t6 GOTO _L5;
_L4:
_L2:
LEAVE;
_L5:
y = (10:int:4);
GOTO _L4;
)ita";
    CHECK(os_test.str() == expected);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: label and goto")
{
    using namespace credence;
    util::AST_Node obj;
    obj["symbols"] = credence::util::AST_Node::load(
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

    obj["test"] = credence::util::AST_Node::load(
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

    std::string expected = R"ita(_L_ADD:
_p1 = (2:int:4);
_p2 = (5:int:4);
PUSH _p2;
PUSH _p1;
CALL add;
POP 16;
_t2 = RET;
x = _t2;
y = (10:int:4);
GOTO ADD;
)ita";
    TEST_BLOCK_STATEMENT_NODE_WITH(
        obj["symbols"], obj["test"], expected, false, false);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_rvalue_statement")
{
    using namespace credence;
    util::AST_Node obj;
    obj["test"] = credence::util::AST_Node::load(
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
    obj["nested_binary"] = credence::util::AST_Node::load(
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
    obj["nested_or"] = credence::util::AST_Node::load(
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
    obj["complex_or"] = credence::util::AST_Node::load(
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
    obj["or_with_call"] = credence::util::AST_Node::load(
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

    std::deque<std::string> symbols = { "x",   "putchar", "getchar", "double",
                                        "exp", "puts",    "y" };

    std::string expected_1 = R"ita(_p1 = (2:int:4);
_p2 = (5:int:4);
PUSH _p2;
PUSH _p1;
CALL exp;
POP 16;
_t1 = RET;
_t2 = _t1;
_t3 = (2:int:4) ^ _t2;
_t4 = ~ (4:int:4);
_t5 = _t3 / _t4;
_t6 = (5:int:4) + (5:int:4);
_t7 = _t5 * _t6;
)ita";
    std::string expected_2 = R"ita(y = (3:int:4);
_t1 = y == (3:int:4);
_t2 = y > (2:int:4);
_t3 = _t1 && _t2;
x = _t3;
)ita";

    std::string expected_3 = R"ita(y = (3:int:4);
_t1 = (2:int:4) || (3:int:4);
_t2 = (1:int:4) || _t1;
x = _t2;
)ita";

    std::string expected_4 = R"ita(y = (3:int:4);
_t1 = (3:int:4) + (3:int:4);
_t2 = (2:int:4) || _t1;
_t3 = (2:int:4) + _t2;
_t4 = (1:int:4) || _t3;
_t5 = (1:int:4) + _t4;
x = _t5;
)ita";
    std::string expected_5 = R"ita(y = (3:int:4);
_p1 = (5:int:4);
PUSH _p1;
CALL putchar;
POP 8;
_t1 = RET;
_p2 = (1:int:4);
PUSH _p2;
CALL getchar;
POP 8;
_t2 = RET;
_t3 = _t2;
_t4 = (3:int:4) + _t3;
_t5 = (3:int:4) || _t4;
_t6 = (1:int:4) || _t5;
x = (1:int:4) + _t6;
)ita";
    TEST_RVALUE_STATEMENT_NODE_WITH(obj, symbols, obj["test"], expected_1);
    TEST_RVALUE_STATEMENT_NODE_WITH(
        obj, symbols, obj["nested_binary"], expected_2);
    TEST_RVALUE_STATEMENT_NODE_WITH(obj, symbols, obj["nested_or"], expected_3);
    TEST_RVALUE_STATEMENT_NODE_WITH(
        obj, symbols, obj["complex_or"], expected_4);
    TEST_RVALUE_STATEMENT_NODE_WITH(
        obj, symbols, obj["or_with_call"], expected_5);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_auto_statement")
{
    using namespace credence;
    util::AST_Node obj;
    obj["test"] = credence::util::AST_Node::load(
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
    auto internal_symbols = credence::util::AST_Node::load(
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
    obj["test"] = credence::util::AST_Node::load(
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

    std::string expected = R"ita(_t1 = (5:int:4) + (5:int:4);
_t2 = (6:int:4) + (6:int:4);
_t3 = _t1 * _t2;
x = _t3;
)ita";
    TEST_RVALUE_STATEMENT_NODE_WITH(obj, { "x" }, obj["test"], expected);
}
