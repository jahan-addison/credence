#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase

#include <credence/ir/ita.h> // for ITA
#include <credence/symbol.h> // for Symbol_Table
#include <credence/util.h>   // for AST_Node
#include <credence/values.h> // for Value_Type, TYPE_LITERAL, Byte, NULL_L...
#include <deque>             // for deque
#include <easyjson.h>        // for JSON
#include <map>               // for map
#include <sstream>           // for basic_ostringstream, ostringstream
#include <string>            // for basic_string, allocator, char_traits
#include <string_view>       // for basic_string_view
#include <tuple>             // for tuple
#include <utility>           // for pair, make_pair
#include <variant>           // for monostate

#define EMIT(os, inst) credence::ir::detail::emit_to(os, inst)
#define LOAD_JSON_FROM_STRING(str) credence::util::AST_Node::load(str)

struct ITA_Fixture
{
    using NULL_symbols = std::deque<std::string>;
    using Node = credence::util::AST_Node;
    inline auto make_node() { return credence::util::AST::object(); }
    const Node definitions_symbols = LOAD_JSON_FROM_STRING(
        "{\"number\": {\"type\": \"function_definition\", \"line\": 9, "
        "\"start_pos\": 50, \"column\": 1, \"end_pos\": 56, "
        "\"end_column\": "
        "7}, \"main\": {\"type\": \"function_definition\", \"line\": 1, "
        "\"start_pos\": 0, \"column\": 1, \"end_pos\": 4, \"end_column\": "
        "5}, "
        "\"x\": {\"type\": \"lvalue\", \"line\": 5, \"start_pos\": 28, "
        "\"column\": 5, \"end_pos\": 29, \"end_column\": 6}, \"ret\": "
        "{\"type\": \"function_definition\", \"line\": 5, \"start_pos\": "
        "24, "
        "\"column\": 1, \"end_pos\": 27, \"end_column\": 4}}");
    const Node global_symbols = LOAD_JSON_FROM_STRING(
        "{\n  \"arg\" : {\n    \"column\" : 6,\n    \"end_column\" : 9,\n "
        "   "
        "\"end_pos\" : 8,\n    \"line\" : 1,\n    \"start_pos\" : 5,\n    "
        "\"type\" : \"lvalue\"\n  },\n  \"exp\" : {\n    \"column\" : "
        "1,\n    "
        "\"end_column\" : 4,\n    \"end_pos\" : 52,\n    \"line\" : 6,\n  "
        "  "
        "\"start_pos\" : 49,\n    \"type\" : \"function_definition\"\n  "
        "},\n  "
        "\"main\" : {\n    \"column\" : 1,\n    \"end_column\" : 5,\n    "
        "\"end_pos\" : 4,\n    \"line\" : 1,\n    \"start_pos\" : 0,\n    "
        "\"type\" : \"function_definition\"\n  },\n  \"x\" : {\n    "
        "\"column\" "
        ": 8,\n    \"end_column\" : 9,\n    \"end_pos\" : 20,\n    "
        "\"line\" : "
        "2,\n    \"start_pos\" : 19,\n    \"type\" : \"lvalue\"\n  },\n  "
        "\"y\" "
        ": {\n    \"column\" : 7,\n    \"end_column\" : 8,\n    "
        "\"end_pos\" : "
        "56,\n    \"line\" : 6,\n    \"start_pos\" : 55,\n    \"type\" : "
        "\"lvalue\"\n  }\n}");

    static inline credence::ir::ITA ITA_hoisted(Node const& node)
    {
        return credence::ir::ITA{ node };
    }
    static inline credence::ir::ITA ITA_with_tail_branch(Node const& node)
    {
        auto ita = credence::ir::ITA{ node };
        auto tail_branch = credence::ir::make_temporary(&ita.temporary);
        return ita;
    }

    inline void TEST_DEFINITIONS_WITH(Node const& node, std::string_view test)
    {
        auto os_test = std::ostringstream();
        auto ita = ITA_hoisted(definitions_symbols);
        auto test_instructions =
            credence::ir::ITA::make_ita_instructions(definitions_symbols, node);
        for (auto const& inst : test_instructions) {
            EMIT(os_test, inst);
        }
        REQUIRE(os_test.str() == test);
    }

    inline void TEST_DEFINITIONS_WITH(Node const& node,
        std::string_view test,
        Node const& symbols)
    {
        auto os_test = std::ostringstream();
        auto test_instructions =
            credence::ir::ITA::make_ita_instructions(symbols, node);
        for (auto const& inst : test_instructions) {
            EMIT(os_test, inst);
        }
        REQUIRE(os_test.str() == test);
    }

    inline void TEST_FUNCTION_DEFINITION_WITH(Node const& node,
        std::string_view test,
        NULL_symbols const& nulls = {})
    {
        auto os_test = std::ostringstream();
        auto ita = ITA_hoisted(global_symbols);
        for (auto const& s : nulls)
            ita.symbols_.table_.emplace(
                s, credence::value::Expression::NULL_LITERAL);
        auto test_instructions = ita.build_from_function_definition(node);
        for (auto const& inst : test_instructions) {
            EMIT(os_test, inst);
        }
        REQUIRE(os_test.str() == test);
    }

