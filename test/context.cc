#include <doctest/doctest.h>

#include <credence/ir/context.h> // for Context
#include <credence/ir/ita.h>     // for make_ITA_instructions, ITA
#include <credence/symbol.h>     // for Symbol_Table
#include <credence/util.h>       // for AST_Node, AST
#include <format>                // for format
#include <memory>                // for allocator, shared_ptr
#include <ostream>               // for basic_ostream
#include <simplejson.h>          // for JSON, object
#include <sstream>               // for basic_ostringstream, ostringstream
#include <string>                // for basic_string, char_traits, string
#include <string_view>           // for basic_string_view
#include <tuple>                 // for get

#define EMIT(os, inst) credence::ir::ITA::emit_to(os, inst)
#define LOAD_JSON_FROM_STRING(str) credence::util::AST_Node::load(str)

inline auto make_node()
{
    return credence::util::AST::object();
}

TEST_CASE("ir/context.cc: Context::from_ast_to_symbolic_ita")
{
    auto symbols = LOAD_JSON_FROM_STRING(
        "{\"m\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 17, "
        "\"column\": 9, \"end_pos\": 18, \"end_column\": 10}, \"i\": "
        "{\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 19, \"column\": "
        "11, \"end_pos\": 20, \"end_column\": 12}, \"j\": {\"type\": "
        "\"lvalue\", \"line\": 2, \"start_pos\": 21, \"column\": 13, "
        "\"end_pos\": 22, \"end_column\": 14}, \"c\": {\"type\": \"lvalue\", "
        "\"line\": 2, \"start_pos\": 23, \"column\": 15, \"end_pos\": 24, "
        "\"end_column\": 16}, \"sign\": {\"type\": \"lvalue\", \"line\": 2, "
        "\"start_pos\": 25, \"column\": 17, \"end_pos\": 29, \"end_column\": "
        "21}, \"C\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 30, "
        "\"column\": 22, \"end_pos\": 31, \"end_column\": 23}, \"s\": "
        "{\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 32, \"column\": "
        "24, \"end_pos\": 33, \"end_column\": 25}, \"loop\": {\"type\": "
        "\"lvalue\", \"line\": 3, \"start_pos\": 43, \"column\": 9, "
        "\"end_pos\": 47, \"end_column\": 13}, \"char\": {\"type\": "
        "\"function_definition\", \"line\": 31, \"start_pos\": 774, "
        "\"column\": 1, \"end_pos\": 778, \"end_column\": 5}, \"error\": "
        "{\"type\": \"function_definition\", \"line\": 34, \"start_pos\": 789, "
        "\"column\": 1, \"end_pos\": 794, \"end_column\": 6}, \"main\": "
        "{\"type\": \"function_definition\", \"line\": 1, \"start_pos\": 0, "
        "\"column\": 1, \"end_pos\": 4, \"end_column\": 5}, \"a\": {\"type\": "
        "\"lvalue\", \"line\": 31, \"start_pos\": 779, \"column\": 6, "
        "\"end_pos\": 780, \"end_column\": 7}, \"b\": {\"type\": \"lvalue\", "
        "\"line\": 31, \"start_pos\": 781, \"column\": 8, \"end_pos\": 782, "
        "\"end_column\": 9}, \"printf\": {\"type\": \"function_definition\", "
        "\"line\": 39, \"start_pos\": 844, \"column\": 1, \"end_pos\": 850, "
        "\"end_column\": 7}}");
    auto ast = LOAD_JSON_FROM_STRING(
        "{\n  \"left\" : [{\n      \"left\" : [null],\n      \"node\" : "
        "\"function_definition\",\n      \"right\" : {\n        \"left\" : "
        "[{\n            \"left\" : [{\n                \"node\" : "
        "\"lvalue\",\n                \"root\" : \"m\"\n              }, {\n   "
        "             \"node\" : \"lvalue\",\n                \"root\" : "
        "\"i\"\n              }, {\n                \"node\" : \"lvalue\",\n   "
        "             \"root\" : \"j\"\n              }, {\n                "
        "\"node\" : \"lvalue\",\n                \"root\" : \"c\"\n            "
        "  }, {\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"sign\"\n              }, {\n                \"node\" : "
        "\"lvalue\",\n                \"root\" : \"C\"\n              }, {\n   "
        "             \"node\" : \"lvalue\",\n                \"root\" : "
        "\"s\"\n              }],\n            \"node\" : \"statement\",\n     "
        "       \"root\" : \"auto\"\n          }, {\n            \"left\" : "
        "[{\n                \"node\" : \"lvalue\",\n                \"root\" "
        ": \"loop\"\n              }],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"auto\"\n          }, {\n     "
        "       \"left\" : [[{\n                  \"left\" : {\n               "
        "     \"node\" : \"lvalue\",\n                    \"root\" : \"i\"\n   "
        "               },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 0\n                  },\n                  \"root\" : "
        "[\"=\"]\n                }], [{\n                  \"left\" : {\n     "
        "               \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"j\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 1\n                  },\n                  \"root\" : "
        "[\"=\"]\n                }], [{\n                  \"left\" : {\n     "
        "               \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"m\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 0\n                  },\n                  \"root\" : "
        "[\"=\"]\n                }], [{\n                  \"left\" : {\n     "
        "               \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"sign\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 0\n                  },\n                  \"root\" : "
        "[\"=\"]\n                }], [{\n                  \"left\" : {\n     "
        "               \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"loop\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n         "
        "           \"node\" : \"number_literal\",\n                    "
        "\"root\" : 1\n                  },\n                  \"root\" : "
        "[\"=\"]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }, {\n   "
        "         \"left\" : {\n              \"left\" : {\n                "
        "\"node\" : \"lvalue\",\n                \"root\" : \"loop\"\n         "
        "     },\n              \"node\" : \"relation_expression\",\n          "
        "    \"right\" : {\n                \"node\" : \"number_literal\",\n   "
        "             \"root\" : 1\n              },\n              \"root\" : "
        "[\"==\"]\n            },\n            \"node\" : \"statement\",\n     "
        "       \"right\" : [{\n                \"left\" : [{\n                "
        "    \"left\" : {\n                      \"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n       "
        "                 \"left\" : {\n                          \"node\" : "
        "\"lvalue\",\n                          \"root\" : \"C\"\n             "
        "           },\n                        \"node\" : "
        "\"assignment_expression\",\n                        \"right\" : {\n   "
        "                       \"left\" : {\n                            "
        "\"node\" : \"lvalue\",\n                            \"root\" : "
        "\"char\"\n                          },\n                          "
        "\"node\" : \"function_expression\",\n                          "
        "\"right\" : [{\n                              \"node\" : "
        "\"lvalue\",\n                              \"root\" : \"s\"\n         "
        "                   }, {\n                              \"left\" : {\n "
        "                               \"node\" : \"lvalue\",\n               "
        "                 \"root\" : \"j\"\n                              },\n "
        "                             \"node\" : \"pre_inc_dec_expression\",\n "
        "                             \"root\" : [\"++\"]\n                    "
        "        }],\n                          \"root\" : \"char\"\n          "
        "              },\n                        \"root\" : [\"=\"]\n        "
        "              }\n                    },\n                    \"node\" "
        ": \"statement\",\n                    \"right\" : [{\n                "
        "        \"left\" : {\n                          \"node\" : "
        "\"constant_literal\",\n                          \"root\" : \"-\"\n   "
        "                     },\n                        \"node\" : "
        "\"statement\",\n                        \"right\" : [{\n              "
        "              \"left\" : {\n                              \"node\" : "
        "\"lvalue\",\n                              \"root\" : \"sign\"\n      "
        "                      },\n                            \"node\" : "
        "\"statement\",\n                            \"right\" : [{\n          "
        "                      \"left\" : [{\n                                 "
        "   \"left\" : [[{\n                                          \"left\" "
        ": {\n                                            \"node\" : "
        "\"lvalue\",\n                                            \"root\" : "
        "\"loop\"\n                                          },\n              "
        "                            \"node\" : \"assignment_expression\",\n   "
        "                                       \"right\" : {\n                "
        "                            \"node\" : \"number_literal\",\n          "
        "                                  \"root\" : 0\n                      "
        "                    },\n                                          "
        "\"root\" : [\"=\"]\n                                        }], [{\n  "
        "                                        \"left\" : {\n                "
        "                            \"node\" : \"lvalue\",\n                  "
        "                          \"root\" : \"error\"\n                      "
        "                    },\n                                          "
        "\"node\" : \"function_expression\",\n                                 "
        "         \"right\" : [null],\n                                        "
        "  \"root\" : \"error\"\n                                        "
        "}]],\n                                    \"node\" : \"statement\",\n "
        "                                   \"root\" : \"rvalue\"\n            "
        "                      }],\n                                \"node\" : "
        "\"statement\",\n                                \"root\" : "
        "\"block\"\n                              }, null],\n                  "
        "          \"root\" : \"if\"\n                          }, {\n         "
        "                   \"left\" : [[{\n                                  "
        "\"left\" : {\n                                    \"node\" : "
        "\"lvalue\",\n                                    \"root\" : \"s\"\n   "
        "                               },\n                                  "
        "\"node\" : \"assignment_expression\",\n                               "
        "   \"right\" : {\n                                    \"node\" : "
        "\"number_literal\",\n                                    \"root\" : "
        "1\n                                  },\n                             "
        "     \"root\" : [\"=\"]\n                                }]],\n       "
        "                     \"node\" : \"statement\",\n                      "
        "      \"root\" : \"rvalue\"\n                          }],\n          "
        "              \"root\" : \"case\"\n                      }, {\n       "
        "                 \"left\" : {\n                          \"node\" : "
        "\"constant_literal\",\n                          \"root\" : \"' '\"\n "
        "                       },\n                        \"node\" : "
        "\"statement\",\n                        \"right\" : [{\n              "
        "              \"node\" : \"statement\",\n                            "
        "\"root\" : \"break\"\n                          }],\n                 "
        "       \"root\" : \"case\"\n                      }, {\n              "
        "          \"left\" : {\n                          \"node\" : "
        "\"constant_literal\",\n                          \"root\" : \"0\"\n   "
        "                     },\n                        \"node\" : "
        "\"statement\",\n                        \"right\" : [],\n             "
        "           \"root\" : \"case\"\n                      }, {\n          "
        "              \"left\" : {\n                          \"node\" : "
        "\"constant_literal\",\n                          \"root\" : \",\"\n   "
        "                     },\n                        \"node\" : "
        "\"statement\",\n                        \"right\" : [{\n              "
        "              \"left\" : {\n                              \"left\" : "
        "{\n                                \"node\" : \"lvalue\",\n           "
        "                     \"root\" : \"c\"\n                              "
        "},\n                              \"node\" : "
        "\"relation_expression\",\n                              \"right\" : "
        "{\n                                \"node\" : \"constant_literal\",\n "
        "                               \"root\" : \"0\"\n                     "
        "         },\n                              \"root\" : [\"==\"]\n      "
        "                      },\n                            \"node\" : "
        "\"statement\",\n                            \"right\" : [{\n          "
        "                      \"left\" : [{\n                                 "
        "   \"node\" : \"lvalue\",\n                                    "
        "\"root\" : \"i\"\n                                  }],\n             "
        "                   \"node\" : \"statement\",\n                        "
        "        \"root\" : \"return\"\n                              }, "
        "null],\n                            \"root\" : \"if\"\n               "
        "           }],\n                        \"root\" : \"case\"\n         "
        "             }],\n                    \"root\" : \"switch\"\n         "
        "         }, {\n                    \"left\" : {\n                     "
        " \"left\" : {\n                        \"node\" : "
        "\"constant_literal\",\n                        \"root\" : \"0\"\n     "
        "                 },\n                      \"node\" : "
        "\"relation_expression\",\n                      \"right\" : {\n       "
        "                 \"left\" : {\n                          \"node\" : "
        "\"lvalue\",\n                          \"root\" : \"c\"\n             "
        "           },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : {\n     "
        "                     \"left\" : {\n                            "
        "\"node\" : \"lvalue\",\n                            \"root\" : "
        "\"c\"\n                          },\n                          "
        "\"node\" : \"relation_expression\",\n                          "
        "\"right\" : {\n                            \"node\" : "
        "\"constant_literal\",\n                            \"root\" : \"9\"\n "
        "                         },\n                          \"root\" : "
        "[\"<=\"]\n                        },\n                        "
        "\"root\" : [\"&&\"]\n                      },\n                      "
        "\"root\" : [\"<=\"]\n                    },\n                    "
        "\"node\" : \"statement\",\n                    \"right\" : [{\n       "
        "                 \"left\" : [{\n                            \"left\" "
        ": [[{\n                                  \"left\" : {\n               "
        "                     \"node\" : \"lvalue\",\n                         "
        "           \"root\" : \"m\"\n                                  },\n   "
        "                               \"node\" : "
        "\"assignment_expression\",\n                                  "
        "\"right\" : {\n                                    \"left\" : {\n     "
        "                                 \"node\" : \"number_literal\",\n     "
        "                                 \"root\" : 10\n                      "
        "              },\n                                    \"node\" : "
        "\"relation_expression\",\n                                    "
        "\"right\" : {\n                                      \"left\" : {\n   "
        "                                     \"node\" : \"lvalue\",\n         "
        "                               \"root\" : \"m\"\n                     "
        "                 },\n                                      \"node\" : "
        "\"relation_expression\",\n                                      "
        "\"right\" : {\n                                        \"left\" : {\n "
        "                                         \"node\" : \"lvalue\",\n     "
        "                                     \"root\" : \"c\"\n               "
        "                         },\n                                        "
        "\"node\" : \"relation_expression\",\n                                 "
        "       \"right\" : {\n                                          "
        "\"node\" : \"constant_literal\",\n                                    "
        "      \"root\" : \"0\"\n                                        },\n  "
        "                                      \"root\" : [\"-\"]\n            "
        "                          },\n                                      "
        "\"root\" : [\"+\"]\n                                    },\n          "
        "                          \"root\" : [\"*\"]\n                        "
        "          },\n                                  \"root\" : [\"=\"]\n  "
        "                              }]],\n                            "
        "\"node\" : \"statement\",\n                            \"root\" : "
        "\"rvalue\"\n                          }],\n                        "
        "\"node\" : \"statement\",\n                        \"root\" : "
        "\"block\"\n                      }, null],\n                    "
        "\"root\" : \"if\"\n                  }],\n                \"node\" : "
        "\"statement\",\n                \"root\" : \"block\"\n              "
        "}],\n            \"root\" : \"while\"\n          }],\n        "
        "\"node\" : \"statement\",\n        \"root\" : \"block\"\n      },\n   "
        "   \"root\" : \"main\"\n    }, {\n      \"left\" : [{\n          "
        "\"node\" : \"lvalue\",\n          \"root\" : \"a\"\n        }, {\n    "
        "      \"node\" : \"lvalue\",\n          \"root\" : \"b\"\n        "
        "}],\n      \"node\" : \"function_definition\",\n      \"right\" : {\n "
        "       \"left\" : [],\n        \"node\" : \"statement\",\n        "
        "\"root\" : \"block\"\n      },\n      \"root\" : \"char\"\n    }, {\n "
        "     \"left\" : [null],\n      \"node\" : \"function_definition\",\n  "
        "    \"right\" : {\n        \"left\" : [{\n            \"left\" : "
        "[[{\n                  \"left\" : {\n                    \"node\" : "
        "\"lvalue\",\n                    \"root\" : \"printf\"\n              "
        "    },\n                  \"node\" : \"function_expression\",\n       "
        "           \"right\" : [{\n                      \"node\" : "
        "\"string_literal\",\n                      \"root\" : \"\\\"bad "
        "syntax*n\\\"\"\n                    }],\n                  \"root\" : "
        "\"printf\"\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          }, {\n   "
        "         \"left\" : [{\n                \"left\" : {\n                "
        "  \"node\" : \"number_literal\",\n                  \"root\" : 1\n    "
        "            },\n                \"node\" : \"unary_expression\",\n    "
        "            \"root\" : [\"-\"]\n              }],\n            "
        "\"node\" : \"statement\",\n            \"root\" : \"return\"\n        "
        "  }],\n        \"node\" : \"statement\",\n        \"root\" : "
        "\"block\"\n      },\n      \"root\" : \"error\"\n    }, {\n      "
        "\"left\" : [{\n          \"node\" : \"lvalue\",\n          \"root\" : "
        "\"s\"\n        }],\n      \"node\" : \"function_definition\",\n      "
        "\"right\" : {\n        \"left\" : [{\n            \"left\" : [{\n     "
        "           \"node\" : \"lvalue\",\n                \"root\" : \"s\"\n "
        "             }],\n            \"node\" : \"statement\",\n            "
        "\"root\" : \"return\"\n          }],\n        \"node\" : "
        "\"statement\",\n        \"root\" : \"block\"\n      },\n      "
        "\"root\" : \"printf\"\n    }],\n  \"node\" : \"program\",\n  \"root\" "
        ": \"definitions\"\n}\n");

    std::string expected_switch_main_function = R"ita(__main():
 BeginFunc ;
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
_L15:
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
    std::ostringstream out_to{};
    auto symbolic_context =
        credence::ir::Context::from_ast_to_symbolic_ita(symbols, ast);
    auto context = std::move(symbolic_context.first);
    credence::ir::ITA::emit(out_to, symbolic_context.second);
    REQUIRE(out_to.str() == expected_switch_main_function);
    REQUIRE(context->functions.at("main")->address_location[0] == 2);
    REQUIRE(context->functions.at("main")->address_location[1] == 68);
    REQUIRE(context->functions.at("main")->allocation == 24);
    REQUIRE(context->functions.size() == 4);
    REQUIRE(context->functions.at("main")->labels.size() == 19);
    REQUIRE(context->functions.at("main")->locals.size() == 6);
    REQUIRE(context->symbols_.size() == 6);
    out_to.str("");
    credence::ir::ITA::emit_to(
        out_to,
        symbolic_context
            .second[context->functions.at("main")->address_location[0]]);
    REQUIRE(out_to.str() == "i = (0:int:4);\n");
    out_to.str("");
    credence::ir::ITA::emit_to(
        out_to,
        symbolic_context
            .second[context->functions.at("main")->address_location[1]]);
    REQUIRE(out_to.str() == "GOTO _L25;\n");
}

