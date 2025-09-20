#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase

#include <credence/ir/ita.h> // for ITA
#include <credence/symbol.h> // for Symbol_Table
#include <credence/types.h>  // for Value_Type, LITERAL_TYPE, Byte, NULL_L...
#include <credence/util.h>   // for AST_Node
#include <deque>
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

    inline void TEST_BLOCK_STATEMENT_NODE_WITH(
        Node const& symbols,
        Node const& node,
        std::string_view test,
        bool tail = true,
        bool ret = true)
    {
        std::ostringstream os_test;
        auto hoisted =
            tail ? ITA_with_tail_branch(symbols) : ITA_hoisted(symbols);
        auto test_instructions = hoisted.build_from_block_statement(node, ret);
        for (auto const& inst : test_instructions) {
            EMIT(os_test, inst);
        }
        REQUIRE(os_test.str() == test);
    }

    inline void TEST_RETURN_STATEMENT_NODE_WITH(
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

    credence::ir::ITA ita_;
};

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_function_definition")
{
    using namespace credence;
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

    auto os_test = std::ostringstream();
    auto ita = ITA_hoisted(internal_symbols);
    auto test_instructions = ita.build_from_function_definition(obj["test"]);
    for (auto const& inst : test_instructions) {
        EMIT(os_test, inst);
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

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_block_statement")
{
    using namespace credence;
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

    obj["test"] = json::JSON::load(
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

    auto expected = R"ita(_t3 = (5:int:4) + (5:int:4);
_t4 = (3:int:4) + (3:int:4);
_t5 = _t3 * _t4;
x = _t5;
y = (100:int:4);
_t6 = x > (0:int:4);
IF _t6 GOTO _L7;
GOTO _L13;
_L2:
_L14:
x = (2:int:4);
LEAVE;
_L7:
_t10 = x >= (0:int:4);
_L8:
IF _t10 GOTO _L9;
GOTO _L2;
_L9:
_t11 = -- x;
_t12 = -- y;
x = _t12;
GOTO _L8;
GOTO _L2;
_L13:
_t15 = ++ x;
y = (5:int:4);
_t16 = x + y;
_t17 = x + x;
_t18 = _t16 * _t17;
x = _t18;
GOTO _L14;
)ita";
    auto expected_2 = R"ita(x = (1:int:4);
y = (10:int:4);
_t5 = x >= (0:int:4);
_L3:
IF _t5 GOTO _L4;
_t11 = x <= (100:int:4);
_L9:
IF _t11 GOTO _L10;
x = (2:int:4);
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
_t3 = x > (5:int:4);
IF _t3 GOTO _L4;
_L2:
_t10 = ++ x;
y = (5:int:4);
_t11 = x + y;
_t12 = x + x;
_t13 = _t11 * _t12;
x = _t13;
LEAVE;
_L4:
_t7 = x >= (0:int:4);
_L5:
IF _t7 GOTO _L6;
GOTO _L2;
_L6:
_t8 = -- x;
_t9 = -- y;
x = _t9;
GOTO _L5;
GOTO _L2;
)ita";
    TEST_BLOCK_STATEMENT_NODE_WITH(obj["symbols"], obj["while"], expected);
    TEST_BLOCK_STATEMENT_NODE_WITH(obj["symbols"], obj["while_2"], expected_2);
    TEST_BLOCK_STATEMENT_NODE_WITH(obj["symbols"], obj["while_4"], expected_3);
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

    obj["if"] = json::JSON::load(
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

    obj["if_2"] = json::JSON::load(
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

    auto expected = R"ita(_t3 = (5:int:4) + (5:int:4);
_t4 = (3:int:4) + (3:int:4);
_t5 = _t3 * _t4;
x = _t5;
_t6 = x <= (5:int:4);
IF _t6 GOTO _L7;
GOTO _L9;
_L8:
_L10:
_t11 = (5:int:4) || (2:int:4);
x = _t11;
LEAVE;
_L7:
x = (1:int:4);
GOTO _L8;
_L9:
x = (8:int:4);
GOTO _L10;
)ita";

    auto expected_2 = R"ita(_t3 = (5:int:4) + (5:int:4);
_t4 = (3:int:4) + (3:int:4);
_t5 = _t3 * _t4;
x = _t5;
_t6 = x <= (5:int:4);
IF _t6 GOTO _L7;
_L8:
_t9 = x > (5:int:4);
IF _t9 GOTO _L10;
_L11:
_t12 = (5:int:4) || (2:int:4);
x = _t12;
LEAVE;
_L7:
x = (1:int:4);
GOTO _L8;
_L10:
x = (10:int:4);
GOTO _L11;
)ita";

    TEST_BLOCK_STATEMENT_NODE_WITH(obj["symbols"], obj["if"], expected);
    TEST_BLOCK_STATEMENT_NODE_WITH(obj["symbols"], obj["if_2"], expected_2);
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
_t3 = CMP x;
IF _t3 GOTO _L4;
_L5:
LEAVE;
_L4:
y = (10:int:4);
GOTO _L5;
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
_t2 = RET;
x = _t2;
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