    inline void TEST_BLOCK_STATEMENT_NODE_WITH(Node const& symbols,
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

    inline void TEST_BLOCK_STATEMENT_NODE_WITH(Node const& symbols,
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
            hoisted.symbols_.table_.emplace(
                s, credence::value::Expression::NULL_LITERAL);
        auto test_instructions = hoisted.build_from_block_statement(node, ret);
        for (auto const& inst : test_instructions) {
            EMIT(os_test, inst);
        }
        REQUIRE(os_test.str() == test);
    }

    inline void TEST_RETURN_STATEMENT_NODE_WITH(Node const& symbols,
        NULL_symbols const& nulls,
        Node const& node,
        std::string_view test)
    {
        std::ostringstream os_test;
        auto hoisted = ITA_hoisted(symbols);
        for (auto const& s : nulls)
            hoisted.symbols_.table_.emplace(
                s, credence::value::Expression::NULL_LITERAL);
        auto test_instructions = hoisted.build_from_return_statement(node);
        for (auto const& inst : test_instructions) {
            EMIT(os_test, inst);
        }
        REQUIRE(os_test.str() == test);
    }

    inline void TEST_RVALUE_STATEMENT_NODE_WITH(Node const& symbols,
        NULL_symbols const& nulls,
        Node const& node,
        std::string_view test)
    {
        std::ostringstream os_test;
        auto hoisted = ITA_hoisted(symbols);
        for (auto const& s : nulls)
            hoisted.symbols_.table_.emplace(
                s, credence::value::Expression::NULL_LITERAL);
        auto test_instructions = hoisted.build_from_rvalue_statement(node);
        for (auto const& inst : test_instructions) {
            EMIT(os_test, inst);
        }
        REQUIRE(os_test.str() == test);
    }

    credence::ir::ITA ita_;
};

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: integration test")
{
    auto obj = make_node();
    obj["switch_4"] = LOAD_JSON_FROM_STRING(
        "{\n  \"left\" : [{\n      \"left\" : [null],\n      \"node\" : "
        "\"function_definition\",\n      \"right\" : {\n        \"left\" "
        ": "
        "[{\n            \"left\" : [{\n                \"node\" : "
        "\"lvalue\",\n                \"root\" : \"m\"\n              }, "
        "{\n   "
        "             \"node\" : \"lvalue\",\n                \"root\" : "
        "\"i\"\n              }, {\n                \"node\" : "
        "\"lvalue\",\n   "
        "             \"root\" : \"j\"\n              }, {\n              "
        "  "
        "\"node\" : \"lvalue\",\n                \"root\" : \"c\"\n       "
        "     "
        "  }, {\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"sign\"\n              }, {\n                "
        "\"node\" : "
        "\"lvalue\",\n                \"root\" : \"C\"\n              }, "
        "{\n   "
        "             \"node\" : \"lvalue\",\n                \"root\" : "
        "\"s\"\n              }],\n            \"node\" : "
        "\"statement\",\n     "
        "       \"root\" : \"auto\"\n          }, {\n            \"left\" "
        ": "
        "[{\n                \"node\" : \"lvalue\",\n                "
        "\"root\" "
        ": \"loop\"\n              }],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"auto\"\n          }, "
        "{\n     "
        "       \"left\" : [[{\n                  \"left\" : {\n          "
        "     "
        "     \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"i\"\n   "
        "               },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 0\n                  },\n                  \"root\" : "
        "[\"=\"]\n                }], [{\n                  \"left\" : "
        "{\n     "
        "               \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"j\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 1\n                  },\n                  \"root\" : "
        "[\"=\"]\n                }], [{\n                  \"left\" : "
        "{\n     "
        "               \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"m\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 0\n                  },\n                  \"root\" : "
        "[\"=\"]\n                }], [{\n                  \"left\" : "
        "{\n     "
        "               \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"sign\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 0\n                  },\n                  \"root\" : "
        "[\"=\"]\n                }], [{\n                  \"left\" : "
        "{\n     "
        "               \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"loop\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 1\n                  },\n                  \"root\" : "
        "[\"=\"]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }, "
        "{\n   "
        "         \"left\" : {\n              \"left\" : {\n              "
        "  "
        "\"node\" : \"lvalue\",\n                \"root\" : \"loop\"\n    "
        "     "
        "     },\n              \"node\" : \"relation_expression\",\n     "
        "     "
        "    \"right\" : {\n                \"node\" : "
        "\"number_literal\",\n   "
        "             \"root\" : 1\n              },\n              "
        "\"root\" : "
        "[\"==\"]\n            },\n            \"node\" : "
        "\"statement\",\n     "
        "       \"right\" : [{\n                \"left\" : [{\n           "
        "     "
        "    \"left\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n  "
        "     "
        "                 \"left\" : {\n                          "
        "\"node\" : "
        "\"lvalue\",\n                          \"root\" : \"C\"\n        "
        "     "
        "           },\n                        \"node\" : "
        "\"assignment_expression\",\n                        \"right\" : "
        "{\n   "
        "                       \"left\" : {\n                            "
        "\"node\" : \"lvalue\",\n                            \"root\" : "
        "\"char\"\n                          },\n                         "
        " "
        "\"node\" : \"function_expression\",\n                          "
        "\"right\" : [{\n                              \"node\" : "
        "\"lvalue\",\n                              \"root\" : \"s\"\n    "
        "     "
        "                   }, {\n                              \"left\" "
        ": {\n "
        "                               \"node\" : \"lvalue\",\n          "
        "     "
        "                 \"root\" : \"j\"\n                              "
        "},\n "
        "                             \"node\" : "
        "\"pre_inc_dec_expression\",\n "
        "                             \"root\" : [\"++\"]\n               "
        "     "
        "        }],\n                          \"root\" : \"char\"\n     "
        "     "
        "              },\n                        \"root\" : [\"=\"]\n   "
        "     "
        "              }\n                    },\n                    "
        "\"node\" "
        ": \"statement\",\n                    \"right\" : [{\n           "
        "     "
        "        \"left\" : {\n                          \"node\" : "
        "\"constant_literal\",\n                          \"root\" : "
        "\"-\"\n   "
        "                     },\n                        \"node\" : "
        "\"statement\",\n                        \"right\" : [{\n         "
        "     "
        "              \"left\" : {\n                              "
        "\"node\" : "
        "\"lvalue\",\n                              \"root\" : \"sign\"\n "
        "     "
        "                      },\n                            \"node\" : "
        "\"statement\",\n                            \"right\" : [{\n     "
        "     "
        "                      \"left\" : [{\n                            "
        "     "
        "   \"left\" : [[{\n                                          "
        "\"left\" "
        ": {\n                                            \"node\" : "
        "\"lvalue\",\n                                            "
        "\"root\" : "
        "\"loop\"\n                                          },\n         "
        "     "
        "                            \"node\" : "
        "\"assignment_expression\",\n   "
        "                                       \"right\" : {\n           "
        "     "
        "                            \"node\" : \"number_literal\",\n     "
        "     "
        "                                  \"root\" : 0\n                 "
        "     "
        "                    },\n                                         "
        " "
        "\"root\" : [\"=\"]\n                                        }], "
        "[{\n  "
        "                                        \"left\" : {\n           "
        "     "
        "                            \"node\" : \"lvalue\",\n             "
        "     "
        "                          \"root\" : \"error\"\n                 "
        "     "
        "                    },\n                                         "
        " "
        "\"node\" : \"function_expression\",\n                            "
        "     "
        "         \"right\" : [null],\n                                   "
        "     "
        "  \"root\" : \"error\"\n                                        "
        "}]],\n                                    \"node\" : "
        "\"statement\",\n "
        "                                   \"root\" : \"rvalue\"\n       "
        "     "
        "                      }],\n                                "
        "\"node\" : "
        "\"statement\",\n                                \"root\" : "
        "\"block\"\n                              }, null],\n             "
        "     "
        "          \"root\" : \"if\"\n                          }, {\n    "
        "     "
        "                   \"left\" : [[{\n                              "
        "    "
        "\"left\" : {\n                                    \"node\" : "
        "\"lvalue\",\n                                    \"root\" : "
        "\"s\"\n   "
        "                               },\n                              "
        "    "
        "\"node\" : \"assignment_expression\",\n                          "
        "     "
        "   \"right\" : {\n                                    \"node\" : "
        "\"number_literal\",\n                                    "
        "\"root\" : "
        "1\n                                  },\n                        "
        "     "
        "     \"root\" : [\"=\"]\n                                }]],\n  "
        "     "
        "                     \"node\" : \"statement\",\n                 "
        "     "
        "      \"root\" : \"rvalue\"\n                          }],\n     "
        "     "
        "              \"root\" : \"case\"\n                      }, {\n  "
        "     "
        "                 \"left\" : {\n                          "
        "\"node\" : "
        "\"constant_literal\",\n                          \"root\" : \"' "
        "'\"\n "
        "                       },\n                        \"node\" : "
        "\"statement\",\n                        \"right\" : [{\n         "
        "     "
        "              \"node\" : \"statement\",\n                        "
        "    "
        "\"root\" : \"break\"\n                          }],\n            "
        "     "
        "       \"root\" : \"case\"\n                      }, {\n         "
        "     "
        "          \"left\" : {\n                          \"node\" : "
        "\"constant_literal\",\n                          \"root\" : "
        "\"0\"\n   "
        "                     },\n                        \"node\" : "
        "\"statement\",\n                        \"right\" : [],\n        "
        "     "
        "           \"root\" : \"case\"\n                      }, {\n     "
        "     "
        "              \"left\" : {\n                          \"node\" : "
        "\"constant_literal\",\n                          \"root\" : "
        "\",\"\n   "
        "                     },\n                        \"node\" : "
        "\"statement\",\n                        \"right\" : [{\n         "
        "     "
        "              \"left\" : {\n                              "
        "\"left\" : "
        "{\n                                \"node\" : \"lvalue\",\n      "
        "     "
        "                     \"root\" : \"c\"\n                          "
        "    "
        "},\n                              \"node\" : "
        "\"relation_expression\",\n                              "
        "\"right\" : "
        "{\n                                \"node\" : "
        "\"constant_literal\",\n "
        "                               \"root\" : \"0\"\n                "
        "     "
        "         },\n                              \"root\" : [\"==\"]\n "
        "     "
        "                      },\n                            \"node\" : "
        "\"statement\",\n                            \"right\" : [{\n     "
        "     "
        "                      \"left\" : [{\n                            "
        "     "
        "   \"node\" : \"lvalue\",\n                                    "
        "\"root\" : \"i\"\n                                  }],\n        "
        "     "
        "                   \"node\" : \"statement\",\n                   "
        "     "
        "        \"root\" : \"return\"\n                              }, "
        "null],\n                            \"root\" : \"if\"\n          "
        "     "
        "           }],\n                        \"root\" : \"case\"\n    "
        "     "
        "             }],\n                    \"root\" : \"switch\"\n    "
        "     "
        "         }, {\n                    \"left\" : {\n                "
        "     "
        " \"left\" : {\n                        \"node\" : "
        "\"constant_literal\",\n                        \"root\" : "
        "\"0\"\n     "
        "                 },\n                      \"node\" : "
        "\"relation_expression\",\n                      \"right\" : {\n  "
        "     "
        "                 \"left\" : {\n                          "
        "\"node\" : "
        "\"lvalue\",\n                          \"root\" : \"c\"\n        "
        "     "
        "           },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : "
        "{\n     "
        "                     \"left\" : {\n                            "
        "\"node\" : \"lvalue\",\n                            \"root\" : "
        "\"c\"\n                          },\n                          "
        "\"node\" : \"relation_expression\",\n                          "
        "\"right\" : {\n                            \"node\" : "
        "\"constant_literal\",\n                            \"root\" : "
        "\"9\"\n "
        "                         },\n                          \"root\" "
        ": "
        "[\"<=\"]\n                        },\n                        "
        "\"root\" : [\"&&\"]\n                      },\n                  "
        "    "
        "\"root\" : [\"<=\"]\n                    },\n                    "
        "\"node\" : \"statement\",\n                    \"right\" : [{\n  "
        "     "
        "                 \"left\" : [{\n                            "
        "\"left\" "
        ": [[{\n                                  \"left\" : {\n          "
        "     "
        "                     \"node\" : \"lvalue\",\n                    "
        "     "
        "           \"root\" : \"m\"\n                                  "
        "},\n   "
        "                               \"node\" : "
        "\"assignment_expression\",\n                                  "
        "\"right\" : {\n                                    \"left\" : "
        "{\n     "
        "                                 \"node\" : "
        "\"number_literal\",\n     "
        "                                 \"root\" : 10\n                 "
        "     "
        "              },\n                                    \"node\" : "
        "\"relation_expression\",\n                                    "
        "\"right\" : {\n                                      \"left\" : "
        "{\n   "
        "                                     \"node\" : \"lvalue\",\n    "
        "     "
        "                               \"root\" : \"m\"\n                "
        "     "
        "                 },\n                                      "
        "\"node\" : "
        "\"relation_expression\",\n                                      "
        "\"right\" : {\n                                        \"left\" "
        ": {\n "
        "                                         \"node\" : "
        "\"lvalue\",\n     "
        "                                     \"root\" : \"c\"\n          "
        "     "
        "                         },\n                                    "
        "    "
        "\"node\" : \"relation_expression\",\n                            "
        "     "
        "       \"right\" : {\n                                          "
        "\"node\" : \"constant_literal\",\n                               "
        "     "
        "      \"root\" : \"0\"\n                                        "
        "},\n  "
        "                                      \"root\" : [\"-\"]\n       "
        "     "
        "                          },\n                                   "
        "   "
        "\"root\" : [\"+\"]\n                                    },\n     "
        "     "
        "                          \"root\" : [\"*\"]\n                   "
        "     "
        "          },\n                                  \"root\" : "
        "[\"=\"]\n  "
        "                              }]],\n                            "
        "\"node\" : \"statement\",\n                            \"root\" "
        ": "
        "\"rvalue\"\n                          }],\n                      "
        "  "
        "\"node\" : \"statement\",\n                        \"root\" : "
        "\"block\"\n                      }, null],\n                    "
        "\"root\" : \"if\"\n                  }],\n                "
        "\"node\" : "
        "\"statement\",\n                \"root\" : \"block\"\n           "
        "   "
        "}],\n            \"root\" : \"while\"\n          }],\n        "
        "\"node\" : \"statement\",\n        \"root\" : \"block\"\n      "
        "},\n   "
        "   \"root\" : \"main\"\n    }, {\n      \"left\" : [{\n          "
        "\"node\" : \"lvalue\",\n          \"root\" : \"a\"\n        }, "
        "{\n    "
        "      \"node\" : \"lvalue\",\n          \"root\" : \"b\"\n       "
        " "
        "}],\n      \"node\" : \"function_definition\",\n      \"right\" "
        ": {\n "
        "       \"left\" : [],\n        \"node\" : \"statement\",\n       "
        " "
        "\"root\" : \"block\"\n      },\n      \"root\" : \"char\"\n    "
        "}, {\n "
        "     \"left\" : [null],\n      \"node\" : "
        "\"function_definition\",\n  "
        "    \"right\" : {\n        \"left\" : [{\n            \"left\" : "
        "[[{\n                  \"left\" : {\n                    "
        "\"node\" : "
        "\"lvalue\",\n                    \"root\" : \"printf\"\n         "
        "     "
        "    },\n                  \"node\" : \"function_expression\",\n  "
        "     "
        "           \"right\" : [{\n                      \"node\" : "
        "\"string_literal\",\n                      \"root\" : \"\\\"bad "
        "syntax*n\\\"\"\n                    }],\n                  "
        "\"root\" : "
        "\"printf\"\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }, "
        "{\n   "
        "         \"left\" : [{\n                \"left\" : {\n           "
        "     "
        "  \"node\" : \"number_literal\",\n                  \"root\" : "
        "1\n    "
        "            },\n                \"node\" : "
        "\"unary_expression\",\n    "
        "            \"root\" : [\"-\"]\n              }],\n            "
        "\"node\" : \"statement\",\n            \"root\" : \"return\"\n   "
        "     "
        "  }],\n        \"node\" : \"statement\",\n        \"root\" : "
        "\"block\"\n      },\n      \"root\" : \"error\"\n    }, {\n      "
        "\"left\" : [{\n          \"node\" : \"lvalue\",\n          "
        "\"root\" : "
        "\"s\"\n        }],\n      \"node\" : \"function_definition\",\n  "
        "    "
        "\"right\" : {\n        \"left\" : [{\n            \"left\" : "
        "[{\n     "
        "           \"node\" : \"lvalue\",\n                \"root\" : "
        "\"s\"\n "
        "             }],\n            \"node\" : \"statement\",\n        "
        "    "
        "\"root\" : \"return\"\n          }],\n        \"node\" : "
        "\"statement\",\n        \"root\" : \"block\"\n      },\n      "
        "\"root\" : \"printf\"\n    }],\n  \"node\" : \"program\",\n  "
        "\"root\" "
        ": \"definitions\"\n}\n");
    obj["ir"] = LOAD_JSON_FROM_STRING(
        " {\n      \"left\" : [null],\n      \"node\" : "
        "\"function_definition\",\n      \"right\" : {\n        \"left\" "
        ": "
        "[{\n            \"left\" : [{\n                \"node\" : "
        "\"lvalue\",\n                \"root\" : \"x\"\n              }, "
        "{\n   "
        "             \"node\" : \"lvalue\",\n                \"root\" : "
        "\"y\"\n              }, {\n                \"node\" : "
        "\"lvalue\",\n   "
        "             \"root\" : \"z\"\n              }],\n            "
        "\"node\" : \"statement\",\n            \"root\" : \"auto\"\n     "
        " "
        "    "
        "}, {\n            \"left\" : [[{\n                  \"left\" : "
        "{\n    "
        "                \"node\" : \"lvalue\",\n                    "
        "\"root\" "
        ": \"x\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        " "
        "    "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 5\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }], [{\n                  "
        "\"left\" "
        ": "
        "{\n                    \"node\" : \"lvalue\",\n                  "
        " "
        " "
        "\"root\" : \"y\"\n                  },\n                  "
        "\"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        " "
        "    "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 1\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }], [{\n                  "
        "\"left\" "
        ": "
        "{\n                    \"node\" : \"lvalue\",\n                  "
        " "
        " "
        "\"root\" : \"z\"\n                  },\n                  "
        "\"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        " "
        "    "
        "           \"left\" : {\n                      \"left\" : {\n    "
        " "
        "    "
        "               \"node\" : \"lvalue\",\n                        "
        "\"root\" : \"add\"\n                      },\n                   "
        " "
        "  "
        "\"node\" : \"function_expression\",\n                      "
        "\"right\" "
        ": [{\n                          \"node\" : \"lvalue\",\n         "
        " "
        "    "
        "            \"root\" : \"x\"\n                        }, {\n     "
        " "
        "    "
        "                \"left\" : {\n                            "
        "\"node\" : "
        "\"lvalue\",\n                            \"root\" : \"sub\"\n    "
        " "
        "    "
        "                 },\n                          \"node\" : "
        "\"function_expression\",\n                          \"right\" : "
        "[{\n  "
        "                            \"node\" : \"lvalue\",\n             "
        " "
        "    "
        "            \"root\" : \"x\"\n                            }, {\n "
        " "
        "    "
        "                        \"node\" : \"lvalue\",\n                 "
        " "
        "    "
        "        \"root\" : \"y\"\n                            }],\n      "
        " "
        "    "
        "               \"root\" : \"sub\"\n                        }],\n "
        " "
        "    "
        "                \"root\" : \"add\"\n                    },\n     "
        " "
        "    "
        "          \"node\" : \"relation_expression\",\n                  "
        " "
        " "
        "\"right\" : {\n                      \"node\" : "
        "\"number_literal\",\n "
        "                     \"root\" : 2\n                    },\n      "
        " "
        "    "
        "         \"root\" : [\"-\"]\n                  },\n              "
        " "
        "   "
        "\"root\" : [\"=\", null]\n                }]],\n            "
        "\"node\" "
        ": \"statement\",\n            \"root\" : \"rvalue\"\n          "
        "}, "
        "{\n "
        "           \"left\" : {\n              \"left\" : {\n            "
        " "
        "   "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n       "
        " "
        "    "
        "  },\n              \"node\" : \"relation_expression\",\n        "
        " "
        "    "
        " \"right\" : {\n                \"node\" : \"lvalue\",\n         "
        " "
        "    "
        "  \"root\" : \"y\"\n              },\n              \"root\" : "
        "[\">\"]\n            },\n            \"node\" : \"statement\",\n "
        " "
        "    "
        "      \"right\" : [{\n                \"left\" : [{\n            "
        " "
        "    "
        "   \"left\" : {\n                      \"left\" : {\n            "
        " "
        "    "
        "       \"node\" : \"lvalue\",\n                        \"root\" "
        ": "
        "\"z\"\n                      },\n                      \"node\" "
        ": "
        "\"relation_expression\",\n                      \"right\" : {\n  "
        " "
        "    "
        "                 \"node\" : \"lvalue\",\n                        "
        "\"root\" : \"x\"\n                      },\n                     "
        " "
        "\"root\" : [\">\"]\n                    },\n                    "
        "\"node\" : \"statement\",\n                    \"right\" : [{\n  "
        " "
        "    "
        "                 \"left\" : [{\n                            "
        "\"left\" "
        ": [[{\n                                  \"node\" : "
        "\"post_inc_dec_expression\",\n                                  "
        "\"right\" : {\n                                    \"node\" : "
        "\"lvalue\",\n                                    \"root\" : "
        "\"z\"\n   "
        "                               },\n                              "
        " "
        "   "
        "\"root\" : [\"--\"]\n                                }]],\n      "
        " "
        "    "
        "                 \"node\" : \"statement\",\n                     "
        " "
        "    "
        "  \"root\" : \"rvalue\"\n                          }],\n         "
        " "
        "    "
        "          \"node\" : \"statement\",\n                        "
        "\"root\" "
        ": \"block\"\n                      }],\n                    "
        "\"root\" "
        ": \"while\"\n                  }],\n                \"node\" : "
        "\"statement\",\n                \"root\" : \"block\"\n           "
        " "
        "  "
        "}, null],\n            \"root\" : \"if\"\n          }, {\n       "
        " "
        "    "
        "\"left\" : [[{\n                  \"left\" : {\n                 "
        " "
        "  "
        "\"node\" : \"lvalue\",\n                    \"root\" : \"x\"\n   "
        " "
        "    "
        "          },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        " "
        "    "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 0\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          "
        "}],\n    "
        "    \"node\" : \"statement\",\n        \"root\" : \"block\"\n    "
        " "
        " "
        "},\n      \"root\" : \"main\"\n    }");

    obj["switch_4-symbols"] = LOAD_JSON_FROM_STRING(
        "{\"m\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 17, "
        "\"column\": 9, \"end_pos\": 18, \"end_column\": 10}, \"i\": "
        "{\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 19, "
        "\"column\": "
        "11, \"end_pos\": 20, \"end_column\": 12}, \"j\": {\"type\": "
        "\"lvalue\", \"line\": 2, \"start_pos\": 21, \"column\": 13, "
        "\"end_pos\": 22, \"end_column\": 14}, \"c\": {\"type\": "
        "\"lvalue\", "
        "\"line\": 2, \"start_pos\": 23, \"column\": 15, \"end_pos\": 24, "
        "\"end_column\": 16}, \"sign\": {\"type\": \"lvalue\", \"line\": "
        "2, "
        "\"start_pos\": 25, \"column\": 17, \"end_pos\": 29, "
        "\"end_column\": "
        "21}, \"C\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": "
        "30, "
        "\"column\": 22, \"end_pos\": 31, \"end_column\": 23}, \"s\": "
        "{\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 32, "
        "\"column\": "
        "24, \"end_pos\": 33, \"end_column\": 25}, \"loop\": {\"type\": "
        "\"lvalue\", \"line\": 3, \"start_pos\": 43, \"column\": 9, "
        "\"end_pos\": 47, \"end_column\": 13}, \"char\": {\"type\": "
        "\"function_definition\", \"line\": 31, \"start_pos\": 774, "
        "\"column\": 1, \"end_pos\": 778, \"end_column\": 5}, \"error\": "
        "{\"type\": \"function_definition\", \"line\": 34, \"start_pos\": "
        "789, "
        "\"column\": 1, \"end_pos\": 794, \"end_column\": 6}, \"main\": "
        "{\"type\": \"function_definition\", \"line\": 1, \"start_pos\": "
        "0, "
        "\"column\": 1, \"end_pos\": 4, \"end_column\": 5}, \"a\": "
        "{\"type\": "
        "\"lvalue\", \"line\": 31, \"start_pos\": 779, \"column\": 6, "
        "\"end_pos\": 780, \"end_column\": 7}, \"b\": {\"type\": "
        "\"lvalue\", "
        "\"line\": 31, \"start_pos\": 781, \"column\": 8, \"end_pos\": "
        "782, "
        "\"end_column\": 9}, \"printf\": {\"type\": "
        "\"function_definition\", "
        "\"line\": 39, \"start_pos\": 844, \"column\": 1, \"end_pos\": "
        "850, "
        "\"end_column\": 7}}");

    std::string expected = R"ita(__main():
 BeginFunc ;
LOCL m;
LOCL i;
LOCL j;
LOCL c;
LOCL sign;
LOCL C;
LOCL s;
LOCL loop;
i = (0:int:4);
j = (1:int:4);
m = (0:int:4);
sign = (0:int:4);
loop = (1:int:4);
_L2:
_L4:
_t5 = loop == (1:int:4);
IF _t5 GOTO _L3;
_L1:
LEAVE;
_L3:
_p1 = s;
j = ++j;
_p2 = j;
PUSH _p2;
PUSH _p1;
CALL char;
POP 16;
_t6 = RET;
C = _t6;
JMP_E C ('45':char:1) _L8;
JMP_E C ('39':char:1) _L15;
JMP_E C ('48':char:1) _L17;
JMP_E C ('44':char:1) _L19;
_L18:
_L16:
_L14:
_L7:
_L24:
_t27 = c <= ('57':char:1);
_t28 = c && _t27;
_t29 = ('48':char:1) <= _t28;
IF _t29 GOTO _L26;
_L25:
GOTO _L4;
_L8:
_L9:
_t12 = CMP sign;
IF _t12 GOTO _L11;
_L10:
s = (1:int:4);
GOTO _L7;
_L11:
loop = (0:int:4);
CALL error;
_t13 = RET;
GOTO _L10;
GOTO _L4;
_L15:
GOTO _L14;
GOTO _L4;
_L17:
GOTO _L4;
_L19:
_L20:
_t23 = c == ('48':char:1);
IF _t23 GOTO _L22;
_L21:
GOTO _L18;
_L22:
RET i ;
GOTO _L21;
GOTO _L4;
_L26:
_t30 = c - ('48':char:1);
_t31 = m + _t30;
_t32 = (10:int:4) * _t31;
m = _t32;
GOTO _L25;
 EndFunc ;
__char(a,b):
 BeginFunc ;
_L1:
LEAVE;
 EndFunc ;
__error():
 BeginFunc ;
_p1 = ("bad syntax*n":string:12);
PUSH _p1;
CALL printf;
POP 8;
_t2 = RET;
_t3 = - (1:int:4);
RET _t3;
_L1:
LEAVE;
 EndFunc ;
__printf(s):
 BeginFunc ;
RET s ;
_L1:
LEAVE;
 EndFunc ;
)ita";

    auto expected_2 = R"ita(__main():
 BeginFunc ;
LOCL x;
LOCL y;
LOCL z;
x = (5:int:4);
y = (1:int:4);
_p1 = x;
_p3 = x;
_p4 = y;
PUSH _p4;
PUSH _p3;
CALL sub;
POP 16;
_t2 = RET;
_p2 = _t2;
PUSH _p2;
PUSH _p1;
CALL add;
POP 16;
_t3 = RET;
_t4 = _t3;
z = (2:int:4) - _t4;
_L5:
_t8 = x > y;
IF _t8 GOTO _L7;
_L6:
x = (0:int:4);
_L1:
LEAVE;
_L7:
_L9:
_L11:
_t12 = z > x;
IF _t12 GOTO _L10;
GOTO _L6;
_L10:
z = --z;
GOTO _L9;
 EndFunc ;
)ita";
    TEST_DEFINITIONS_WITH(obj["switch_4"], expected, obj["switch_4-symbols"]);
    TEST_FUNCTION_DEFINITION_WITH(obj["ir"], expected_2, { "add", "sub" });
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_definitions")
{
    auto obj = make_node();
    obj["call_3"] = LOAD_JSON_FROM_STRING(
        "{\n  \"left\" : [{\n      \"left\" : [null],\n      \"node\" : "
        "\"function_definition\",\n      \"right\" : {\n        \"left\" "
        ": "
        "[{\n            \"left\" : [[{\n                  \"left\" : {\n "
        "     "
        "              \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"number\"\n                  },\n                  \"node\" : "
        "\"function_expression\",\n                  \"right\" : "
        "[null],\n     "
        "             \"root\" : \"number\"\n                }]],\n       "
        "     "
        "\"node\" : \"statement\",\n            \"root\" : \"rvalue\"\n   "
        "     "
        "  }],\n        \"node\" : \"statement\",\n        \"root\" : "
        "\"block\"\n      },\n      \"root\" : \"main\"\n    }, {\n      "
        "\"left\" : [{\n          \"node\" : \"lvalue\",\n          "
        "\"root\" : "
        "\"x\"\n        }],\n      \"node\" : \"function_definition\",\n  "
        "    "
        "\"right\" : {\n        \"left\" : [{\n            \"left\" : "
        "[{\n     "
        "           \"node\" : \"lvalue\",\n                \"root\" : "
        "\"x\"\n "
        "             }],\n            \"node\" : \"statement\",\n        "
        "    "
        "\"root\" : \"return\"\n          }],\n        \"node\" : "
        "\"statement\",\n        \"root\" : \"block\"\n      },\n      "
        "\"root\" : \"ret\"\n    }, {\n      \"left\" : [null],\n      "
        "\"node\" : \"function_definition\",\n      \"right\" : {\n       "
        " "
        "\"left\" : [{\n            \"left\" : [[{\n                  "
        "\"left\" "
        ": {\n                    \"node\" : \"lvalue\",\n                "
        "    "
        "\"root\" : \"ret\"\n                  },\n                  "
        "\"node\" "
        ": \"function_expression\",\n                  \"right\" : [{\n   "
        "     "
        "              \"node\" : \"number_literal\",\n                   "
        "   "
        "\"root\" : 5\n                    }],\n                  "
        "\"root\" : "
        "\"ret\"\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          "
        "}],\n    "
        "    \"node\" : \"statement\",\n        \"root\" : \"block\"\n    "
        "  "
        "},\n      \"root\" : \"number\"\n    }],\n  \"node\" : "
        "\"program\",\n "
        " \"root\" : \"definitions\"\n}");

    std::string expected = R"ita(__main():
 BeginFunc ;
CALL number;
_t2 = RET;
_L1:
LEAVE;
 EndFunc ;
__ret(x):
 BeginFunc ;
RET x ;
_L1:
LEAVE;
 EndFunc ;
__number():
 BeginFunc ;
_p1 = (5:int:4);
PUSH _p1;
CALL ret;
POP 8;
_t2 = RET;
_L1:
LEAVE;
 EndFunc ;
)ita";
    TEST_DEFINITIONS_WITH(obj["call_3"], expected);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_function_definition")
{
    auto obj = make_node();
    obj["function_2"] = LOAD_JSON_FROM_STRING(
        "{\n      \"left\" : [null],\n      \"node\" : "
        "\"function_definition\",\n      \"right\" : {\n        \"left\" "
        ": "
        "[{\n            \"left\" : [{\n                \"node\" : "
        "\"lvalue\",\n                \"root\" : \"x\"\n              "
        "}],\n    "
        "        \"node\" : \"statement\",\n            \"root\" : "
        "\"auto\"\n  "
        "        }, {\n            \"left\" : [[{\n                  "
        "\"left\" "
        ": {\n                    \"node\" : \"lvalue\",\n                "
        "    "
        "\"root\" : \"x\"\n                  },\n                  "
        "\"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"left\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 5\n        "
        "     "
        "       },\n                    \"node\" : "
        "\"relation_expression\",\n  "
        "                  \"right\" : {\n                      \"left\" "
        ": {\n "
        "                       \"node\" : \"number_literal\",\n          "
        "     "
        "         \"root\" : 5\n                      },\n                "
        "     "
        " \"node\" : \"relation_expression\",\n                      "
        "\"right\" "
        ": {\n                        \"left\" : {\n                      "
        "    "
        "\"node\" : \"lvalue\",\n                          \"root\" : "
        "\"exp\"\n                        },\n                        "
        "\"node\" "
        ": \"function_expression\",\n                        \"right\" : "
        "[{\n  "
        "                          \"node\" : \"number_literal\",\n       "
        "     "
        "                \"root\" : 2\n                          }, {\n   "
        "     "
        "                    \"node\" : \"number_literal\",\n             "
        "     "
        "          \"root\" : 5\n                          }],\n          "
        "     "
        "         \"root\" : \"exp\"\n                      },\n          "
        "     "
        "       \"root\" : [\"+\"]\n                    },\n              "
        "     "
        " \"root\" : [\"*\"]\n                  },\n                  "
        "\"root\" "
        ": [\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          "
        "}],\n    "
        "    \"node\" : \"statement\",\n        \"root\" : \"block\"\n    "
        "  "
        "},\n      \"root\" : \"main\"\n    }");

    std::string expected = R"ita(__main():
 BeginFunc ;
LOCL x;
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
    TEST_FUNCTION_DEFINITION_WITH(obj["function_2"], expected);
}