TEST_CASE("ir/context.cc: Context::get_rvalue_symbol_type_size")
{
    auto [test1_1, test1_2, test1_3] =
        credence::ir::Context::get_rvalue_symbol_type_size("(10:int:4)");
    auto [test2_1, test2_2, test2_3] =
        credence::ir::Context::get_rvalue_symbol_type_size(
            std::format("(10.005:float:{})", sizeof(float)));
    auto [test3_1, test3_2, test3_3] =
        credence::ir::Context::get_rvalue_symbol_type_size(
            std::format("(10.000000000000000005:double:{})", sizeof(double)));
    auto [test4_1, test4_2, test4_3] =
        credence::ir::Context::get_rvalue_symbol_type_size(
            std::format("('0':byte:{})", sizeof(char)));
    auto [test5_1, test5_2, test5_3] =
        credence::ir::Context::get_rvalue_symbol_type_size(
            std::format("(__WORD__:word:{})", sizeof(void*)));
    auto [test6_1, test6_2, test6_3] =
        credence::ir::Context::get_rvalue_symbol_type_size(
            std::format(
                "(\"hello this is a very long string\":string:{})",
                std::string{ "hello this is a very long string" }.size()));

    REQUIRE(test1_1 == "10");
    REQUIRE(test1_2 == "int");
    REQUIRE(test1_3 == 4UL);

    REQUIRE(test2_1 == "10.005");
    REQUIRE(test2_2 == "float");
    REQUIRE(test2_3 == sizeof(float));

    REQUIRE(test3_1 == "10.000000000000000005");
    REQUIRE(test3_2 == "double");
    REQUIRE(test3_3 == sizeof(double));

    REQUIRE(test4_1 == "'0'");
    REQUIRE(test4_2 == "byte");
    REQUIRE(test4_3 == 1UL);

    REQUIRE(test5_1 == "__WORD__");
    REQUIRE(test5_2 == "word");
    REQUIRE(test5_3 == sizeof(void*));

    REQUIRE(test6_1 == "hello this is a very long string");
    REQUIRE(test6_2 == "string");
    REQUIRE(
        test6_3 == std::string{ "hello this is a very long string" }.size());
}