TEST_CASE_FIXTURE(ITA_Fixture,
    "ir/ita.cc: function recursion and tail function calls")
{
    auto obj = make_node();

    obj["recursion"] = LOAD_JSON_FROM_STRING(
        "  {\n      \"left\" : [{\n          \"node\" : \"lvalue\",\n     "
        "     "
        "\"root\" : \"x\"\n        }, {\n          \"node\" : "
        "\"lvalue\",\n    "
        "      \"root\" : \"y\"\n        }],\n      \"node\" : "
        "\"function_definition\",\n      \"right\" : {\n        \"left\" "
        ": "
        "[{\n            \"left\" : {\n              \"left\" : {\n       "
        "     "
        "    \"node\" : \"lvalue\",\n                \"root\" : \"x\"\n   "
        "     "
        "      },\n              \"node\" : \"relation_expression\",\n    "
        "     "
        "     \"right\" : {\n                \"left\" : {\n               "
        "   "
        "\"node\" : \"number_literal\",\n                  \"root\" : 1\n "
        "     "
        "          },\n                \"node\" : "
        "\"relation_expression\",\n   "
        "             \"right\" : {\n                  \"left\" : {\n     "
        "     "
        "          \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"y\"\n                  },\n                  \"node\" : "
        "\"relation_expression\",\n                  \"right\" : {\n      "
        "     "
        "         \"node\" : \"number_literal\",\n                    "
        "\"root\" "
        ": 1\n                  },\n                  \"root\" : "
        "[\"==\"]\n    "
        "            },\n                \"root\" : [\"||\"]\n            "
        "  "
        "},\n              \"root\" : [\"==\"]\n            },\n          "
        "  "
        "\"node\" : \"statement\",\n            \"right\" : [{\n          "
        "     "
        " \"left\" : [{\n                    \"left\" : {\n               "
        "     "
        "  \"node\" : \"lvalue\",\n                      \"root\" : "
        "\"x\"\n    "
        "                },\n                    \"node\" : "
        "\"relation_expression\",\n                    \"right\" : {\n    "
        "     "
        "             \"node\" : \"lvalue\",\n                      "
        "\"root\" : "
        "\"y\"\n                    },\n                    \"root\" : "
        "[\"*\"]\n                  }],\n                \"node\" : "
        "\"statement\",\n                \"root\" : \"return\"\n          "
        "    "
        "}, null],\n            \"root\" : \"if\"\n          }, {\n       "
        "     "
        "\"left\" : [{\n                \"left\" : {\n                  "
        "\"node\" : \"lvalue\",\n                  \"root\" : \"exp\"\n   "
        "     "
        "        },\n                \"node\" : "
        "\"function_expression\",\n     "
        "           \"right\" : [{\n                    \"left\" : {\n    "
        "     "
        "             \"node\" : \"lvalue\",\n                      "
        "\"root\" : "
        "\"x\"\n                    },\n                    \"node\" : "
        "\"relation_expression\",\n                    \"right\" : {\n    "
        "     "
        "             \"node\" : \"number_literal\",\n                    "
        "  "
        "\"root\" : 1\n                    },\n                    "
        "\"root\" : "
        "[\"-\"]\n                  }, {\n                    \"left\" : "
        "{\n   "
        "                   \"node\" : \"lvalue\",\n                      "
        "\"root\" : \"y\"\n                    },\n                    "
        "\"node\" : \"relation_expression\",\n                    "
        "\"right\" : "
        "{\n                      \"node\" : \"number_literal\",\n        "
        "     "
        "         \"root\" : 1\n                    },\n                  "
        "  "
        "\"root\" : [\"-\"]\n                  }],\n                "
        "\"root\" : "
        "\"exp\"\n              }],\n            \"node\" : "
        "\"statement\",\n   "
        "         \"root\" : \"return\"\n          }],\n        \"node\" "
        ": "
        "\"statement\",\n        \"root\" : \"block\"\n      },\n      "
        "\"root\" : \"exp\"\n    }");

    std::string expected = R"ita(__exp(x,y):
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
    TEST_FUNCTION_DEFINITION_WITH(obj["recursion"], expected);
}

TEST_CASE_FIXTURE(ITA_Fixture,
    "ir/ita.cc: nested function call and return rvalues")
{
    auto obj = make_node();

    obj["test"] = LOAD_JSON_FROM_STRING(
        "\n{\n  \"left\" : [{\n      \"left\" : [null],\n      \"node\" : "
        "\"function_definition\",\n      \"right\" : {\n        \"left\" "
        ": "
        "[{\n            \"left\" : [{\n                \"node\" : "
        "\"lvalue\",\n                \"root\" : \"x\"\n              "
        "}],\n    "
        "        \"node\" : \"statement\",\n            \"root\" : "
        "\"auto\"\n  "
        "        }, {\n            \"left\" : [[{\n                  "
        "\"left\" "
        ": {\n                    \"node\" : \"lvalue\",\n                "
        "    "
        "\"root\" : \"x\"\n                  },\n                  "
        "\"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"left\" : {\n                      \"node\" : "
        "\"lvalue\",\n                      \"root\" : \"exp\"\n          "
        "     "
        "     },\n                    \"node\" : "
        "\"function_expression\",\n    "
        "                \"right\" : [{\n                        \"left\" "
        ": "
        "{\n                          \"node\" : \"lvalue\",\n            "
        "     "
        "         \"root\" : \"exp\"\n                        },\n        "
        "     "
        "           \"node\" : \"function_expression\",\n                 "
        "     "
        "  \"right\" : [{\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 2\n  "
        "     "
        "                   }, {\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 5\n  "
        "     "
        "                   }],\n                        \"root\" : "
        "\"exp\"\n  "
        "                    }, {\n                        \"node\" : "
        "\"number_literal\",\n                        \"root\" : 2\n      "
        "     "
        "           }],\n                    \"root\" : \"exp\"\n         "
        "     "
        "    },\n                  \"root\" : [\"=\", null]\n             "
        "   "
        "}]],\n            \"node\" : \"statement\",\n            "
        "\"root\" : "
        "\"rvalue\"\n          }],\n        \"node\" : \"statement\",\n   "
        "     "
        "\"root\" : \"block\"\n      },\n      \"root\" : \"main\"\n    "
        "}, {\n "
        "     \"left\" : [{\n          \"node\" : \"lvalue\",\n          "
        "\"root\" : \"x\"\n        }, {\n          \"node\" : "
        "\"lvalue\",\n    "
        "      \"root\" : \"y\"\n        }],\n      \"node\" : "
        "\"function_definition\",\n      \"right\" : {\n        \"left\" "
        ": "
        "[{\n            \"left\" : [{\n                \"node\" : "
        "\"lvalue\",\n                \"root\" : \"x\"\n              }, "
        "{\n   "
        "             \"node\" : \"lvalue\",\n                \"root\" : "
        "\"y\"\n              }],\n            \"node\" : "
        "\"statement\",\n     "
        "       \"root\" : \"auto\"\n          }, {\n            \"left\" "
        ": "
        "[[{\n                  \"left\" : {\n                    "
        "\"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n              "
        "    "
        "},\n                  \"node\" : \"relation_expression\",\n      "
        "     "
        "       \"right\" : {\n                    \"node\" : "
        "\"lvalue\",\n    "
        "                \"root\" : \"y\"\n                  },\n         "
        "     "
        "    \"root\" : [\"*\"]\n                }]],\n            "
        "\"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          "
        "}],\n    "
        "    \"node\" : \"statement\",\n        \"root\" : \"block\"\n    "
        "  "
        "},\n      \"root\" : \"exp\"\n    }, {\n      \"left\" : [{\n    "
        "     "
        " \"node\" : \"lvalue\",\n          \"root\" : \"x\"\n        "
        "}],\n    "
        "  \"node\" : \"function_definition\",\n      \"right\" : {\n     "
        "   "
        "\"left\" : [{\n            \"left\" : {\n              \"left\" "
        ": {\n "
        "               \"node\" : \"lvalue\",\n                \"root\" "
        ": "
        "\"x\"\n              },\n              \"node\" : "
        "\"relation_expression\",\n              \"right\" : {\n          "
        "     "
        " \"node\" : \"number_literal\",\n                \"root\" : 0\n  "
        "     "
        "       },\n              \"root\" : [\"==\"]\n            },\n   "
        "     "
        "    \"node\" : \"statement\",\n            \"right\" : [{\n      "
        "     "
        "     \"left\" : [{\n                    \"left\" : [{\n          "
        "     "
        "         \"node\" : \"lvalue\",\n                        "
        "\"root\" : "
        "\"x\"\n                      }],\n                    \"node\" : "
        "\"statement\",\n                    \"root\" : \"return\"\n      "
        "     "
        "       }],\n                \"node\" : \"statement\",\n          "
        "     "
        " \"root\" : \"block\"\n              }, null],\n            "
        "\"root\" "
        ": \"if\"\n          }, {\n            \"left\" : [{\n            "
        "    "
        "\"left\" : {\n                  \"node\" : \"lvalue\",\n         "
        "     "
        "    \"root\" : \"sub\"\n                },\n                "
        "\"node\" "
        ": \"function_expression\",\n                \"right\" : [{\n     "
        "     "
        "          \"left\" : {\n                      \"node\" : "
        "\"lvalue\",\n                      \"root\" : \"x\"\n            "
        "     "
        "   },\n                    \"node\" : \"relation_expression\",\n "
        "     "
        "              \"right\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 1\n        "
        "     "
        "       },\n                    \"root\" : [\"-\"]\n              "
        "    "
        "}],\n                \"root\" : \"sub\"\n              }],\n     "
        "     "
        "  \"node\" : \"statement\",\n            \"root\" : \"return\"\n "
        "     "
        "    }],\n        \"node\" : \"statement\",\n        \"root\" : "
        "\"block\"\n      },\n      \"root\" : \"sub\"\n    }],\n  "
        "\"node\" : "
        "\"program\",\n  \"root\" : \"definitions\"\n}\n");

    auto ita = ITA_hoisted(global_symbols);
    auto definitions = obj["test"]["left"].to_deque();
    auto expected = R"ita(LOCL x;
_p2 = (2:int:4);
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
        global_symbols, definitions[0]["right"], expected, false, false);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_block_statement")
{
    auto obj = make_node();
    obj["test"] = LOAD_JSON_FROM_STRING(
        "{\n        \"left\" : [{\n            \"left\" : [{\n            "
        "    "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n       "
        "     "
        "  }],\n            \"node\" : \"statement\",\n            "
        "\"root\" : "
        "\"auto\"\n          }, {\n            \"left\" : [[{\n           "
        "     "
        "  \"left\" : {\n                    \"node\" : \"lvalue\",\n     "
        "     "
        "          \"root\" : \"x\"\n                  },\n               "
        "   "
        "\"node\" : \"assignment_expression\",\n                  "
        "\"right\" : "
        "{\n                    \"left\" : {\n                      "
        "\"node\" : "
        "\"number_literal\",\n                      \"root\" : 5\n        "
        "     "
        "       },\n                    \"node\" : "
        "\"relation_expression\",\n  "
        "                  \"right\" : {\n                      \"node\" "
        ": "
        "\"number_literal\",\n                      \"root\" : 2\n        "
        "     "
        "       },\n                    \"root\" : [\"||\"]\n             "
        "     "
        "},\n                  \"root\" : [\"=\", null]\n                "
        "}]],\n            \"node\" : \"statement\",\n            "
        "\"root\" : "
        "\"rvalue\"\n          }],\n        \"node\" : \"statement\",\n   "
        "     "
        "\"root\" : \"block\"\n      }");
    auto expected = R"ita(LOCL x;
_t2 = (5:int:4) || (2:int:4);
x = _t2;
)ita";
    TEST_BLOCK_STATEMENT_NODE_WITH(
        global_symbols, obj["test"], expected, false, false);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_extrn_statement")
{
    auto obj = make_node();

    obj["test"] = LOAD_JSON_FROM_STRING(
        "{\n            \"left\" : [{\n                \"node\" : "
        "\"lvalue\",\n                \"root\" : \"a\"\n              }, "
        "{\n   "
        "             \"node\" : \"lvalue\",\n                \"root\" : "
        "\"b\"\n              }, {\n                \"node\" : "
        "\"lvalue\",\n   "
        "             \"root\" : \"c\"\n              }],\n            "
        "\"node\" : \"statement\",\n            \"root\" : \"extrn\"\n    "
        "     "
        " }");

    auto vectors = obj["test"].to_deque();
    auto ita = ITA_hoisted(obj);
    credence::ir::Instructions instructions{};

    CHECK_THROWS(ita.build_from_extrn_statement(obj["test"], instructions));

    ita.globals_.addr_.emplace("a",
        credence::value::Array{ credence::value::Expression::NULL_LITERAL });
    ita.globals_.addr_.emplace("b",
        credence::value::Array{ credence::value::Expression::NULL_LITERAL });
    ita.globals_.addr_.emplace("c",
        credence::value::Array{ credence::value::Expression::NULL_LITERAL });

    CHECK_NOTHROW(ita.build_from_extrn_statement(obj["test"], instructions));

    CHECK_EQ(ita.symbols_.is_defined("a"), true);
    CHECK_EQ(ita.symbols_.is_defined("b"), true);
    CHECK_EQ(ita.symbols_.is_defined("c"), true);
    REQUIRE(instructions.size() == 3);
    REQUIRE(std::get<0>(instructions[0]) == credence::ir::Instruction::GLOBL);
    REQUIRE(std::get<0>(instructions[1]) == credence::ir::Instruction::GLOBL);
    REQUIRE(std::get<0>(instructions[2]) == credence::ir::Instruction::GLOBL);
    REQUIRE(std::get<1>(instructions[0]) == "a");
    REQUIRE(std::get<1>(instructions[1]) == "b");
    REQUIRE(std::get<1>(instructions[2]) == "c");
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_vector_definition")
{
    using namespace credence;
    credence::util::AST_Node obj;
    obj["symbols"] = LOAD_JSON_FROM_STRING(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"main\": "
        "{\"type\": \"function_definition\", \"line\": 1, \"start_pos\": "
        "0, "
        "\"column\": 1, \"end_pos\": 4, \"end_column\": 5}, \"a\": "
        "{\"type\": "
        "\"vector_definition\", \"line\": 11, \"start_pos\": 93, "
        "\"column\": "
        "1, \"end_pos\": 94, \"end_column\": 2}, \"b\": {\"type\": "
        "\"vector_definition\", \"line\": 12, \"start_pos\": 103, "
        "\"column\": "
        "1, \"end_pos\": 104, \"end_column\": 2}, \"add\": {\"type\": "
        "\"function_definition\", \"line\": 6, \"start_pos\": 39, "
        "\"column\": "
        "1, \"end_pos\": 42, \"end_column\": 4}, \"c\": {\"type\": "
        "\"vector_definition\", \"line\": 13, \"start_pos\": 113, "
        "\"column\": "
        "1, \"end_pos\": 114, \"end_column\": 2}, \"mess\": {\"type\": "
        "\"vector_definition\", \"line\": 15, \"start_pos\": 124, "
        "\"column\": "
        "1, \"end_pos\": 128, \"end_column\": 5}}");

    obj["test"] = LOAD_JSON_FROM_STRING(
        "{\n      \"left\" : {\n        \"node\" : \"number_literal\",\n  "
        "     "
        " \"root\" : 6\n      },\n      \"node\" : "
        "\"vector_definition\",\n    "
        "  \"right\" : [{\n          \"node\" : \"string_literal\",\n     "
        "     "
        "\"root\" : \"\\\"too bad\\\"\"\n        }, {\n          \"node\" "
        ": "
        "\"string_literal\",\n          \"root\" : \"\\\"tough "
        "luck\\\"\"\n    "
        "    }, {\n          \"node\" : \"string_literal\",\n          "
        "\"root\" : \"\\\"sorry, Charlie\\\"\"\n        }, {\n          "
        "\"node\" : \"string_literal\",\n          \"root\" : "
        "\"\\\"that's the "
        "breaks\\\"\"\n        }, {\n          \"node\" : "
        "\"string_literal\",\n          \"root\" : \"\\\"what a "
        "shame\\\"\"\n  "
        "      }, {\n          \"node\" : \"string_literal\",\n          "
        "\"root\" : \"\\\"some days you can't win\\\"\"\n        }],\n    "
        "  "
        "\"root\" : \"mess\"\n    }");

    auto vector = obj["test"];
    auto ita = ITA_hoisted(obj["symbols"]);
    ita.build_from_vector_definition(vector);
    CHECK(ita.globals_.is_defined(vector["root"].to_string()) == true);
    auto vector_of_strings =
        ita.globals_.get_pointer_by_name(vector["root"].to_string());
    CHECK(std::get<std::string>(vector_of_strings.at(0).first) == "too bad");
    CHECK(std::get<std::string>(vector_of_strings.at(1).first) == "tough luck");
    CHECK(vector_of_strings.size() == 6);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_return_statement")
{
    auto obj = make_node();
    auto internal_symbols = LOAD_JSON_FROM_STRING(
        "{\n  \"arg\" : {\n    \"column\" : 6,\n    \"end_column\" : 9,\n "
        "   "
        "\"end_pos\" : 8,\n    \"line\" : 1,\n    \"start_pos\" : 5,\n    "
        "\"type\" : \"lvalue\"\n  },\n  \"exp\" : {\n    \"column\" : "
        "1,\n    "
        "\"end_column\" : 4,\n    \"end_pos\" : 52,\n    \"line\" : 6,\n  "
        "  "
        "\"start_pos\" : 49,\n    \"type\" : \"function_definition\"\n  "
        "},\n  "
        "\"main\" : {\n    \"column\" : 1,\n    \"end_column\" : 5,\n    "
        "\"end_pos\" : 4,\n    \"line\" : 1,\n    \"start_pos\" : 0,\n    "
        "\"type\" : \"function_definition\"\n  },\n  \"x\" : {\n    "
        "\"column\" "
        ": 8,\n    \"end_column\" : 9,\n    \"end_pos\" : 20,\n    "
        "\"line\" : "
        "2,\n    \"start_pos\" : 19,\n    \"type\" : \"lvalue\"\n  },\n  "
        "\"y\" "
        ": {\n    \"column\" : 7,\n    \"end_column\" : 8,\n    "
        "\"end_pos\" : "
        "56,\n    \"line\" : 6,\n    \"start_pos\" : 55,\n    \"type\" : "
        "\"lvalue\"\n  }\n}");
    obj["test"] = LOAD_JSON_FROM_STRING(
        "{\n            \"left\" : [{\n                \"left\" : {\n     "
        "     "
        "        \"node\" : \"lvalue\",\n                  \"root\" : "
        "\"x\"\n  "
        "              },\n                \"node\" : "
        "\"relation_expression\",\n                \"right\" : {\n        "
        "     "
        "     \"left\" : {\n                    \"node\" : \"lvalue\",\n  "
        "     "
        "             \"root\" : \"y\"\n                  },\n            "
        "     "
        " \"node\" : \"relation_expression\",\n                  "
        "\"right\" : "
        "{\n                    \"node\" : \"lvalue\",\n                  "
        "  "
        "\"root\" : \"y\"\n                  },\n                  "
        "\"root\" : "
        "[\"*\"]\n                },\n                \"root\" : "
        "[\"*\"]\n     "
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
    auto obj = make_node();
    obj["test"] = LOAD_JSON_FROM_STRING(
        "{\n        \"left\" : [{\n            \"left\" : [{\n            "
        "    "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n       "
        "     "
        "  }],\n            \"node\" : \"statement\",\n            "
        "\"root\" : "
        "\"auto\"\n          }, {\n            \"left\" : [[{\n           "
        "     "
        "  \"left\" : {\n                    \"node\" : \"lvalue\",\n     "
        "     "
        "          \"root\" : \"x\"\n                  },\n               "
        "   "
        "\"node\" : \"assignment_expression\",\n                  "
        "\"right\" : "
        "{\n                    \"left\" : {\n                      "
        "\"node\" : "
        "\"number_literal\",\n                      \"root\" : 5\n        "
        "     "
        "       },\n                    \"node\" : "
        "\"relation_expression\",\n  "
        "                  \"right\" : {\n                      \"node\" "
        ": "
        "\"number_literal\",\n                      \"root\" : 2\n        "
        "     "
        "       },\n                    \"root\" : [\"||\"]\n             "
        "     "
        "},\n                  \"root\" : [\"=\", null]\n                "
        "}]],\n            \"node\" : \"statement\",\n            "
        "\"root\" : "
        "\"rvalue\"\n          }],\n        \"node\" : \"statement\",\n   "
        "     "
        "\"root\" : \"block\"\n      }");

    auto expected = R"ita(LOCL x;
_t2 = (5:int:4) || (2:int:4);
x = _t2;
)ita";
    TEST_BLOCK_STATEMENT_NODE_WITH(obj, obj["test"], expected, false, false);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: switch and case statements")
{
    auto obj = make_node();
    obj["symbols"] = LOAD_JSON_FROM_STRING(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"main\": "
        "{\"type\": \"function_definition\", \"line\": 1, \"start_pos\": "
        "0, "
        "\"column\": 1, \"end_pos\": 4, \"end_column\": 5}}");

    obj["switch"] = LOAD_JSON_FROM_STRING(
        "{\n        \"left\" : [{\n            \"left\" : [{\n            "
        "    "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n       "
        "     "
        "  }, {\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"y\"\n              }],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"auto\"\n          }, "
        "{\n     "
        "       \"left\" : [[{\n                  \"left\" : {\n          "
        "     "
        "     \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"x\"\n   "
        "               },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 5\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }, "
        "{\n   "
        "         \"left\" : {\n              \"node\" : \"lvalue\",\n    "
        "     "
        "     \"root\" : \"x\"\n            },\n            \"node\" : "
        "\"statement\",\n            \"right\" : [{\n                "
        "\"left\" "
        ": {\n                  \"node\" : \"number_literal\",\n          "
        "     "
        "   \"root\" : 0\n                },\n                \"node\" : "
        "\"statement\",\n                \"right\" : [{\n                 "
        "   "
        "\"left\" : [[{\n                          \"left\" : {\n         "
        "     "
        "              \"node\" : \"lvalue\",\n                           "
        " "
        "\"root\" : \"y\"\n                          },\n                 "
        "     "
        "    \"node\" : \"assignment_expression\",\n                      "
        "    "
        "\"right\" : {\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 1\n  "
        "     "
        "                   },\n                          \"root\" : "
        "[\"=\", "
        "null]\n                        }]],\n                    "
        "\"node\" : "
        "\"statement\",\n                    \"root\" : \"rvalue\"\n      "
        "     "
        "       }, {\n                    \"node\" : \"statement\",\n     "
        "     "
        "          \"root\" : \"break\"\n                  }],\n          "
        "     "
        " \"root\" : \"case\"\n              }, {\n                "
        "\"left\" : "
        "{\n                  \"node\" : \"number_literal\",\n            "
        "     "
        " \"root\" : 1\n                },\n                \"node\" : "
        "\"statement\",\n                \"right\" : [{\n                 "
        "   "
        "\"left\" : [[{\n                          \"left\" : {\n         "
        "     "
        "              \"node\" : \"lvalue\",\n                           "
        " "
        "\"root\" : \"y\"\n                          },\n                 "
        "     "
        "    \"node\" : \"assignment_expression\",\n                      "
        "    "
        "\"right\" : {\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 2\n  "
        "     "
        "                   },\n                          \"root\" : "
        "[\"=\", "
        "null]\n                        }]],\n                    "
        "\"node\" : "
        "\"statement\",\n                    \"root\" : \"rvalue\"\n      "
        "     "
        "       }],\n                \"root\" : \"case\"\n              "
        "}, {\n "
        "               \"left\" : {\n                  \"node\" : "
        "\"number_literal\",\n                  \"root\" : 2\n            "
        "    "
        "},\n                \"node\" : \"statement\",\n                "
        "\"right\" : [{\n                    \"left\" : [[{\n             "
        "     "
        "        \"left\" : {\n                            \"node\" : "
        "\"lvalue\",\n                            \"root\" : \"x\"\n      "
        "     "
        "               },\n                          \"node\" : "
        "\"assignment_expression\",\n                          \"right\" "
        ": {\n "
        "                           \"node\" : \"number_literal\",\n      "
        "     "
        "                 \"root\" : 5\n                          },\n    "
        "     "
        "                 \"root\" : [\"=\", null]\n                      "
        "  "
        "}]],\n                    \"node\" : \"statement\",\n            "
        "     "
        "   \"root\" : \"rvalue\"\n                  }, {\n               "
        "     "
        "\"node\" : \"statement\",\n                    \"root\" : "
        "\"break\"\n "
        "                 }],\n                \"root\" : \"case\"\n      "
        "     "
        "   }],\n            \"root\" : \"switch\"\n          }, {\n      "
        "     "
        " \"left\" : [[{\n                  \"left\" : {\n                "
        "    "
        "\"node\" : \"lvalue\",\n                    \"root\" : \"y\"\n   "
        "     "
        "          },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 10\n                  },\n                  \"root\" "
        ": "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          "
        "}],\n    "
        "    \"node\" : \"statement\",\n        \"root\" : \"block\"\n    "
        "  }");

    obj["switch_2"] = LOAD_JSON_FROM_STRING(
        "{\n        \"left\" : [{\n            \"left\" : [{\n            "
        "    "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n       "
        "     "
        "  }, {\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"y\"\n              }],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"auto\"\n          }, "
        "{\n     "
        "       \"left\" : [[{\n                  \"left\" : {\n          "
        "     "
        "     \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"x\"\n   "
        "               },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 5\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }, "
        "{\n   "
        "         \"left\" : {\n              \"node\" : \"lvalue\",\n    "
        "     "
        "     \"root\" : \"x\"\n            },\n            \"node\" : "
        "\"statement\",\n            \"right\" : [{\n                "
        "\"left\" "
        ": {\n                  \"node\" : \"number_literal\",\n          "
        "     "
        "   \"root\" : 0\n                },\n                \"node\" : "
        "\"statement\",\n                \"right\" : [{\n                 "
        "   "
        "\"left\" : {\n                      \"left\" : {\n               "
        "     "
        "    \"node\" : \"lvalue\",\n                        \"root\" : "
        "\"x\"\n                      },\n                      \"node\" "
        ": "
        "\"relation_expression\",\n                      \"right\" : {\n  "
        "     "
        "                 \"node\" : \"number_literal\",\n                "
        "     "
        "   \"root\" : 5\n                      },\n                      "
        "\"root\" : [\">=\"]\n                    },\n                    "
        "\"node\" : \"statement\",\n                    \"right\" : [{\n  "
        "     "
        "                 \"left\" : [{\n                            "
        "\"left\" "
        ": {\n                              \"left\" : {\n                "
        "     "
        "           \"node\" : \"lvalue\",\n                              "
        "  "
        "\"root\" : \"x\"\n                              },\n             "
        "     "
        "            \"node\" : \"relation_expression\",\n                "
        "     "
        "         \"right\" : {\n                                \"node\" "
        ": "
        "\"number_literal\",\n                                \"root\" : "
        "1\n   "
        "                           },\n                              "
        "\"root\" "
        ": [\">\"]\n                            },\n                      "
        "     "
        " \"node\" : \"statement\",\n                            "
        "\"right\" : "
        "[{\n                                \"left\" : [{\n              "
        "     "
        "                 \"left\" : [[{\n                                "
        "     "
        "     \"node\" : \"post_inc_dec_expression\",\n                   "
        "     "
        "                  \"right\" : {\n                                "
        "     "
        "       \"node\" : \"lvalue\",\n                                  "
        "     "
        "     \"root\" : \"x\"\n                                          "
        "},\n "
        "                                         \"root\" : [\"--\"]\n   "
        "     "
        "                                }]],\n                           "
        "     "
        "    \"node\" : \"statement\",\n                                  "
        "  "
        "\"root\" : \"rvalue\"\n                                  }],\n   "
        "     "
        "                        \"node\" : \"statement\",\n              "
        "     "
        "             \"root\" : \"block\"\n                              "
        "}],\n                            \"root\" : \"while\"\n          "
        "     "
        "           }],\n                        \"node\" : "
        "\"statement\",\n   "
        "                     \"root\" : \"block\"\n                      "
        "}, "
        "null],\n                    \"root\" : \"if\"\n                  "
        "}, "
        "{\n                    \"node\" : \"statement\",\n               "
        "     "
        "\"root\" : \"break\"\n                  }],\n                "
        "\"root\" "
        ": \"case\"\n              }, {\n                \"left\" : {\n   "
        "     "
        "          \"node\" : \"number_literal\",\n                  "
        "\"root\" "
        ": 1\n                },\n                \"node\" : "
        "\"statement\",\n  "
        "              \"right\" : [{\n                    \"left\" : "
        "[[{\n    "
        "                      \"left\" : {\n                            "
        "\"node\" : \"lvalue\",\n                            \"root\" : "
        "\"y\"\n                          },\n                          "
        "\"node\" : \"assignment_expression\",\n                          "
        "\"right\" : {\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 2\n  "
        "     "
        "                   },\n                          \"root\" : "
        "[\"=\", "
        "null]\n                        }]],\n                    "
        "\"node\" : "
        "\"statement\",\n                    \"root\" : \"rvalue\"\n      "
        "     "
        "       }],\n                \"root\" : \"case\"\n              "
        "}, {\n "
        "               \"left\" : {\n                  \"node\" : "
        "\"number_literal\",\n                  \"root\" : 2\n            "
        "    "
        "},\n                \"node\" : \"statement\",\n                "
        "\"right\" : [{\n                    \"left\" : [[{\n             "
        "     "
        "        \"left\" : {\n                            \"node\" : "
        "\"lvalue\",\n                            \"root\" : \"x\"\n      "
        "     "
        "               },\n                          \"node\" : "
        "\"assignment_expression\",\n                          \"right\" "
        ": {\n "
        "                           \"node\" : \"number_literal\",\n      "
        "     "
        "                 \"root\" : 5\n                          },\n    "
        "     "
        "                 \"root\" : [\"=\", null]\n                      "
        "  "
        "}]],\n                    \"node\" : \"statement\",\n            "
        "     "
        "   \"root\" : \"rvalue\"\n                  }, {\n               "
        "     "
        "\"node\" : \"statement\",\n                    \"root\" : "
        "\"break\"\n "
        "                 }],\n                \"root\" : \"case\"\n      "
        "     "
        "   }],\n            \"root\" : \"switch\"\n          }, {\n      "
        "     "
        " \"left\" : [[{\n                  \"left\" : {\n                "
        "    "
        "\"node\" : \"lvalue\",\n                    \"root\" : \"y\"\n   "
        "     "
        "          },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 10\n                  },\n                  \"root\" "
        ": "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          "
        "}],\n    "
        "    \"node\" : \"statement\",\n        \"root\" : \"block\"\n    "
        "  }");

    obj["switch_3"] = LOAD_JSON_FROM_STRING(
        "{\n        \"left\" : [{\n            \"left\" : [{\n            "
        "    "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n       "
        "     "
        "  }, {\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"y\"\n              }],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"auto\"\n          }, "
        "{\n     "
        "       \"left\" : [[{\n                  \"left\" : {\n          "
        "     "
        "     \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"x\"\n   "
        "               },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 10\n                  },\n                  \"root\" "
        ": "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }, "
        "{\n   "
        "         \"left\" : {\n              \"left\" : {\n              "
        "  "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n       "
        "     "
        "  },\n              \"node\" : \"relation_expression\",\n        "
        "     "
        " \"right\" : {\n                \"node\" : \"number_literal\",\n "
        "     "
        "          \"root\" : 5\n              },\n              \"root\" "
        ": "
        "[\">=\"]\n            },\n            \"node\" : "
        "\"statement\",\n     "
        "       \"right\" : [{\n                \"left\" : [{\n           "
        "     "
        "    \"left\" : {\n                      \"node\" : \"lvalue\",\n "
        "     "
        "                \"root\" : \"x\"\n                    },\n       "
        "     "
        "        \"node\" : \"statement\",\n                    \"right\" "
        ": "
        "[{\n                        \"left\" : {\n                       "
        "   "
        "\"node\" : \"number_literal\",\n                          "
        "\"root\" : "
        "5\n                        },\n                        \"node\" "
        ": "
        "\"statement\",\n                        \"right\" : [{\n         "
        "     "
        "              \"left\" : {\n                              "
        "\"left\" : "
        "{\n                                \"node\" : \"lvalue\",\n      "
        "     "
        "                     \"root\" : \"x\"\n                          "
        "    "
        "},\n                              \"node\" : "
        "\"relation_expression\",\n                              "
        "\"right\" : "
        "{\n                                \"node\" : "
        "\"number_literal\",\n   "
        "                             \"root\" : 1\n                      "
        "     "
        "   },\n                              \"root\" : [\">\"]\n        "
        "     "
        "               },\n                            \"node\" : "
        "\"statement\",\n                            \"right\" : [{\n     "
        "     "
        "                      \"left\" : [{\n                            "
        "     "
        "   \"left\" : [[{\n                                          "
        "\"node\" "
        ": \"post_inc_dec_expression\",\n                                 "
        "     "
        "    \"right\" : {\n                                            "
        "\"node\" : \"lvalue\",\n                                         "
        "   "
        "\"root\" : \"x\"\n                                          },\n "
        "     "
        "                                    \"root\" : [\"--\"]\n        "
        "     "
        "                           }]],\n                                "
        "    "
        "\"node\" : \"statement\",\n                                    "
        "\"root\" : \"rvalue\"\n                                  }],\n   "
        "     "
        "                        \"node\" : \"statement\",\n              "
        "     "
        "             \"root\" : \"block\"\n                              "
        "}],\n                            \"root\" : \"while\"\n          "
        "     "
        "           }, {\n                            \"left\" : [[{\n    "
        "     "
        "                         \"left\" : {\n                          "
        "     "
        "     \"node\" : \"lvalue\",\n                                    "
        "\"root\" : \"y\"\n                                  },\n         "
        "     "
        "                    \"node\" : \"assignment_expression\",\n      "
        "     "
        "                       \"right\" : {\n                           "
        "     "
        "    \"node\" : \"number_literal\",\n                             "
        "     "
        "  \"root\" : 8\n                                  },\n           "
        "     "
        "                  \"root\" : [\"=\", null]\n                     "
        "     "
        "      }], [{\n                                  \"left\" : {\n   "
        "     "
        "                            \"node\" : \"lvalue\",\n             "
        "     "
        "                  \"root\" : \"x\"\n                             "
        "     "
        "},\n                                  \"node\" : "
        "\"assignment_expression\",\n                                  "
        "\"right\" : {\n                                    \"node\" : "
        "\"number_literal\",\n                                    "
        "\"root\" : "
        "2\n                                  },\n                        "
        "     "
        "     \"root\" : [\"=\", null]\n                                "
        "}]],\n "
        "                           \"node\" : \"statement\",\n           "
        "     "
        "            \"root\" : \"rvalue\"\n                          }, "
        "{\n   "
        "                         \"node\" : \"statement\",\n             "
        "     "
        "          \"root\" : \"break\"\n                          }],\n  "
        "     "
        "                 \"root\" : \"case\"\n                      }, "
        "{\n    "
        "                    \"left\" : {\n                          "
        "\"node\" "
        ": \"number_literal\",\n                          \"root\" : 6\n  "
        "     "
        "                 },\n                        \"node\" : "
        "\"statement\",\n                        \"right\" : [{\n         "
        "     "
        "              \"left\" : [[{\n                                  "
        "\"left\" : {\n                                    \"node\" : "
        "\"lvalue\",\n                                    \"root\" : "
        "\"y\"\n   "
        "                               },\n                              "
        "    "
        "\"node\" : \"assignment_expression\",\n                          "
        "     "
        "   \"right\" : {\n                                    \"node\" : "
        "\"number_literal\",\n                                    "
        "\"root\" : "
        "2\n                                  },\n                        "
        "     "
        "     \"root\" : [\"=\", null]\n                                "
        "}]],\n "
        "                           \"node\" : \"statement\",\n           "
        "     "
        "            \"root\" : \"rvalue\"\n                          "
        "}],\n    "
        "                    \"root\" : \"case\"\n                      "
        "}, {\n "
        "                       \"left\" : {\n                          "
        "\"node\" : \"number_literal\",\n                          "
        "\"root\" : "
        "7\n                        },\n                        \"node\" "
        ": "
        "\"statement\",\n                        \"right\" : [{\n         "
        "     "
        "              \"left\" : [[{\n                                  "
        "\"left\" : {\n                                    \"node\" : "
        "\"lvalue\",\n                                    \"root\" : "
        "\"x\"\n   "
        "                               },\n                              "
        "    "
        "\"node\" : \"assignment_expression\",\n                          "
        "     "
        "   \"right\" : {\n                                    \"node\" : "
        "\"number_literal\",\n                                    "
        "\"root\" : "
        "5\n                                  },\n                        "
        "     "
        "     \"root\" : [\"=\", null]\n                                "
        "}]],\n "
        "                           \"node\" : \"statement\",\n           "
        "     "
        "            \"root\" : \"rvalue\"\n                          }, "
        "{\n   "
        "                         \"node\" : \"statement\",\n             "
        "     "
        "          \"root\" : \"break\"\n                          }],\n  "
        "     "
        "                 \"root\" : \"case\"\n                      "
        "}],\n     "
        "               \"root\" : \"switch\"\n                  }],\n    "
        "     "
        "       \"node\" : \"statement\",\n                \"root\" : "
        "\"block\"\n              }, null],\n            \"root\" : "
        "\"if\"\n   "
        "       }, {\n            \"left\" : [[{\n                  "
        "\"left\" : "
        "{\n                    \"node\" : \"lvalue\",\n                  "
        "  "
        "\"root\" : \"y\"\n                  },\n                  "
        "\"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 10\n                  },\n                  \"root\" "
        ": "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          "
        "}],\n    "
        "    \"node\" : \"statement\",\n        \"root\" : \"block\"\n    "
        "  }");

    auto expected = R"ita(LOCL x;
LOCL y;
x = (5:int:4);
_t2 = CMP x;
JMP_E _t2 (0:int:4) _L4;
JMP_E _t2 (1:int:4) _L6;
JMP_E _t2 (2:int:4) _L8;
_L7:
_L5:
_L3:
y = (10:int:4);
_L1:
LEAVE;
_L4:
y = (1:int:4);
GOTO _L3;
_L6:
y = (2:int:4);
_L8:
x = (5:int:4);
GOTO _L7;
)ita";

    auto expected_2 = R"ita(LOCL x;
LOCL y;
x = (5:int:4);
_t2 = CMP x;
JMP_E _t2 (0:int:4) _L4;
JMP_E _t2 (1:int:4) _L14;
JMP_E _t2 (2:int:4) _L16;
_L15:
_L13:
_L3:
y = (10:int:4);
_L1:
LEAVE;
_L4:
_L5:
_t8 = x >= (5:int:4);
IF _t8 GOTO _L7;
_L6:
GOTO _L3;
_L7:
_L9:
_L11:
_t12 = x > (1:int:4);
IF _t12 GOTO _L10;
GOTO _L6;
_L10:
x = --x;
GOTO _L9;
_L14:
y = (2:int:4);
_L16:
x = (5:int:4);
GOTO _L15;
)ita";

    auto expected_3 = R"ita(LOCL x;
LOCL y;
x = (10:int:4);
_L2:
_t5 = x >= (5:int:4);
IF _t5 GOTO _L4;
_L3:
y = (10:int:4);
_L1:
LEAVE;
_L4:
_t6 = CMP x;
JMP_E _t6 (5:int:4) _L8;
JMP_E _t6 (6:int:4) _L14;
JMP_E _t6 (7:int:4) _L16;
_L15:
_L13:
_L7:
GOTO _L3;
_L8:
_L9:
_L11:
_t12 = x > (1:int:4);
IF _t12 GOTO _L10;
y = (8:int:4);
x = (2:int:4);
GOTO _L7;
_L10:
x = --x;
GOTO _L9;
GOTO _L3;
_L14:
y = (2:int:4);
GOTO _L3;
_L16:
x = (5:int:4);
GOTO _L15;
)ita";

    TEST_BLOCK_STATEMENT_NODE_WITH(
        obj["symbols"], obj["switch"], expected, { "x", "y" }, false);
    TEST_BLOCK_STATEMENT_NODE_WITH(
        obj["symbols"], obj["switch_2"], expected_2, { "x", "y" }, false);
    TEST_BLOCK_STATEMENT_NODE_WITH(
        obj["symbols"], obj["switch_3"], expected_3, { "x", "y" }, false);
}

TEST_CASE_FIXTURE(ITA_Fixture,
    "ir/ita.cc: while statement branching and nested while and if "
    "branching")
{

    auto obj = make_node();
    obj["symbols"] = LOAD_JSON_FROM_STRING(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"main\": "
        "{\"type\": \"function_definition\", \"line\": 1, \"start_pos\": "
        "0, "
        "\"column\": 1, \"end_pos\": 4, \"end_column\": 5}}");

    obj["while_4"] = LOAD_JSON_FROM_STRING(
        "{\n        \"left\" : [ {\n            \"left\" : [[{\n          "
        "    "
        "    \"left\" : {\n                    \"node\" : \"lvalue\",\n   "
        "     "
        "            \"root\" : \"x\"\n                  },\n             "
        "     "
        "\"node\" : \"assignment_expression\",\n                  "
        "\"right\" : "
        "{\n                    \"node\" : \"number_literal\",\n          "
        "     "
        "     \"root\" : 100\n                  },\n                  "
        "\"root\" "
        ": [\"=\", null]\n                }], [{\n                  "
        "\"left\" : "
        "{\n                    \"node\" : \"lvalue\",\n                  "
        "  "
        "\"root\" : \"y\"\n                  },\n                  "
        "\"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 100\n                  },\n                  \"root\" "
        ": "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }, "
        "{\n   "
        "         \"left\" : {\n              \"left\" : {\n              "
        "  "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n       "
        "     "
        "  },\n              \"node\" : \"relation_expression\",\n        "
        "     "
        " \"right\" : {\n                \"node\" : \"number_literal\",\n "
        "     "
        "          \"root\" : 100\n              },\n              "
        "\"root\" : "
        "[\"==\"]\n            },\n            \"node\" : "
        "\"statement\",\n     "
        "       \"right\" : [{\n                \"left\" : [{\n           "
        "     "
        "    \"left\" : {\n                      \"left\" : {\n           "
        "     "
        "        \"node\" : \"lvalue\",\n                        \"root\" "
        ": "
        "\"x\"\n                      },\n                      \"node\" "
        ": "
        "\"relation_expression\",\n                      \"right\" : {\n  "
        "     "
        "                 \"node\" : \"number_literal\",\n                "
        "     "
        "   \"root\" : 5\n                      },\n                      "
        "\"root\" : [\">\"]\n                    },\n                    "
        "\"node\" : \"statement\",\n                    \"right\" : [{\n  "
        "     "
        "                 \"left\" : [{\n                            "
        "\"left\" "
        ": {\n                              \"left\" : {\n                "
        "     "
        "           \"node\" : \"lvalue\",\n                              "
        "  "
        "\"root\" : \"x\"\n                              },\n             "
        "     "
        "            \"node\" : \"relation_expression\",\n                "
        "     "
        "         \"right\" : {\n                                \"node\" "
        ": "
        "\"number_literal\",\n                                \"root\" : "
        "0\n   "
        "                           },\n                              "
        "\"root\" "
        ": [\">=\"]\n                            },\n                     "
        "     "
        "  \"node\" : \"statement\",\n                            "
        "\"right\" : "
        "[{\n                                \"left\" : [{\n              "
        "     "
        "                 \"left\" : [[{\n                                "
        "     "
        "     \"node\" : \"post_inc_dec_expression\",\n                   "
        "     "
        "                  \"right\" : {\n                                "
        "     "
        "       \"node\" : \"lvalue\",\n                                  "
        "     "
        "     \"root\" : \"x\"\n                                          "
        "},\n "
        "                                         \"root\" : [\"--\"]\n   "
        "     "
        "                                }], [{\n                         "
        "     "
        "            \"left\" : {\n                                       "
        "     "
        "\"node\" : \"lvalue\",\n                                         "
        "   "
        "\"root\" : \"x\"\n                                          },\n "
        "     "
        "                                    \"node\" : "
        "\"assignment_expression\",\n                                     "
        "     "
        "\"right\" : {\n                                            "
        "\"node\" : "
        "\"post_inc_dec_expression\",\n                                   "
        "     "
        "    \"right\" : {\n                                              "
        "\"node\" : \"lvalue\",\n                                         "
        "     "
        "\"root\" : \"y\"\n                                            "
        "},\n    "
        "                                        \"root\" : [\"--\"]\n    "
        "     "
        "                                 },\n                            "
        "     "
        "         \"root\" : [\"=\", null]\n                              "
        "     "
        "     }]],\n                                    \"node\" : "
        "\"statement\",\n                                    \"root\" : "
        "\"rvalue\"\n                                  }],\n              "
        "     "
        "             \"node\" : \"statement\",\n                         "
        "     "
        "  \"root\" : \"block\"\n                              }],\n      "
        "     "
        "                 \"root\" : \"while\"\n                          "
        "}],\n                        \"node\" : \"statement\",\n         "
        "     "
        "          \"root\" : \"block\"\n                      }, "
        "null],\n     "
        "               \"root\" : \"if\"\n                  }],\n        "
        "     "
        "   \"node\" : \"statement\",\n                \"root\" : "
        "\"block\"\n  "
        "            }, null],\n            \"root\" : \"if\"\n          "
        "}, "
        "{\n            \"left\" : [[{\n                  \"node\" : "
        "\"post_inc_dec_expression\",\n                  \"right\" : {\n  "
        "     "
        "             \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"x\"\n                  },\n                  \"root\" : "
        "[\"++\"]\n  "
        "              }], [{\n                  \"left\" : {\n           "
        "     "
        "    \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"y\"\n    "
        "              },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 5\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }], [{\n                  "
        "\"left\" : "
        "{\n                    \"node\" : \"lvalue\",\n                  "
        "  "
        "\"root\" : \"x\"\n                  },\n                  "
        "\"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"left\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n  "
        "     "
        "                 \"left\" : {\n                          "
        "\"node\" : "
        "\"lvalue\",\n                          \"root\" : \"x\"\n        "
        "     "
        "           },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : "
        "{\n     "
        "                     \"node\" : \"lvalue\",\n                    "
        "     "
        " \"root\" : \"y\"\n                        },\n                  "
        "     "
        " \"root\" : [\"+\"]\n                      }\n                   "
        " "
        "},\n                    \"node\" : \"relation_expression\",\n    "
        "     "
        "           \"right\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n  "
        "     "
        "                 \"left\" : {\n                          "
        "\"node\" : "
        "\"lvalue\",\n                          \"root\" : \"x\"\n        "
        "     "
        "           },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : "
        "{\n     "
        "                     \"node\" : \"lvalue\",\n                    "
        "     "
        " \"root\" : \"x\"\n                        },\n                  "
        "     "
        " \"root\" : [\"+\"]\n                      }\n                   "
        " "
        "},\n                    \"root\" : [\"*\"]\n                  "
        "},\n    "
        "              \"root\" : [\"=\", null]\n                }]],\n   "
        "     "
        "    \"node\" : \"statement\",\n            \"root\" : "
        "\"rvalue\"\n    "
        "      }],\n        \"node\" : \"statement\",\n        \"root\" : "
        "\"block\"\n      }");

    obj["while_2"] = LOAD_JSON_FROM_STRING(
        "       {\n        \"left\" : [{\n            \"left\" : [{\n     "
        "     "
        "      \"node\" : \"lvalue\",\n                \"root\" : \"x\"\n "
        "     "
        "        }, {\n                \"node\" : \"lvalue\",\n           "
        "     "
        "\"root\" : \"y\"\n              }],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"auto\"\n          }, "
        "{\n     "
        "       \"left\" : [[{\n                  \"left\" : {\n          "
        "     "
        "     \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"x\"\n   "
        "               },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 1\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }], [{\n                  "
        "\"left\" : "
        "{\n                    \"node\" : \"lvalue\",\n                  "
        "  "
        "\"root\" : \"y\"\n                  },\n                  "
        "\"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 10\n                  },\n                  \"root\" "
        ": "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }, "
        "{\n   "
        "         \"left\" : {\n              \"left\" : {\n              "
        "  "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n       "
        "     "
        "  },\n              \"node\" : \"relation_expression\",\n        "
        "     "
        " \"right\" : {\n                \"node\" : \"number_literal\",\n "
        "     "
        "          \"root\" : 0\n              },\n              \"root\" "
        ": "
        "[\">=\"]\n            },\n            \"node\" : "
        "\"statement\",\n     "
        "       \"right\" : [{\n                \"left\" : [{\n           "
        "     "
        "    \"left\" : [[{\n                          \"node\" : "
        "\"post_inc_dec_expression\",\n                          "
        "\"right\" : "
        "{\n                            \"node\" : \"lvalue\",\n          "
        "     "
        "             \"root\" : \"x\"\n                          },\n    "
        "     "
        "                 \"root\" : [\"--\"]\n                        "
        "}], "
        "[{\n                          \"left\" : {\n                     "
        "     "
        "  \"node\" : \"lvalue\",\n                            \"root\" : "
        "\"x\"\n                          },\n                          "
        "\"node\" : \"assignment_expression\",\n                          "
        "\"right\" : {\n                            \"node\" : "
        "\"post_inc_dec_expression\",\n                            "
        "\"right\" : "
        "{\n                              \"node\" : \"lvalue\",\n        "
        "     "
        "                 \"root\" : \"y\"\n                            "
        "},\n   "
        "                         \"root\" : [\"--\"]\n                   "
        "     "
        "  },\n                          \"root\" : [\"=\", null]\n       "
        "     "
        "            }]],\n                    \"node\" : "
        "\"statement\",\n     "
        "               \"root\" : \"rvalue\"\n                  }],\n    "
        "     "
        "       \"node\" : \"statement\",\n                \"root\" : "
        "\"block\"\n              }],\n            \"root\" : \"while\"\n "
        "     "
        "    }, {\n            \"left\" : {\n              \"left\" : {\n "
        "     "
        "          \"node\" : \"lvalue\",\n                \"root\" : "
        "\"x\"\n  "
        "            },\n              \"node\" : "
        "\"relation_expression\",\n   "
        "           \"right\" : {\n                \"node\" : "
        "\"number_literal\",\n                \"root\" : 100\n            "
        "  "
        "},\n              \"root\" : [\"<=\"]\n            },\n          "
        "  "
        "\"node\" : \"statement\",\n            \"right\" : [{\n          "
        "     "
        " \"left\" : [{\n                    \"left\" : [[{\n             "
        "     "
        "        \"node\" : \"post_inc_dec_expression\",\n                "
        "     "
        "     \"right\" : {\n                            \"node\" : "
        "\"lvalue\",\n                            \"root\" : \"x\"\n      "
        "     "
        "               },\n                          \"root\" : "
        "[\"++\"]\n    "
        "                    }]],\n                    \"node\" : "
        "\"statement\",\n                    \"root\" : \"rvalue\"\n      "
        "     "
        "       }],\n                \"node\" : \"statement\",\n          "
        "     "
        " \"root\" : \"block\"\n              }],\n            \"root\" : "
        "\"while\"\n          }, {\n            \"left\" : [[{\n          "
        "     "
        "   \"left\" : {\n                    \"node\" : \"lvalue\",\n    "
        "     "
        "           \"root\" : \"x\"\n                  },\n              "
        "    "
        "\"node\" : \"assignment_expression\",\n                  "
        "\"right\" : "
        "{\n                    \"node\" : \"number_literal\",\n          "
        "     "
        "     \"root\" : 2\n                  },\n                  "
        "\"root\" : "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          "
        "}],\n    "
        "    \"node\" : \"statement\",\n        \"root\" : \"block\"\n    "
        "  }");

    obj["while"] = LOAD_JSON_FROM_STRING(
        "{\n        \"left\" : [{\n            \"left\" : [{\n            "
        "    "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n       "
        "     "
        "  }, {\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"y\"\n              }],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"auto\"\n          }, "
        "{\n     "
        "       \"left\" : [[{\n                  \"left\" : {\n          "
        "     "
        "     \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"x\"\n   "
        "               },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"left\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n  "
        "     "
        "                 \"left\" : {\n                          "
        "\"node\" : "
        "\"number_literal\",\n                          \"root\" : 5\n    "
        "     "
        "               },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : "
        "{\n     "
        "                     \"node\" : \"number_literal\",\n            "
        "     "
        "         \"root\" : 5\n                        },\n              "
        "     "
        "     \"root\" : [\"+\"]\n                      }\n               "
        "     "
        "},\n                    \"node\" : \"relation_expression\",\n    "
        "     "
        "           \"right\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n  "
        "     "
        "                 \"left\" : {\n                          "
        "\"node\" : "
        "\"number_literal\",\n                          \"root\" : 3\n    "
        "     "
        "               },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : "
        "{\n     "
        "                     \"node\" : \"number_literal\",\n            "
        "     "
        "         \"root\" : 3\n                        },\n              "
        "     "
        "     \"root\" : [\"+\"]\n                      }\n               "
        "     "
        "},\n                    \"root\" : [\"*\"]\n                  "
        "},\n    "
        "              \"root\" : [\"=\", null]\n                }], [{\n "
        "     "
        "            \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"y\"\n              "
        "    "
        "},\n                  \"node\" : \"assignment_expression\",\n    "
        "     "
        "         \"right\" : {\n                    \"node\" : "
        "\"number_literal\",\n                    \"root\" : 100\n        "
        "     "
        "     },\n                  \"root\" : [\"=\", null]\n            "
        "    "
        "}]],\n            \"node\" : \"statement\",\n            "
        "\"root\" : "
        "\"rvalue\"\n          }, {\n            \"left\" : {\n           "
        "   "
        "\"left\" : {\n                \"node\" : \"lvalue\",\n           "
        "     "
        "\"root\" : \"x\"\n              },\n              \"node\" : "
        "\"relation_expression\",\n              \"right\" : {\n          "
        "     "
        " \"node\" : \"number_literal\",\n                \"root\" : 0\n  "
        "     "
        "       },\n              \"root\" : [\">\"]\n            },\n    "
        "     "
        "   \"node\" : \"statement\",\n            \"right\" : [{\n       "
        "     "
        "    \"left\" : [{\n                    \"left\" : {\n            "
        "     "
        "     \"left\" : {\n                        \"node\" : "
        "\"lvalue\",\n   "
        "                     \"root\" : \"x\"\n                      "
        "},\n     "
        "                 \"node\" : \"relation_expression\",\n           "
        "     "
        "      \"right\" : {\n                        \"node\" : "
        "\"number_literal\",\n                        \"root\" : 0\n      "
        "     "
        "           },\n                      \"root\" : [\">=\"]\n       "
        "     "
        "        },\n                    \"node\" : \"statement\",\n      "
        "     "
        "         \"right\" : [{\n                        \"left\" : [{\n "
        "     "
        "                      \"left\" : [[{\n                           "
        "     "
        "  \"node\" : \"post_inc_dec_expression\",\n                      "
        "     "
        "       \"right\" : {\n                                    "
        "\"node\" : "
        "\"lvalue\",\n                                    \"root\" : "
        "\"x\"\n   "
        "                               },\n                              "
        "    "
        "\"root\" : [\"--\"]\n                                }], [{\n    "
        "     "
        "                         \"left\" : {\n                          "
        "     "
        "     \"node\" : \"lvalue\",\n                                    "
        "\"root\" : \"x\"\n                                  },\n         "
        "     "
        "                    \"node\" : \"assignment_expression\",\n      "
        "     "
        "                       \"right\" : {\n                           "
        "     "
        "    \"node\" : \"post_inc_dec_expression\",\n                    "
        "     "
        "           \"right\" : {\n                                      "
        "\"node\" : \"lvalue\",\n                                      "
        "\"root\" : \"y\"\n                                    },\n       "
        "     "
        "                        \"root\" : [\"--\"]\n                    "
        "     "
        "         },\n                                  \"root\" : "
        "[\"=\", "
        "null]\n                                }]],\n                    "
        "     "
        "   \"node\" : \"statement\",\n                            "
        "\"root\" : "
        "\"rvalue\"\n                          }],\n                      "
        "  "
        "\"node\" : \"statement\",\n                        \"root\" : "
        "\"block\"\n                      }],\n                    "
        "\"root\" : "
        "\"while\"\n                  }],\n                \"node\" : "
        "\"statement\",\n                \"root\" : \"block\"\n           "
        "   "
        "}, {\n                \"left\" : [{\n                    "
        "\"left\" : "
        "[[{\n                          \"node\" : "
        "\"post_inc_dec_expression\",\n                          "
        "\"right\" : "
        "{\n                            \"node\" : \"lvalue\",\n          "
        "     "
        "             \"root\" : \"x\"\n                          },\n    "
        "     "
        "                 \"root\" : [\"++\"]\n                        "
        "}], "
        "[{\n                          \"left\" : {\n                     "
        "     "
        "  \"node\" : \"lvalue\",\n                            \"root\" : "
        "\"y\"\n                          },\n                          "
        "\"node\" : \"assignment_expression\",\n                          "
        "\"right\" : {\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 5\n  "
        "     "
        "                   },\n                          \"root\" : "
        "[\"=\", "
        "null]\n                        }], [{\n                          "
        "\"left\" : {\n                            \"node\" : "
        "\"lvalue\",\n    "
        "                        \"root\" : \"x\"\n                       "
        "   "
        "},\n                          \"node\" : "
        "\"assignment_expression\",\n "
        "                         \"right\" : {\n                         "
        "   "
        "\"left\" : {\n                              \"node\" : "
        "\"evaluated_expression\",\n                              "
        "\"root\" : "
        "{\n                                \"left\" : {\n                "
        "     "
        "             \"node\" : \"lvalue\",\n                            "
        "     "
        " \"root\" : \"x\"\n                                },\n          "
        "     "
        "                 \"node\" : \"relation_expression\",\n           "
        "     "
        "                \"right\" : {\n                                  "
        "\"node\" : \"lvalue\",\n                                  "
        "\"root\" : "
        "\"y\"\n                                },\n                      "
        "     "
        "     \"root\" : [\"+\"]\n                              }\n       "
        "     "
        "                },\n                            \"node\" : "
        "\"relation_expression\",\n                            \"right\" "
        ": {\n "
        "                             \"node\" : "
        "\"evaluated_expression\",\n   "
        "                           \"root\" : {\n                        "
        "     "
        "   \"left\" : {\n                                  \"node\" : "
        "\"lvalue\",\n                                  \"root\" : "
        "\"x\"\n     "
        "                           },\n                                "
        "\"node\" : \"relation_expression\",\n                            "
        "    "
        "\"right\" : {\n                                  \"node\" : "
        "\"lvalue\",\n                                  \"root\" : "
        "\"x\"\n     "
        "                           },\n                                "
        "\"root\" : [\"+\"]\n                              }\n            "
        "     "
        "           },\n                            \"root\" : [\"*\"]\n  "
        "     "
        "                   },\n                          \"root\" : "
        "[\"=\", "
        "null]\n                        }]],\n                    "
        "\"node\" : "
        "\"statement\",\n                    \"root\" : \"rvalue\"\n      "
        "     "
        "       }],\n                \"node\" : \"statement\",\n          "
        "     "
        " \"root\" : \"block\"\n              }],\n            \"root\" : "
        "\"if\"\n          }, {\n            \"left\" : [[{\n             "
        "     "
        "\"left\" : {\n                    \"node\" : \"lvalue\",\n       "
        "     "
        "        \"root\" : \"x\"\n                  },\n                 "
        " "
        "\"node\" : \"assignment_expression\",\n                  "
        "\"right\" : "
        "{\n                    \"node\" : \"number_literal\",\n          "
        "     "
        "     \"root\" : 2\n                  },\n                  "
        "\"root\" : "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          "
        "}],\n    "
        "    \"node\" : \"statement\",\n        \"root\" : \"block\"\n    "
        "  }");

    auto expected = R"ita(LOCL x;
LOCL y;
_t2 = (5:int:4) + (5:int:4);
_t3 = (3:int:4) + (3:int:4);
_t4 = _t2 * _t3;
x = _t4;
y = (100:int:4);
_L5:
_t8 = x > (0:int:4);
IF _t8 GOTO _L7;
GOTO _L13;
_L6:
x = (2:int:4);
_L1:
LEAVE;
_L7:
_L9:
_L11:
_t12 = x >= (0:int:4);
IF _t12 GOTO _L10;
GOTO _L6;
_L10:
x = --x;
y = --y;
x = y;
GOTO _L9;
_L13:
x = ++x;
y = (5:int:4);
_t14 = x + y;
_t15 = x + x;
_t16 = _t14 * _t15;
x = _t16;
GOTO _L6;
)ita";
    auto expected_2 = R"ita(LOCL x;
LOCL y;
x = (1:int:4);
y = (10:int:4);
_L2:
_L4:
_t5 = x >= (0:int:4);
IF _t5 GOTO _L3;
_L6:
_L8:
_t9 = x <= (100:int:4);
IF _t9 GOTO _L7;
x = (2:int:4);
_L1:
LEAVE;
_L3:
x = --x;
y = --y;
x = y;
GOTO _L2;
_L7:
x = ++x;
GOTO _L6;
)ita";
    std::string expected_3 = R"ita(x = (100:int:4);
y = (100:int:4);
_L2:
_t5 = x == (100:int:4);
IF _t5 GOTO _L4;
_L3:
x = ++x;
y = (5:int:4);
_t14 = x + y;
_t15 = x + x;
_t16 = _t14 * _t15;
x = _t16;
_L1:
LEAVE;
_L4:
_L6:
_t9 = x > (5:int:4);
IF _t9 GOTO _L8;
_L7:
GOTO _L3;
_L8:
_L10:
_L12:
_t13 = x >= (0:int:4);
IF _t13 GOTO _L11;
GOTO _L7;
_L11:
x = --x;
y = --y;
x = y;
GOTO _L10;
)ita";
    TEST_BLOCK_STATEMENT_NODE_WITH(obj["symbols"], obj["while"], expected);
    TEST_BLOCK_STATEMENT_NODE_WITH(obj["symbols"], obj["while_2"], expected_2);
    TEST_BLOCK_STATEMENT_NODE_WITH(
        obj["symbols"], obj["while_4"], expected_3, { "x", "y" });
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: if and else branching")
{

    auto obj = make_node();
    obj["if"] = LOAD_JSON_FROM_STRING(
        "{\n        \"left\" : [{\n            \"left\" : [{\n            "
        "    "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n       "
        "     "
        "  }],\n            \"node\" : \"statement\",\n            "
        "\"root\" : "
        "\"auto\"\n          }, \n          {\n            \"left\" : "
        "[[{\n    "
        "              \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n              "
        "    "
        "},\n                  \"node\" : \"assignment_expression\",\n    "
        "     "
        "         \"right\" : {\n                    \"left\" : {\n       "
        "     "
        "          \"node\" : \"evaluated_expression\",\n                 "
        "     "
        "\"root\" : {\n                        \"left\" : {\n             "
        "     "
        "        \"node\" : \"number_literal\",\n                         "
        " "
        "\"root\" : 5\n                        },\n                       "
        " "
        "\"node\" : \"relation_expression\",\n                        "
        "\"right\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 5\n    "
        "     "
        "               },\n                        \"root\" : [\"+\"]\n  "
        "     "
        "               }\n                    },\n                    "
        "\"node\" : \"relation_expression\",\n                    "
        "\"right\" : "
        "{\n                      \"node\" : \"evaluated_expression\",\n  "
        "     "
        "               \"root\" : {\n                        \"left\" : "
        "{\n   "
        "                       \"node\" : \"number_literal\",\n          "
        "     "
        "           \"root\" : 3\n                        },\n            "
        "     "
        "       \"node\" : \"relation_expression\",\n                     "
        "   "
        "\"right\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 3\n    "
        "     "
        "               },\n                        \"root\" : [\"+\"]\n  "
        "     "
        "               }\n                    },\n                    "
        "\"root\" : [\"*\"]\n                  },\n                  "
        "\"root\" "
        ": [\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }, "
        "{\n   "
        "         \"left\" : {\n              \"left\" : {\n              "
        "  "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n       "
        "     "
        "  },\n              \"node\" : \"relation_expression\",\n        "
        "     "
        " \"right\" : {\n                \"node\" : \"number_literal\",\n "
        "     "
        "          \"root\" : 5\n              },\n              \"root\" "
        ": "
        "[\"<=\"]\n            },\n            \"node\" : "
        "\"statement\",\n     "
        "       \"right\" : [{\n                \"left\" : [{\n           "
        "     "
        "    \"left\" : [[{\n                          \"left\" : {\n     "
        "     "
        "                  \"node\" : \"lvalue\",\n                       "
        "     "
        "\"root\" : \"x\"\n                          },\n                 "
        "     "
        "    \"node\" : \"assignment_expression\",\n                      "
        "    "
        "\"right\" : {\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 1\n  "
        "     "
        "                   },\n                          \"root\" : "
        "[\"=\", "
        "null]\n                        }]],\n                    "
        "\"node\" : "
        "\"statement\",\n                    \"root\" : \"rvalue\"\n      "
        "     "
        "       }],\n                \"node\" : \"statement\",\n          "
        "     "
        " \"root\" : \"block\"\n              }, {\n                "
        "\"left\" : "
        "[{\n                    \"left\" : [[{\n                         "
        " "
        "\"left\" : {\n                            \"node\" : "
        "\"lvalue\",\n    "
        "                        \"root\" : \"x\"\n                       "
        "   "
        "},\n                          \"node\" : "
        "\"assignment_expression\",\n "
        "                         \"right\" : {\n                         "
        "   "
        "\"node\" : \"number_literal\",\n                            "
        "\"root\" "
        ": 8\n                          },\n                          "
        "\"root\" "
        ": [\"=\", null]\n                        }]],\n                  "
        "  "
        "\"node\" : \"statement\",\n                    \"root\" : "
        "\"rvalue\"\n                  }],\n                \"node\" : "
        "\"statement\",\n                \"root\" : \"block\"\n           "
        "   "
        "}],\n            \"root\" : \"if\"\n          }, {\n            "
        "\"left\" : [[{\n                  \"left\" : {\n                 "
        "   "
        "\"node\" : \"lvalue\",\n                    \"root\" : \"x\"\n   "
        "     "
        "          },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"left\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 5\n        "
        "     "
        "       },\n                    \"node\" : "
        "\"relation_expression\",\n  "
        "                  \"right\" : {\n                      \"node\" "
        ": "
        "\"number_literal\",\n                      \"root\" : 2\n        "
        "     "
        "       },\n                    \"root\" : [\"||\"]\n             "
        "     "
        "},\n                  \"root\" : [\"=\", null]\n                "
        "}]],\n            \"node\" : \"statement\",\n            "
        "\"root\" : "
        "\"rvalue\"\n          }],\n        \"node\" : \"statement\",\n   "
        "     "
        "\"root\" : \"block\"\n      }");

    obj["if_2"] = LOAD_JSON_FROM_STRING(
        "      {\n        \"left\" : [{\n            \"left\" : [{\n      "
        "     "
        "     \"node\" : \"lvalue\",\n                \"root\" : \"x\"\n  "
        "     "
        "       }],\n            \"node\" : \"statement\",\n            "
        "\"root\" : \"auto\"\n          }, {\n            \"left\" : "
        "[[{\n     "
        "             \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n              "
        "    "
        "},\n                  \"node\" : \"assignment_expression\",\n    "
        "     "
        "         \"right\" : {\n                    \"left\" : {\n       "
        "     "
        "          \"node\" : \"evaluated_expression\",\n                 "
        "     "
        "\"root\" : {\n                        \"left\" : {\n             "
        "     "
        "        \"node\" : \"number_literal\",\n                         "
        " "
        "\"root\" : 5\n                        },\n                       "
        " "
        "\"node\" : \"relation_expression\",\n                        "
        "\"right\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 5\n    "
        "     "
        "               },\n                        \"root\" : [\"+\"]\n  "
        "     "
        "               }\n                    },\n                    "
        "\"node\" : \"relation_expression\",\n                    "
        "\"right\" : "
        "{\n                      \"node\" : \"evaluated_expression\",\n  "
        "     "
        "               \"root\" : {\n                        \"left\" : "
        "{\n   "
        "                       \"node\" : \"number_literal\",\n          "
        "     "
        "           \"root\" : 3\n                        },\n            "
        "     "
        "       \"node\" : \"relation_expression\",\n                     "
        "   "
        "\"right\" : {\n                          \"node\" : "
        "\"number_literal\",\n                          \"root\" : 3\n    "
        "     "
        "               },\n                        \"root\" : [\"+\"]\n  "
        "     "
        "               }\n                    },\n                    "
        "\"root\" : [\"*\"]\n                  },\n                  "
        "\"root\" "
        ": [\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }, "
        "{\n   "
        "         \"left\" : {\n              \"left\" : {\n              "
        "  "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n       "
        "     "
        "  },\n              \"node\" : \"relation_expression\",\n        "
        "     "
        " \"right\" : {\n                \"node\" : \"number_literal\",\n "
        "     "
        "          \"root\" : 5\n              },\n              \"root\" "
        ": "
        "[\"<=\"]\n            },\n            \"node\" : "
        "\"statement\",\n     "
        "       \"right\" : [{\n                \"left\" : [{\n           "
        "     "
        "    \"left\" : [[{\n                          \"left\" : {\n     "
        "     "
        "                  \"node\" : \"lvalue\",\n                       "
        "     "
        "\"root\" : \"x\"\n                          },\n                 "
        "     "
        "    \"node\" : \"assignment_expression\",\n                      "
        "    "
        "\"right\" : {\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 1\n  "
        "     "
        "                   },\n                          \"root\" : "
        "[\"=\", "
        "null]\n                        }]],\n                    "
        "\"node\" : "
        "\"statement\",\n                    \"root\" : \"rvalue\"\n      "
        "     "
        "       }],\n                \"node\" : \"statement\",\n          "
        "     "
        " \"root\" : \"block\"\n              }, null],\n            "
        "\"root\" "
        ": \"if\"\n          }, {\n            \"left\" : {\n             "
        " "
        "\"left\" : {\n                \"node\" : \"lvalue\",\n           "
        "     "
        "\"root\" : \"x\"\n              },\n              \"node\" : "
        "\"relation_expression\",\n              \"right\" : {\n          "
        "     "
        " \"node\" : \"number_literal\",\n                \"root\" : 5\n  "
        "     "
        "       },\n              \"root\" : [\">\"]\n            },\n    "
        "     "
        "   \"node\" : \"statement\",\n            \"right\" : [{\n       "
        "     "
        "    \"left\" : [{\n                    \"left\" : [[{\n          "
        "     "
        "           \"left\" : {\n                            \"node\" : "
        "\"lvalue\",\n                            \"root\" : \"x\"\n      "
        "     "
        "               },\n                          \"node\" : "
        "\"assignment_expression\",\n                          \"right\" "
        ": {\n "
        "                           \"node\" : \"number_literal\",\n      "
        "     "
        "                 \"root\" : 10\n                          },\n   "
        "     "
        "                  \"root\" : [\"=\", null]\n                     "
        "   "
        "}]],\n                    \"node\" : \"statement\",\n            "
        "     "
        "   \"root\" : \"rvalue\"\n                  }],\n                "
        "\"node\" : \"statement\",\n                \"root\" : "
        "\"block\"\n     "
        "         }, null],\n            \"root\" : \"if\"\n          }, "
        "{\n   "
        "         \"left\" : [[{\n                  \"left\" : {\n        "
        "     "
        "       \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"x\"\n "
        "                 },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"left\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 5\n        "
        "     "
        "       },\n                    \"node\" : "
        "\"relation_expression\",\n  "
        "                  \"right\" : {\n                      \"node\" "
        ": "
        "\"number_literal\",\n                      \"root\" : 2\n        "
        "     "
        "       },\n                    \"root\" : [\"||\"]\n             "
        "     "
        "},\n                  \"root\" : [\"=\", null]\n                "
        "}]],\n            \"node\" : \"statement\",\n            "
        "\"root\" : "
        "\"rvalue\"\n          }],\n        \"node\" : \"statement\",\n   "
        "     "
        "\"root\" : \"block\"\n      }");
    obj["if_5"] = LOAD_JSON_FROM_STRING(
        "{\n        \"left\" : [ \n          {\n            \"left\" : "
        "[[{\n   "
        "               \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n              "
        "    "
        "},\n                  \"node\" : \"assignment_expression\",\n    "
        "     "
        "         \"right\" : {\n                    \"node\" : "
        "\"number_literal\",\n                    \"root\" : 5\n          "
        "     "
        "   },\n                  \"root\" : [\"=\", null]\n              "
        "  "
        "}]],\n            \"node\" : \"statement\",\n            "
        "\"root\" : "
        "\"rvalue\"\n          }, {\n            \"left\" : {\n           "
        "   "
        "\"node\" : \"lvalue\",\n              \"root\" : \"x\"\n         "
        "   "
        "},\n            \"node\" : \"statement\",\n            \"right\" "
        ": "
        "[{\n                \"left\" : [{\n                    \"left\" "
        ": "
        "[[{\n                          \"left\" : {\n                    "
        "     "
        "   \"node\" : \"lvalue\",\n                            \"root\" "
        ": "
        "\"y\"\n                          },\n                          "
        "\"node\" : \"assignment_expression\",\n                          "
        "\"right\" : {\n                            \"node\" : "
        "\"lvalue\",\n   "
        "                         \"root\" : \"x\"\n                      "
        "    "
        "},\n                          \"root\" : [\"=\", null]\n         "
        "     "
        "          }]],\n                    \"node\" : \"statement\",\n  "
        "     "
        "             \"root\" : \"rvalue\"\n                  }, {\n     "
        "     "
        "          \"left\" : {\n                      \"left\" : {\n     "
        "     "
        "              \"node\" : \"lvalue\",\n                        "
        "\"root\" : \"y\"\n                      },\n                     "
        " "
        "\"node\" : \"unary_expression\",\n                      \"root\" "
        ": "
        "[\"!\"]\n                    },\n                    \"node\" : "
        "\"statement\",\n                    \"right\" : [{\n             "
        "     "
        "      \"left\" : [{\n                            \"left\" : {\n  "
        "     "
        "                       \"left\" : {\n                            "
        "    "
        "\"node\" : \"lvalue\",\n                                \"root\" "
        ": "
        "\"x\"\n                              },\n                        "
        "     "
        " \"node\" : \"relation_expression\",\n                           "
        "   "
        "\"right\" : {\n                                \"node\" : "
        "\"lvalue\",\n                                \"root\" : \"y\"\n  "
        "     "
        "                       },\n                              "
        "\"root\" : "
        "[\">\"]\n                            },\n                        "
        "    "
        "\"node\" : \"statement\",\n                            \"right\" "
        ": "
        "[{\n                                \"left\" : [{\n              "
        "     "
        "                 \"left\" : {\n                                  "
        "    "
        "\"left\" : {\n                                        \"node\" : "
        "\"lvalue\",\n                                        \"root\" : "
        "\"x\"\n                                      },\n                "
        "     "
        "                 \"node\" : \"unary_expression\",\n              "
        "     "
        "                   \"root\" : [\"!\"]\n                          "
        "     "
        "     },\n                                    \"node\" : "
        "\"statement\",\n                                    \"right\" : "
        "[{\n  "
        "                                      \"left\" : [{\n            "
        "     "
        "                           \"left\" : [[{\n                      "
        "     "
        "                       \"left\" : {\n                            "
        "     "
        "                   \"node\" : \"lvalue\",\n                      "
        "     "
        "                         \"root\" : \"x\"\n                      "
        "     "
        "                       },\n                                      "
        "     "
        "       \"node\" : \"assignment_expression\",\n                   "
        "     "
        "                          \"right\" : {\n                        "
        "     "
        "                       \"node\" : \"lvalue\",\n                  "
        "     "
        "                             \"root\" : \"y\"\n                  "
        "     "
        "                           },\n                                  "
        "     "
        "           \"root\" : [\"=\", null]\n                            "
        "     "
        "               }]],\n                                            "
        "\"node\" : \"statement\",\n                                      "
        "     "
        " \"root\" : \"rvalue\"\n                                         "
        " "
        "}],\n                                        \"node\" : "
        "\"statement\",\n                                        \"root\" "
        ": "
        "\"block\"\n                                      }, null],\n     "
        "     "
        "                          \"root\" : \"if\"\n                    "
        "     "
        "         }, {\n                                    \"left\" : "
        "[[{\n   "
        "                                       \"left\" : {\n            "
        "     "
        "                           \"node\" : \"lvalue\",\n              "
        "     "
        "                         \"root\" : \"x\"\n                      "
        "     "
        "               },\n                                          "
        "\"node\" "
        ": \"assignment_expression\",\n                                   "
        "     "
        "  \"right\" : {\n                                            "
        "\"left\" "
        ": {\n                                              \"node\" : "
        "\"number_literal\",\n                                            "
        "  "
        "\"root\" : 3\n                                            },\n   "
        "     "
        "                                    \"node\" : "
        "\"relation_expression\",\n                                       "
        "     "
        "\"right\" : {\n                                              "
        "\"node\" "
        ": \"number_literal\",\n                                          "
        "    "
        "\"root\" : 3\n                                            },\n   "
        "     "
        "                                    \"root\" : [\"+\"]\n         "
        "     "
        "                            },\n                                 "
        "     "
        "    \"root\" : [\"=\", null]\n                                   "
        "     "
        "}]],\n                                    \"node\" : "
        "\"statement\",\n "
        "                                   \"root\" : \"rvalue\"\n       "
        "     "
        "                      }],\n                                "
        "\"node\" : "
        "\"statement\",\n                                \"root\" : "
        "\"block\"\n                              }, null],\n             "
        "     "
        "          \"root\" : \"if\"\n                          }, {\n    "
        "     "
        "                   \"left\" : [[{\n                              "
        "    "
        "\"left\" : {\n                                    \"node\" : "
        "\"lvalue\",\n                                    \"root\" : "
        "\"x\"\n   "
        "                               },\n                              "
        "    "
        "\"node\" : \"assignment_expression\",\n                          "
        "     "
        "   \"right\" : {\n                                    \"left\" : "
        "{\n  "
        "                                    \"node\" : "
        "\"number_literal\",\n  "
        "                                    \"root\" : 1\n               "
        "     "
        "                },\n                                    \"node\" "
        ": "
        "\"relation_expression\",\n                                    "
        "\"right\" : {\n                                      \"node\" : "
        "\"number_literal\",\n                                      "
        "\"root\" : "
        "1\n                                    },\n                      "
        "     "
        "         \"root\" : [\"+\"]\n                                  "
        "},\n   "
        "                               \"root\" : [\"=\", null]\n        "
        "     "
        "                   }]],\n                            \"node\" : "
        "\"statement\",\n                            \"root\" : "
        "\"rvalue\"\n   "
        "                       }],\n                        \"node\" : "
        "\"statement\",\n                        \"root\" : \"block\"\n   "
        "     "
        "              }, null],\n                    \"root\" : \"if\"\n "
        "     "
        "            }, {\n                    \"left\" : [[{\n           "
        "     "
        "          \"left\" : {\n                            \"node\" : "
        "\"lvalue\",\n                            \"root\" : \"x\"\n      "
        "     "
        "               },\n                          \"node\" : "
        "\"assignment_expression\",\n                          \"right\" "
        ": {\n "
        "                           \"left\" : {\n                        "
        "     "
        " \"node\" : \"number_literal\",\n                              "
        "\"root\" : 2\n                            },\n                   "
        "     "
        "    \"node\" : \"relation_expression\",\n                        "
        "    "
        "\"right\" : {\n                              \"node\" : "
        "\"number_literal\",\n                              \"root\" : "
        "2\n     "
        "                       },\n                            \"root\" "
        ": "
        "[\"+\"]\n                          },\n                          "
        "\"root\" : [\"=\", null]\n                        }]],\n         "
        "     "
        "      \"node\" : \"statement\",\n                    \"root\" : "
        "\"rvalue\"\n                  }],\n                \"node\" : "
        "\"statement\",\n                \"root\" : \"block\"\n           "
        "   "
        "}, null],\n            \"root\" : \"if\"\n          }],\n        "
        "\"node\" : \"statement\",\n        \"root\" : \"block\"\n      "
        "}");

    auto expected = R"ita(LOCL x;
_t2 = (5:int:4) + (5:int:4);
_t3 = (3:int:4) + (3:int:4);
_t4 = _t2 * _t3;
x = _t4;
_L5:
_t8 = x <= (5:int:4);
IF _t8 GOTO _L7;
GOTO _L9;
_L6:
_t10 = (5:int:4) || (2:int:4);
x = _t10;
_L1:
LEAVE;
_L7:
x = (1:int:4);
GOTO _L6;
_L9:
x = (8:int:4);
GOTO _L6;
)ita";

    auto expected_2 = R"ita(LOCL x;
_t2 = (5:int:4) + (5:int:4);
_t3 = (3:int:4) + (3:int:4);
_t4 = _t2 * _t3;
x = _t4;
_L5:
_t8 = x <= (5:int:4);
IF _t8 GOTO _L7;
_L6:
_L9:
_t12 = x > (5:int:4);
IF _t12 GOTO _L11;
_L10:
_t13 = (5:int:4) || (2:int:4);
x = _t13;
_L1:
LEAVE;
_L7:
x = (1:int:4);
GOTO _L6;
_L11:
x = (10:int:4);
GOTO _L10;
)ita";

    auto expected_3 = R"ita(x = (5:int:4);
_L2:
_t5 = CMP x;
IF _t5 GOTO _L4;
_L3:
_L1:
LEAVE;
_L4:
y = x;
_L6:
_t9 = ! y;
IF _t9 GOTO _L8;
_L7:
_t20 = (2:int:4) + (2:int:4);
x = _t20;
GOTO _L3;
_L8:
_L10:
_t13 = x > y;
IF _t13 GOTO _L12;
_L11:
_t19 = (1:int:4) + (1:int:4);
x = _t19;
GOTO _L7;
_L12:
_L14:
_t17 = ! x;
IF _t17 GOTO _L16;
_L15:
_t18 = (3:int:4) + (3:int:4);
x = _t18;
GOTO _L11;
_L16:
x = y;
GOTO _L15;
)ita";

    TEST_BLOCK_STATEMENT_NODE_WITH(obj["symbols"], obj["if"], expected);
    TEST_BLOCK_STATEMENT_NODE_WITH(obj["symbols"], obj["if_2"], expected_2);
    TEST_BLOCK_STATEMENT_NODE_WITH(
        obj["symbols"], obj["if_5"], expected_3, { "x", "y" });
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: truthy type coercion")
{
    auto obj = make_node();
    obj["symbols"] = LOAD_JSON_FROM_STRING(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"y\": "
        "{\"type\": "
        "\"lvalue\", \"line\": 2, \"start_pos\": 19, \"column\": 11, "
        "\"end_pos\": 20, \"end_column\": 12}, \"main\": {\"type\": "
        "\"function_definition\", \"line\": 1, \"start_pos\": 0, "
        "\"column\": "
        "1, \"end_pos\": 4, \"end_column\": 5}}");

    obj["if_4"] = LOAD_JSON_FROM_STRING(
        "{\n        \"left\" : [{\n            \"left\" : [{\n            "
        "    "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n       "
        "     "
        "  }, {\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"y\"\n              }],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"auto\"\n          }, "
        "{\n     "
        "       \"left\" : [[{\n                  \"left\" : {\n          "
        "     "
        "     \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"x\"\n   "
        "               },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 5\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }, "
        "{\n   "
        "         \"left\" : {\n              \"node\" : \"lvalue\",\n    "
        "     "
        "     \"root\" : \"x\"\n            },\n            \"node\" : "
        "\"statement\",\n            \"right\" : [{\n                "
        "\"left\" "
        ": [{\n                    \"left\" : [[{\n                       "
        "   "
        "\"left\" : {\n                            \"node\" : "
        "\"lvalue\",\n    "
        "                        \"root\" : \"y\"\n                       "
        "   "
        "},\n                          \"node\" : "
        "\"assignment_expression\",\n "
        "                         \"right\" : {\n                         "
        "   "
        "\"node\" : \"number_literal\",\n                            "
        "\"root\" "
        ": 10\n                          },\n                          "
        "\"root\" : [\"=\", null]\n                        }]],\n         "
        "     "
        "      \"node\" : \"statement\",\n                    \"root\" : "
        "\"rvalue\"\n                  }],\n                \"node\" : "
        "\"statement\",\n                \"root\" : \"block\"\n           "
        "   "
        "}, null],\n            \"root\" : \"if\"\n          }],\n        "
        "\"node\" : \"statement\",\n        \"root\" : \"block\"\n      "
        "}");

    std::string expected = R"ita(LOCL x;
LOCL y;
x = (5:int:4);
_L2:
_t5 = CMP x;
IF _t5 GOTO _L4;
_L3:
_L1:
LEAVE;
_L4:
y = (10:int:4);
GOTO _L3;
)ita";
    TEST_BLOCK_STATEMENT_NODE_WITH(
        obj["symbols"], obj["if_4"], expected, { "x", "y" }, false);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: unary expression")
{
    auto obj = make_node();
    obj["unary_2"] = LOAD_JSON_FROM_STRING(
        "{\n        \"left\" : [{\n            \"left\" : [{\n            "
        "    "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n       "
        "     "
        "  }, {\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"C\"\n              }, {\n                \"node\" : "
        "\"lvalue\",\n                \"root\" : \"s\"\n              }, "
        "{\n   "
        "             \"node\" : \"lvalue\",\n                \"root\" : "
        "\"j\"\n              }],\n            \"node\" : "
        "\"statement\",\n     "
        "       \"root\" : \"auto\"\n          }, {\n            \"left\" "
        ": "
        "[[{\n                  \"left\" : {\n                    "
        "\"node\" : "
        "\"lvalue\",\n                    \"root\" : \"j\"\n              "
        "    "
        "},\n                  \"node\" : \"assignment_expression\",\n    "
        "     "
        "         \"right\" : {\n                    \"node\" : "
        "\"number_literal\",\n                    \"root\" : 0\n          "
        "     "
        "   },\n                  \"root\" : [\"=\"]\n                }], "
        "[{\n "
        "                 \"node\" : \"post_inc_dec_expression\",\n       "
        "     "
        "      \"right\" : {\n                    \"node\" : "
        "\"lvalue\",\n     "
        "               \"root\" : \"j\"\n                  },\n          "
        "     "
        "   \"root\" : [\"++\"]\n                }], [{\n                 "
        " "
        "\"left\" : {\n                    \"node\" : \"lvalue\",\n       "
        "     "
        "        \"root\" : \"C\"\n                  },\n                 "
        " "
        "\"node\" : \"assignment_expression\",\n                  "
        "\"right\" : "
        "{\n                    \"left\" : {\n                      "
        "\"node\" : "
        "\"lvalue\",\n                      \"root\" : \"char\"\n         "
        "     "
        "      },\n                    \"node\" : "
        "\"function_expression\",\n   "
        "                 \"right\" : [{\n                        "
        "\"node\" : "
        "\"lvalue\",\n                        \"root\" : \"s\"\n          "
        "     "
        "       }, {\n                        \"left\" : {\n              "
        "     "
        "       \"node\" : \"lvalue\",\n                          "
        "\"root\" : "
        "\"j\"\n                        },\n                        "
        "\"node\" : "
        "\"pre_inc_dec_expression\",\n                        \"root\" : "
        "[\"++\"]\n                      }],\n                    "
        "\"root\" : "
        "\"char\"\n                  },\n                  \"root\" : "
        "[\"=\"]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          "
        "}],\n    "
        "    \"node\" : \"statement\",\n        \"root\" : \"block\"\n    "
        "  }");
    auto expected = R"ita(LOCL x;
LOCL C;
LOCL s;
LOCL j;
j = (0:int:4);
j = ++j;
_p1 = s;
j = ++j;
_p2 = j;
PUSH _p2;
PUSH _p1;
CALL char;
POP 16;
_t2 = RET;
C = _t2;
_L1:
LEAVE;
)ita";
    TEST_BLOCK_STATEMENT_NODE_WITH(global_symbols,
        obj["unary_2"],
        expected,
        { "x", "y", "j", "c", "char" },
        false);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: label and goto")
{
    auto obj = make_node();
    obj["symbols"] = LOAD_JSON_FROM_STRING(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"y\": "
        "{\"type\": "
        "\"lvalue\", \"line\": 2, \"start_pos\": 18, \"column\": 10, "
        "\"end_pos\": 19, \"end_column\": 11}, \"ADD\": {\"type\": "
        "\"label\", "
        "\"line\": 3, \"start_pos\": 21, \"column\": 1, \"end_pos\": 25, "
        "\"end_column\": 5}, \"add\": {\"type\": \"function_definition\", "
        "\"line\": 9, \"start_pos\": 67, \"column\": 1, \"end_pos\": 70, "
        "\"end_column\": 4}, \"main\": {\"type\": "
        "\"function_definition\", "
        "\"line\": 1, \"start_pos\": 0, \"column\": 1, \"end_pos\": 4, "
        "\"end_column\": 5}, \"a\": {\"type\": \"lvalue\", \"line\": 9, "
        "\"start_pos\": 71, \"column\": 5, \"end_pos\": 72, "
        "\"end_column\": "
        "6}, \"b\": {\"type\": \"lvalue\", \"line\": 9, \"start_pos\": "
        "73, "
        "\"column\": 7, \"end_pos\": 74, \"end_column\": 8}}");

    obj["test"] = LOAD_JSON_FROM_STRING(
        "{\n        \"left\" : [{\n            \"left\" : [{\n            "
        "    "
        "\"node\" : \"lvalue\",\n                \"root\" : \"x\"\n       "
        "     "
        "  }, {\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"y\"\n              }],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"auto\"\n          }, "
        "{\n     "
        "       \"left\" : [\"ADD\"],\n            \"node\" : "
        "\"statement\",\n "
        "           \"root\" : \"label\"\n          }, {\n            "
        "\"left\" "
        ": [[{\n                  \"left\" : {\n                    "
        "\"node\" : "
        "\"lvalue\",\n                    \"root\" : \"x\"\n              "
        "    "
        "},\n                  \"node\" : \"assignment_expression\",\n    "
        "     "
        "         \"right\" : {\n                    \"left\" : {\n       "
        "     "
        "          \"node\" : \"lvalue\",\n                      \"root\" "
        ": "
        "\"add\"\n                    },\n                    \"node\" : "
        "\"function_expression\",\n                    \"right\" : [{\n   "
        "     "
        "                \"node\" : \"number_literal\",\n                 "
        "     "
        "  \"root\" : 2\n                      }, {\n                     "
        "   "
        "\"node\" : \"number_literal\",\n                        \"root\" "
        ": "
        "5\n                      }],\n                    \"root\" : "
        "\"add\"\n                  },\n                  \"root\" : "
        "[\"=\", "
        "null]\n                }], [{\n                  \"left\" : {\n  "
        "     "
        "             \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"y\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 10\n                  },\n                  \"root\" "
        ": "
        "[\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }, "
        "{\n   "
        "         \"left\" : [\"ADD\"],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"goto\"\n          }],\n "
        "     "
        "  \"node\" : \"statement\",\n        \"root\" : \"block\"\n      "
        "}");

    std::string expected = R"ita(LOCL x;
LOCL y;
__LADD:
_p1 = (2:int:4);
_p2 = (5:int:4);
PUSH _p2;
PUSH _p1;
CALL add;
POP 16;
_t2 = RET;
x = _t2;
y = (10:int:4);
GOTO __LADD;
)ita";
    TEST_BLOCK_STATEMENT_NODE_WITH(
        obj["symbols"], obj["test"], expected, false, false);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_rvalue_statement")
{
    auto obj = make_node();
    obj["test"] = LOAD_JSON_FROM_STRING(
        "{\n            \"left\" : [[{\n                  \"left\" : {\n  "
        "     "
        "             \"node\" : \"number_literal\",\n                    "
        "\"root\" : 5\n                  },\n                  \"node\" : "
        "\"relation_expression\",\n                  \"right\" : {\n      "
        "     "
        "         \"left\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 5\n        "
        "     "
        "       },\n                    \"node\" : "
        "\"relation_expression\",\n  "
        "                  \"right\" : {\n                      \"left\" "
        ": {\n "
        "                       \"left\" : {\n                          "
        "\"node\" : \"lvalue\",\n                          \"root\" : "
        "\"exp\"\n                        },\n                        "
        "\"node\" "
        ": \"function_expression\",\n                        \"right\" : "
        "[{\n  "
        "                          \"node\" : \"number_literal\",\n       "
        "     "
        "                \"root\" : 2\n                          }, {\n   "
        "     "
        "                    \"node\" : \"number_literal\",\n             "
        "     "
        "          \"root\" : 5\n                          }],\n          "
        "     "
        "         \"root\" : \"exp\"\n                      },\n          "
        "     "
        "       \"node\" : \"relation_expression\",\n                     "
        " "
        "\"right\" : {\n                        \"left\" : {\n            "
        "     "
        "         \"left\" : {\n                            \"node\" : "
        "\"number_literal\",\n                            \"root\" : 4\n  "
        "     "
        "                   },\n                          \"node\" : "
        "\"unary_expression\",\n                          \"root\" : "
        "[\"~\"]\n "
        "                       },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : "
        "{\n     "
        "                     \"node\" : \"number_literal\",\n            "
        "     "
        "         \"root\" : 2\n                        },\n              "
        "     "
        "     \"root\" : [\"^\"]\n                      },\n              "
        "     "
        "   \"root\" : [\"/\"]\n                    },\n                  "
        "  "
        "\"root\" : [\"+\"]\n                  },\n                  "
        "\"root\" "
        ": [\"*\"]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }");
    obj["nested_binary"] = LOAD_JSON_FROM_STRING(
        "{\n            \"left\" : [[{\n                  \"left\" : {\n  "
        "     "
        "             \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"y\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 3\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }], [{\n                  "
        "\"left\" : "
        "{\n                    \"node\" : \"lvalue\",\n                  "
        "  "
        "\"root\" : \"x\"\n                  },\n                  "
        "\"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"left\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n  "
        "     "
        "                 \"left\" : {\n                          "
        "\"node\" : "
        "\"lvalue\",\n                          \"root\" : \"y\"\n        "
        "     "
        "           },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : "
        "{\n     "
        "                     \"node\" : \"number_literal\",\n            "
        "     "
        "         \"root\" : 3\n                        },\n              "
        "     "
        "     \"root\" : [\"==\"]\n                      }\n              "
        "     "
        " },\n                    \"node\" : \"relation_expression\",\n   "
        "     "
        "            \"right\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n  "
        "     "
        "                 \"left\" : {\n                          "
        "\"node\" : "
        "\"lvalue\",\n                          \"root\" : \"y\"\n        "
        "     "
        "           },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : "
        "{\n     "
        "                     \"node\" : \"number_literal\",\n            "
        "     "
        "         \"root\" : 2\n                        },\n              "
        "     "
        "     \"root\" : [\">\"]\n                      }\n               "
        "     "
        "},\n                    \"root\" : [\"&&\"]\n                  "
        "},\n   "
        "               \"root\" : [\"=\", null]\n                }]],\n  "
        "     "
        "     \"node\" : \"statement\",\n            \"root\" : "
        "\"rvalue\"\n   "
        "       }");
    obj["nested_or"] = LOAD_JSON_FROM_STRING(
        "{\n            \"left\" : [[{\n                  \"left\" : {\n  "
        "     "
        "             \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"y\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 3\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }], [{\n                  "
        "\"left\" : "
        "{\n                    \"node\" : \"lvalue\",\n                  "
        "  "
        "\"root\" : \"x\"\n                  },\n                  "
        "\"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"left\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 1\n        "
        "     "
        "       },\n                    \"node\" : "
        "\"relation_expression\",\n  "
        "                  \"right\" : {\n                      \"left\" "
        ": {\n "
        "                       \"node\" : \"number_literal\",\n          "
        "     "
        "         \"root\" : 2\n                      },\n                "
        "     "
        " \"node\" : \"relation_expression\",\n                      "
        "\"right\" "
        ": {\n                        \"node\" : \"number_literal\",\n    "
        "     "
        "               \"root\" : 3\n                      },\n          "
        "     "
        "       \"root\" : [\"||\"]\n                    },\n             "
        "     "
        "  \"root\" : [\"||\"]\n                  },\n                  "
        "\"root\" : [\"=\", null]\n                }]],\n            "
        "\"node\" "
        ": \"statement\",\n            \"root\" : \"rvalue\"\n          "
        "}");
    obj["complex_or"] = LOAD_JSON_FROM_STRING(
        "{\n            \"left\" : [[{\n                  \"left\" : {\n  "
        "     "
        "             \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"y\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 3\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }], [{\n                  "
        "\"left\" : "
        "{\n                    \"node\" : \"lvalue\",\n                  "
        "  "
        "\"root\" : \"x\"\n                  },\n                  "
        "\"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"left\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 1\n        "
        "     "
        "       },\n                    \"node\" : "
        "\"relation_expression\",\n  "
        "                  \"right\" : {\n                      \"left\" "
        ": {\n "
        "                       \"node\" : \"number_literal\",\n          "
        "     "
        "         \"root\" : 1\n                      },\n                "
        "     "
        " \"node\" : \"relation_expression\",\n                      "
        "\"right\" "
        ": {\n                        \"left\" : {\n                      "
        "    "
        "\"node\" : \"number_literal\",\n                          "
        "\"root\" : "
        "2\n                        },\n                        \"node\" "
        ": "
        "\"relation_expression\",\n                        \"right\" : "
        "{\n     "
        "                     \"left\" : {\n                            "
        "\"node\" : \"number_literal\",\n                            "
        "\"root\" "
        ": 2\n                          },\n                          "
        "\"node\" "
        ": \"relation_expression\",\n                          \"right\" "
        ": {\n "
        "                           \"left\" : {\n                        "
        "     "
        " \"node\" : \"number_literal\",\n                              "
        "\"root\" : 3\n                            },\n                   "
        "     "
        "    \"node\" : \"relation_expression\",\n                        "
        "    "
        "\"right\" : {\n                              \"node\" : "
        "\"number_literal\",\n                              \"root\" : "
        "3\n     "
        "                       },\n                            \"root\" "
        ": "
        "[\"+\"]\n                          },\n                          "
        "\"root\" : [\"||\"]\n                        },\n                "
        "     "
        "   \"root\" : [\"+\"]\n                      },\n                "
        "     "
        " \"root\" : [\"||\"]\n                    },\n                   "
        " "
        "\"root\" : [\"+\"]\n                  },\n                  "
        "\"root\" "
        ": [\"=\", null]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n}");
    obj["or_with_call"] = LOAD_JSON_FROM_STRING(
        "{\n            \"left\" : [[{\n                  \"left\" : {\n  "
        "     "
        "             \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"y\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 3\n                  },\n                  \"root\" : "
        "[\"=\", null]\n                }], [{\n                  "
        "\"left\" : "
        "{\n                    \"node\" : \"lvalue\",\n                  "
        "  "
        "\"root\" : \"putchar\"\n                  },\n                  "
        "\"node\" : \"function_expression\",\n                  \"right\" "
        ": "
        "[{\n                      \"node\" : \"number_literal\",\n       "
        "     "
        "          \"root\" : 5\n                    }],\n                "
        "  "
        "\"root\" : \"putchar\"\n                }], [{\n                 "
        " "
        "\"left\" : {\n                    \"node\" : \"lvalue\",\n       "
        "     "
        "        \"root\" : \"x\"\n                  },\n                 "
        " "
        "\"node\" : \"assignment_expression\",\n                  "
        "\"right\" : "
        "{\n                    \"left\" : {\n                      "
        "\"node\" : "
        "\"number_literal\",\n                      \"root\" : 1\n        "
        "     "
        "       },\n                    \"node\" : "
        "\"relation_expression\",\n  "
        "                  \"right\" : {\n                      \"left\" "
        ": {\n "
        "                       \"node\" : \"number_literal\",\n          "
        "     "
        "         \"root\" : 1\n                      },\n                "
        "     "
        " \"node\" : \"relation_expression\",\n                      "
        "\"right\" "
        ": {\n                        \"left\" : {\n                      "
        "    "
        "\"left\" : {\n                            \"node\" : "
        "\"lvalue\",\n    "
        "                        \"root\" : \"getchar\"\n                 "
        "     "
        "    },\n                          \"node\" : "
        "\"function_expression\",\n                          \"right\" : "
        "[{\n  "
        "                            \"node\" : \"number_literal\",\n     "
        "     "
        "                    \"root\" : 1\n                            "
        "}],\n   "
        "                       \"root\" : \"getchar\"\n                  "
        "     "
        " },\n                        \"node\" : "
        "\"relation_expression\",\n    "
        "                    \"right\" : {\n                          "
        "\"left\" "
        ": {\n                            \"node\" : "
        "\"number_literal\",\n     "
        "                       \"root\" : 3\n                          "
        "},\n   "
        "                       \"node\" : \"relation_expression\",\n     "
        "     "
        "                \"right\" : {\n                            "
        "\"node\" : "
        "\"number_literal\",\n                            \"root\" : 3\n  "
        "     "
        "                   },\n                          \"root\" : "
        "[\"+\"]\n "
        "                       },\n                        \"root\" : "
        "[\"||\"]\n                      },\n                      "
        "\"root\" : "
        "[\"||\"]\n                    },\n                    \"root\" : "
        "[\"+\"]\n                  },\n                  \"root\" : "
        "[\"=\", "
        "null]\n                }]],\n            \"node\" : "
        "\"statement\",\n  "
        "          \"root\" : \"rvalue\"\n          }");

    std::deque<std::string> symbols = {
        "x", "putchar", "getchar", "double", "exp", "puts", "y"
    };

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
    auto obj = make_node();
    using namespace credence;
    obj["test"] = LOAD_JSON_FROM_STRING(
        "{\n  \"left\" : [{\n      \"left\" : {\n        \"node\" : "
        "\"number_literal\",\n        \"root\" : 50\n      },\n      "
        "\"node\" "
        ": \"vector_lvalue\",\n      \"root\" : \"x\"\n    }, {\n      "
        "\"left\" : {\n        \"node\" : \"lvalue\",\n        \"root\" : "
        "\"y\"\n      },\n      \"node\" : \"indirect_lvalue\",\n      "
        "\"root\" : [\"*\"]\n    }, {\n      \"node\" : \"lvalue\",\n     "
        " "
        "\"root\" : \"z\"\n    }],\n  \"node\" : \"statement\",\n  "
        "\"root\" : "
        "\"auto\"\n}");

    credence::ir::Instructions instructions{};

    ita_.build_from_auto_statement(obj["test"], instructions);

    CHECK(ita_.symbols_.table_.size() == 3);
    CHECK(ita_.symbols_.table_.contains("x") == true);
    CHECK(ita_.symbols_.table_.contains("y") == true);
    CHECK(ita_.symbols_.table_.contains("z") == true);

    value::Literal empty_value =
        std::make_pair(std::monostate(), value::TYPE_LITERAL.at("null"));
    value::Literal word_value =
        std::make_pair("__WORD__", value::TYPE_LITERAL.at("word"));
    value::Literal byte_value = std::make_pair(
        static_cast<unsigned char>('0'), std::make_pair("byte", 50));

    CHECK(ita_.symbols_.table_["x"] == byte_value);
    CHECK(ita_.symbols_.table_["y"] == word_value);
    CHECK(ita_.symbols_.table_["z"] == empty_value);

    REQUIRE(std::get<1>(instructions[0]) == "x");
    REQUIRE(std::get<1>(instructions[1]) == "*y");
    REQUIRE(std::get<1>(instructions[2]) == "z");
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: deep-evaluated rvalue")
{
    auto obj = make_node();
    auto internal_symbols = LOAD_JSON_FROM_STRING(
        "{\n  \"arg\" : {\n    \"column\" : 6,\n    \"end_column\" : 9,\n "
        "   "
        "\"end_pos\" : 8,\n    \"line\" : 1,\n    \"start_pos\" : 5,\n    "
        "\"type\" : \"lvalue\"\n  },\n  \"exp\" : {\n    \"column\" : "
        "1,\n    "
        "\"end_column\" : 4,\n    \"end_pos\" : 52,\n    \"line\" : 6,\n  "
        "  "
        "\"start_pos\" : 49,\n    \"type\" : \"function_definition\"\n  "
        "},\n  "
        "\"main\" : {\n    \"column\" : 1,\n    \"end_column\" : 5,\n    "
        "\"end_pos\" : 4,\n    \"line\" : 1,\n    \"start_pos\" : 0,\n    "
        "\"type\" : \"function_definition\"\n  },\n  \"x\" : {\n    "
        "\"column\" "
        ": 8,\n    \"end_column\" : 9,\n    \"end_pos\" : 20,\n    "
        "\"line\" : "
        "2,\n    \"start_pos\" : 19,\n    \"type\" : \"lvalue\"\n  },\n  "
        "\"y\" "
        ": {\n    \"column\" : 7,\n    \"end_column\" : 8,\n    "
        "\"end_pos\" : "
        "56,\n    \"line\" : 6,\n    \"start_pos\" : 55,\n    \"type\" : "
        "\"lvalue\"\n  }\n}");
    obj["test"] = LOAD_JSON_FROM_STRING(
        "{\n            \"left\" : [[{\n                  \"left\" : {\n  "
        "     "
        "             \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"x\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"left\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n  "
        "     "
        "                 \"left\" : {\n                          "
        "\"node\" : "
        "\"number_literal\",\n                          \"root\" : 5\n    "
        "     "
        "               },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : "
        "{\n     "
        "                     \"node\" : \"number_literal\",\n            "
        "     "
        "         \"root\" : 5\n                        },\n              "
        "     "
        "     \"root\" : [\"+\"]\n                      }\n               "
        "     "
        "},\n                    \"node\" : \"relation_expression\",\n    "
        "     "
        "           \"right\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n  "
        "     "
        "                 \"left\" : {\n                          "
        "\"node\" : "
        "\"number_literal\",\n                          \"root\" : 6\n    "
        "     "
        "               },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : "
        "{\n     "
        "                     \"node\" : \"number_literal\",\n            "
        "     "
        "         \"root\" : 6\n                        },\n              "
        "     "
        "     \"root\" : [\"+\"]\n                      }\n               "
        "     "
        "},\n                    \"root\" : [\"*\"]\n                  "
        "},\n    "
        "              \"root\" : [\"=\", null]\n                }]],\n   "
        "     "
        "    \"node\" : \"statement\",\n            \"root\" : "
        "\"rvalue\"\n    "
        "      }");

    std::string expected = R"ita(_t1 = (5:int:4) + (5:int:4);
_t2 = (6:int:4) + (6:int:4);
_t3 = _t1 * _t2;
x = _t3;
)ita";
    TEST_RVALUE_STATEMENT_NODE_WITH(obj, { "x" }, obj["test"], expected);
}
