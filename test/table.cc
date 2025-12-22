#include <doctest/doctest.h> // for ResultBuilder, REQUIRE, TestCase

#include <array>                // for array
#include <credence/ir/ita.h>    // for Instruction, Quadruple, emit_to, emit
#include <credence/ir/object.h> // for Object, Function, Vector
#include <credence/ir/table.h>  // for Table, emit
#include <credence/ir/types.h>  // for Type_Checker
#include <credence/map.h>       // for Ordered_Map
#include <credence/symbol.h>    // for Symbol_Table
#include <credence/types.h>     // for is_unary_expression, get_unary_operator
#include <credence/util.h>      // for AST_Node, STRINGIFY, AST
#include <cstddef>              // for size_t
#include <deque>                // for deque
#include <easyjson.h>           // for JSON, object
#include <filesystem>           // for path
#include <format>               // for format
#include <initializer_list>     // for initializer_list
#include <map>                  // for map
#include <memory>               // for allocator, shared_ptr, make_shared
#include <sstream>              // for basic_ostringstream, ostringstream
#include <string>               // for basic_string, char_traits, string
#include <string_view>          // for basic_string_view
#include <tuple>                // for get, tuple
#include <utility>              // for get

namespace fs = std::filesystem;

#define ROOT_PATH STRINGIFY(ROOT_TEST_PATH)

#define EMIT(os, inst) credence::ir::detail::emit_to(os, inst)
#define LOAD_JSON_FROM_STRING(str) credence::util::AST_Node::load(str)

#define SETUP_TABLE_FIXTURE_AND_TEST_FROM_AST(ast_path, expected)            \
    do {                                                                     \
        using namespace credence::ir;                                        \
        auto test = std::ostringstream{};                                    \
        auto fixture_path = fs::path(ROOT_PATH);                             \
        fixture_path.append("test/fixtures/ast");                            \
        auto file_path =                                                     \
            fs::path(fixture_path).append(fmt::format("{}.json", ast_path)); \
        auto fixture_content =                                               \
            easyjson::JSON::load_file(file_path.string()).to_deque();        \
        credence::ir::emit(test, fixture_content[0], fixture_content[1]);    \
        REQUIRE(test.str() == expected);                                     \
    } while (0)

struct Table_Fixture
{
    using NULL_symbols = std::deque<std::string>;
    using Node = credence::util::AST_Node;
    const Node VECTOR_SYMBOLS = LOAD_JSON_FROM_STRING(
        "{\"x\": {\"type\": \"vector_lvalue\", \"line\": 3, "
        "\"start_pos\": 39, "
        "\"column\": 8, \"end_pos\": 40, \"end_column\": 9, \"size\": "
        "50}, "
        "\"y\": {\"type\": \"indirect_lvalue\", \"line\": 3, "
        "\"start_pos\": "
        "46, \"column\": 15, \"end_pos\": 47, \"end_column\": 16}, \"z\": "
        "{\"type\": \"lvalue\", \"line\": 3, \"start_pos\": 48, "
        "\"column\": "
        "17, \"end_pos\": 49, \"end_column\": 18}, \"main\": {\"type\": "
        "\"function_definition\", \"line\": 2, \"start_pos\": 22, "
        "\"column\": "
        "1, \"end_pos\": 26, \"end_column\": 5}, \"errno\": {\"type\": "
        "\"lvalue\", \"line\": 8, \"start_pos\": 90, \"column\": 7, "
        "\"end_pos\": 95, \"end_column\": 12}, \"t\": {\"type\": "
        "\"lvalue\", "
        "\"line\": 9, \"start_pos\": 106, \"column\": 8, \"end_pos\": "
        "107, "
        "\"end_column\": 9}, \"u\": {\"type\": \"lvalue\", \"line\": 11, "
        "\"start_pos\": 138, \"column\": 9, \"end_pos\": 139, "
        "\"end_column\": "
        "10}, \"unit\": {\"type\": \"vector_definition\", \"line\": 26, "
        "\"start_pos\": 347, \"column\": 1, \"end_pos\": 351, "
        "\"end_column\": "
        "5}, \"mess\": {\"type\": \"vector_definition\", \"line\": 29, "
        "\"start_pos\": 358, \"column\": 1, \"end_pos\": 362, "
        "\"end_column\": "
        "5, \"size\": 5}, \"printf\": {\"type\": \"function_definition\", "
        "\"line\": 20, \"start_pos\": 302, \"column\": 1, \"end_pos\": "
        "308, "
        "\"end_column\": 7}, \"snide\": {\"type\": "
        "\"function_definition\", "
        "\"line\": 8, \"start_pos\": 84, \"column\": 1, \"end_pos\": 89, "
        "\"end_column\": 6}, \"s\": {\"type\": \"lvalue\", \"line\": 20, "
        "\"start_pos\": 309, \"column\": 8, \"end_pos\": 310, "
        "\"end_column\": "
        "9}, \"putchar\": {\"type\": \"vector_definition\", \"line\": 24, "
        "\"start_pos\": 330, \"column\": 1, \"end_pos\": 337, "
        "\"end_column\": "
        "8}}");
    inline auto make_node() { return credence::util::AST::object(); }
    static inline credence::ir::Table make_table(Node const& symbols)
    {
        return credence::ir::Table{ symbols };
    }
    static inline credence::ir::Table make_table_with_global_symbols(
        Node const& node,
        Node const& symbols)
    {
        using namespace credence::ir;

        auto [globals, instructions] = make_ita_instructions(symbols, node);
        auto table = Table{ symbols, instructions, globals };
        table.build_vector_definitions_from_globals();
        table.build_from_ir_instructions();
        return table;
    }
    static inline credence::ir::Table make_table_with_frame(Node const& symbols)
    {
        auto table = credence::ir::Table{ symbols };

        table.instruction_index = 1;
        table.from_func_start_ita_instruction("__main()");
        return table;
    };
};

TEST_CASE_FIXTURE(Table_Fixture, "Type Checking")
{
    // clang-format off
    // Type checking test cases from test/fixtures/types
    // each index corresponds to an ast in the fixtures
    const auto statuses = {
        false, false, false, false,
        false, false,  true,  false,
        false, true,  false, false,
        false, false, false, true,
        false, true, true, true,
        true, true
    };
    // clang-format on
    auto type_fixtures_path = fs::path(ROOT_PATH);
    type_fixtures_path.append("test/fixtures/types/ast");
    for (std::size_t i = 1; i <= statuses.size(); i++) {
        auto* status = statuses.begin() + i - 1;
        auto file_path =
            fs::path(type_fixtures_path).append(std::format("{}.json", i));
        auto path_contents =
            easyjson::JSON::load_file(file_path.string()).to_deque();
        if (*status) {
            try {
                make_table_with_global_symbols(
                    path_contents[1], path_contents[0]);
            } catch (...) {
                FAIL(std::format(
                    "FAIL: Expected '{}.json' to NOT throw exception", i));
            }
        } else
            try {
                make_table_with_global_symbols(
                    path_contents[1], path_contents[0]);
                FAIL(std::format(
                    "FAIL: Expected '{}.json' to throw exception", i));
            } catch (...) {
                // expected
            }
    }
}

TEST_CASE_FIXTURE(Table_Fixture, "Integration")
{
    auto symbols = LOAD_JSON_FROM_STRING(
        "{\"s\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 35, "
        "\"column\": 9, \"end_pos\": 36, \"end_column\": 10}, \"v\": "
        "{\"type\": \"vector_lvalue\", \"line\": 2, \"start_pos\": 37, "
        "\"column\": 11, \"end_pos\": 38, \"end_column\": 12}, \"m\": "
        "{\"type\": \"lvalue\", \"line\": 3, \"start_pos\": 50, "
        "\"column\": 9, "
        "\"end_pos\": 51, \"end_column\": 10}, \"i\": {\"type\": "
        "\"lvalue\", "
        "\"line\": 3, \"start_pos\": 52, \"column\": 11, \"end_pos\": 53, "
        "\"end_column\": 12}, \"j\": {\"type\": \"lvalue\", \"line\": 3, "
        "\"start_pos\": 54, \"column\": 13, \"end_pos\": 55, "
        "\"end_column\": "
        "14}, \"c\": {\"type\": \"lvalue\", \"line\": 3, \"start_pos\": "
        "56, "
        "\"column\": 15, \"end_pos\": 57, \"end_column\": 16}, \"sign\": "
        "{\"type\": \"lvalue\", \"line\": 3, \"start_pos\": 58, "
        "\"column\": "
        "17, \"end_pos\": 62, \"end_column\": 21}, \"C\": {\"type\": "
        "\"lvalue\", \"line\": 3, \"start_pos\": 63, \"column\": 22, "
        "\"end_pos\": 64, \"end_column\": 23}, \"loop\": {\"type\": "
        "\"lvalue\", \"line\": 4, \"start_pos\": 74, \"column\": 9, "
        "\"end_pos\": 78, \"end_column\": 13}, \"char\": {\"type\": "
        "\"function_definition\", \"line\": 32, \"start_pos\": 769, "
        "\"column\": 1, \"end_pos\": 773, \"end_column\": 5}, \"error\": "
        "{\"type\": \"function_definition\", \"line\": 35, \"start_pos\": "
        "784, "
        "\"column\": 1, \"end_pos\": 789, \"end_column\": 6}, "
        "\"convert\": "
        "{\"type\": \"function_definition\", \"line\": 2, \"start_pos\": "
        "27, "
        "\"column\": 1, \"end_pos\": 34, \"end_column\": 8}, \"a\": "
        "{\"type\": "
        "\"lvalue\", \"line\": 32, \"start_pos\": 774, \"column\": 6, "
        "\"end_pos\": 775, \"end_column\": 7}, \"b\": {\"type\": "
        "\"lvalue\", "
        "\"line\": 32, \"start_pos\": 776, \"column\": 8, \"end_pos\": "
        "777, "
        "\"end_column\": 9}, \"printf\": {\"type\": "
        "\"function_definition\", "
        "\"line\": 41, \"start_pos\": 850, \"column\": 1, \"end_pos\": "
        "856, "
        "\"end_column\": 7}}");
    auto vector_2 = LOAD_JSON_FROM_STRING(
        "{\n  \"left\" : [{\n      \"left\" : [null],\n      \"node\" : "
        "\"function_definition\",\n      \"right\" : {\n        \"left\" "
        ": "
        "[{\n            \"left\" : [{\n                \"left\" : {\n    "
        "     "
        "         \"node\" : \"integer_literal\",\n                  "
        "\"root\" : "
        "50\n                },\n                \"node\" : "
        "\"vector_lvalue\",\n                \"root\" : \"x\"\n           "
        "   "
        "}, {\n                \"left\" : {\n                  \"node\" : "
        "\"lvalue\",\n                  \"root\" : \"y\"\n                "
        "},\n "
        "               \"node\" : \"indirect_lvalue\",\n                "
        "\"root\" : [\"*\"]\n              }, {\n                \"node\" "
        ": "
        "\"lvalue\",\n                \"root\" : \"z\"\n              "
        "}],\n    "
        "        \"node\" : \"statement\",\n            \"root\" : "
        "\"auto\"\n  "
        "        }, {\n            \"left\" : [{\n                "
        "\"node\" : "
        "\"lvalue\",\n                \"root\" : \"putchar\"\n            "
        "  "
        "}],\n            \"node\" : \"statement\",\n            \"root\" "
        ": "
        "\"extrn\"\n          }, {\n            \"left\" : [[{\n          "
        "     "
        "   \"left\" : {\n                    \"left\" : {\n              "
        "     "
        "   \"node\" : \"integer_literal\",\n                      "
        "\"root\" : "
        "49\n                    },\n                    \"node\" : "
        "\"vector_lvalue\",\n                    \"root\" : \"x\"\n       "
        "     "
        "      },\n                  \"node\" : "
        "\"assignment_expression\",\n   "
        "               \"right\" : {\n                    \"node\" : "
        "\"integer_literal\",\n                    \"root\" : 0\n          "
        "     "
        "   },\n                  \"root\" : [\"=\"]\n                "
        "}]],\n   "
        "         \"node\" : \"statement\",\n            \"root\" : "
        "\"rvalue\"\n          }],\n        \"node\" : \"statement\",\n   "
        "     "
        "\"root\" : \"block\"\n      },\n      \"root\" : \"main\"\n    "
        "}, {\n "
        "     \"left\" : [{\n          \"node\" : \"lvalue\",\n          "
        "\"root\" : \"errno\"\n        }],\n      \"node\" : "
        "\"function_definition\",\n      \"right\" : {\n        \"left\" "
        ": "
        "[{\n            \"left\" : [{\n                \"node\" : "
        "\"lvalue\",\n                \"root\" : \"t\"\n              "
        "}],\n    "
        "        \"node\" : \"statement\",\n            \"root\" : "
        "\"auto\"\n  "
        "        }, {\n            \"left\" : [{\n                "
        "\"node\" : "
        "\"lvalue\",\n                \"root\" : \"unit\"\n              "
        "}, "
        "{\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : "
        "\"mess\"\n              }],\n            \"node\" : "
        "\"statement\",\n  "
        "          \"root\" : \"extrn\"\n          }, {\n            "
        "\"left\" "
        ": [{\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"u\"\n              }],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"auto\"\n          }, "
        "{\n     "
        "       \"left\" : [[{\n                  \"left\" : {\n          "
        "     "
        "     \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"u\"\n   "
        "               },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"lvalue\",\n                    \"root\" "
        ": "
        "\"unit\"\n                  },\n                  \"root\" : "
        "[\"=\"]\n                }], [{\n                  \"left\" : "
        "{\n     "
        "               \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"unit\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"integer_literal\",\n                    "
        "\"root\" : 1\n                  },\n                  \"root\" : "
        "[\"=\"]\n                }], [{\n                  \"left\" : "
        "{\n     "
        "               \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"t\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"left\" : {\n                      \"node\" : "
        "\"lvalue\",\n                      \"root\" : \"errno\"\n        "
        "     "
        "       },\n                    \"node\" : \"vector_lvalue\",\n   "
        "     "
        "            \"root\" : \"mess\"\n                  },\n          "
        "     "
        "   \"root\" : [\"=\"]\n                }], [{\n                  "
        "\"left\" : {\n                    \"node\" : \"lvalue\",\n       "
        "     "
        "        \"root\" : \"printf\"\n                  },\n            "
        "     "
        " \"node\" : \"function_expression\",\n                  "
        "\"right\" : "
        "[{\n                      \"node\" : \"string_literal\",\n       "
        "     "
        "          \"root\" : \"\\\"error number %d, "
        "%s*n'*,errno,mess[errno]\\\"\"\n                    }],\n        "
        "     "
        "     \"root\" : \"printf\"\n                }], [{\n             "
        "     "
        "\"left\" : {\n                    \"node\" : \"lvalue\",\n       "
        "     "
        "        \"root\" : \"unit\"\n                  },\n              "
        "    "
        "\"node\" : \"assignment_expression\",\n                  "
        "\"right\" : "
        "{\n                    \"node\" : \"lvalue\",\n                  "
        "  "
        "\"root\" : \"u\"\n                  },\n                  "
        "\"root\" : "
        "[\"=\"]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          "
        "}],\n    "
        "    \"node\" : \"statement\",\n        \"root\" : \"block\"\n    "
        "  "
        "},\n      \"root\" : \"snide\"\n    }, {\n      \"left\" : [{\n  "
        "     "
        "   \"node\" : \"lvalue\",\n          \"root\" : \"s\"\n        "
        "}],\n  "
        "    \"node\" : \"function_definition\",\n      \"right\" : {\n   "
        "     "
        "\"left\" : [{\n            \"left\" : [{\n                "
        "\"node\" : "
        "\"lvalue\",\n                \"root\" : \"s\"\n              "
        "}],\n    "
        "        \"node\" : \"statement\",\n            \"root\" : "
        "\"return\"\n          }],\n        \"node\" : \"statement\",\n   "
        "     "
        "\"root\" : \"block\"\n      },\n      \"root\" : \"printf\"\n    "
        "}, "
        "{\n      \"node\" : \"vector_definition\",\n      \"right\" : "
        "[{\n    "
        "      \"node\" : \"string_literal\",\n          \"root\" : "
        "\"\\\"puts\\\"\"\n        }],\n      \"root\" : \"putchar\"\n    "
        "}, "
        "{\n      \"node\" : \"vector_definition\",\n      \"right\" : "
        "[{\n    "
        "      \"node\" : \"integer_literal\",\n          \"root\" : 10\n  "
        "     "
        " }],\n      \"root\" : \"unit\"\n    }, {\n      \"left\" : {\n  "
        "     "
        " \"node\" : \"integer_literal\",\n        \"root\" : 6\n      "
        "},\n     "
        " \"node\" : \"vector_definition\",\n      \"right\" : [{\n       "
        "   "
        "\"node\" : \"string_literal\",\n          \"root\" : \"\\\"too "
        "bad\\\"\"\n        }, {\n          \"node\" : "
        "\"string_literal\",\n   "
        "       \"root\" : \"\\\"tough luck\\\"\"\n        }, {\n         "
        " "
        "\"node\" : \"string_literal\",\n          \"root\" : "
        "\"\\\"sorry, "
        "Charlie\\\"\"\n        }, {\n          \"node\" : "
        "\"string_literal\",\n          \"root\" : \"\\\"that's the "
        "breaks\\\"\"\n        }, {\n          \"node\" : "
        "\"string_literal\",\n          \"root\" : \"\\\"what a "
        "shame\\\"\"\n  "
        "      }, {\n          \"node\" : \"string_literal\",\n          "
        "\"root\" : \"\\\"some days you can't win\\\"\"\n        }],\n    "
        "  "
        "\"root\" : \"mess\"\n    }],\n  \"node\" : \"program\",\n  "
        "\"root\" : "
        "\"definitions\"\n}\n");
    auto bitwise_constant = LOAD_JSON_FROM_STRING(
        "{\n  \"left\" : [{\n      \"left\" : [null],\n      \"node\" : "
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
        "     "
        "}, {\n            \"left\" : [[{\n                  \"left\" : "
        "{\n    "
        "                \"node\" : \"lvalue\",\n                    "
        "\"root\" "
        ": \"x\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"left\" : {\n                      \"node\" : "
        "\"integer_literal\",\n                      \"root\" : 10\n       "
        "     "
        "        },\n                    \"node\" : "
        "\"unary_expression\",\n    "
        "                \"root\" : [\"~\"]\n                  },\n       "
        "     "
        "      \"root\" : [\"=\"]\n                }], [{\n               "
        "   "
        "\"left\" : {\n                    \"node\" : \"lvalue\",\n       "
        "     "
        "        \"root\" : \"y\"\n                  },\n                 "
        " "
        "\"node\" : \"assignment_expression\",\n                  "
        "\"right\" : "
        "{\n                    \"left\" : {\n                      "
        "\"node\" : "
        "\"evaluated_expression\",\n                      \"root\" : {\n  "
        "     "
        "                 \"left\" : {\n                          "
        "\"node\" : "
        "\"integer_literal\",\n                          \"root\" : 10\n   "
        "     "
        "                },\n                        \"node\" : "
        "\"relation_expression\",\n                        \"right\" : "
        "{\n     "
        "                     \"node\" : \"lvalue\",\n                    "
        "     "
        " \"root\" : \"x\"\n                        },\n                  "
        "     "
        " \"root\" : [\"^\"]\n                      }\n                   "
        " "
        "},\n                    \"node\" : \"relation_expression\",\n    "
        "     "
        "           \"right\" : {\n                      \"left\" : {\n   "
        "     "
        "                \"node\" : \"integer_literal\",\n                 "
        "     "
        "  \"root\" : 2\n                      },\n                      "
        "\"node\" : \"relation_expression\",\n                      "
        "\"right\" "
        ": {\n                        \"node\" : \"integer_literal\",\n    "
        "     "
        "               \"root\" : 1\n                      },\n          "
        "     "
        "       \"root\" : [\">>\"]\n                    },\n             "
        "     "
        "  \"root\" : [\"|\"]\n                  },\n                  "
        "\"root\" : [\"=\"]\n                }], [{\n                  "
        "\"left\" : {\n                    \"node\" : \"lvalue\",\n       "
        "     "
        "        \"root\" : \"z\"\n                  },\n                 "
        " "
        "\"node\" : \"assignment_expression\",\n                  "
        "\"right\" : "
        "{\n                    \"left\" : {\n                      "
        "\"left\" : "
        "{\n                        \"node\" : \"lvalue\",\n              "
        "     "
        "     \"root\" : \"x\"\n                      },\n                "
        "     "
        " \"node\" : \"unary_expression\",\n                      "
        "\"root\" : "
        "[\"~\"]\n                    },\n                    \"node\" : "
        "\"relation_expression\",\n                    \"right\" : {\n    "
        "     "
        "             \"left\" : {\n                        \"node\" : "
        "\"lvalue\",\n                        \"root\" : \"y\"\n          "
        "     "
        "       },\n                      \"node\" : "
        "\"unary_expression\",\n   "
        "                   \"root\" : [\"~\"]\n                    },\n  "
        "     "
        "             \"root\" : [\"&\"]\n                  },\n          "
        "     "
        "   \"root\" : [\"=\"]\n                }]],\n            "
        "\"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          "
        "}],\n    "
        "    \"node\" : \"statement\",\n        \"root\" : \"block\"\n    "
        "  "
        "},\n      \"root\" : \"main\"\n    }],\n  \"node\" : "
        "\"program\",\n  "
        "\"root\" : \"definitions\"\n}\n");
    auto switch_main_function = LOAD_JSON_FROM_STRING(
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
        "           \"node\" : \"integer_literal\",\n                    "
        "\"root\" : 0\n                  },\n                  \"root\" : "
        "[\"=\"]\n                }], [{\n                  \"left\" : "
        "{\n     "
        "               \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"j\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"integer_literal\",\n                    "
        "\"root\" : 1\n                  },\n                  \"root\" : "
        "[\"=\"]\n                }], [{\n                  \"left\" : "
        "{\n     "
        "               \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"m\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"integer_literal\",\n                    "
        "\"root\" : 0\n                  },\n                  \"root\" : "
        "[\"=\"]\n                }], [{\n                  \"left\" : "
        "{\n     "
        "               \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"sign\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"integer_literal\",\n                    "
        "\"root\" : 0\n                  },\n                  \"root\" : "
        "[\"=\"]\n                }], [{\n                  \"left\" : "
        "{\n     "
        "               \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"loop\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"integer_literal\",\n                    "
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
        "\"integer_literal\",\n   "
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
        "                            \"node\" : \"integer_literal\",\n     "
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
        "\"integer_literal\",\n                                    "
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
        "\"integer_literal\",\n     "
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
        "  \"node\" : \"integer_literal\",\n                  \"root\" : "
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

    std::string expected_bitwise_constant = R"ita(__main():
 BeginFunc ;
    LOCL x;
    LOCL y;
    LOCL z;
    _t2 = ~ (10:int:4);
    x = _t2;
    _t3 = (10:int:4) ^ x;
    _t4 = (2:int:4) >> (1:int:4);
    _t5 = _t3 | _t4;
    y = _t5;
    _t6 = ~ x;
    _t7 = ~ y;
    _t8 = _t6 & _t7;
    z = _t8;
_L1:
    LEAVE;
 EndFunc ;

)ita";

    std::string expected_switch_main_function = R"ita(__main():
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
    auto expected_2 = R"ita(__main():
 BeginFunc ;
    LOCL x;
    LOCL *y;
    LOCL z;
    GLOBL putchar;
    x[49] = (0:int:4);
_L1:
    LEAVE;
 EndFunc ;


__snide(errno):
 BeginFunc ;
    LOCL t;
    GLOBL unit;
    GLOBL mess;
    LOCL u;
    u = unit;
    unit = (1:int:4);
    t = mess[errno];
    _p1 = ("error number %d, %s*n'*,errno,mess[errno]":string:41);
    PUSH _p1;
    CALL printf;
    POP 8;
    _t2 = RET;
    unit = u;
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
    auto table_pointer =
        make_table_with_global_symbols(switch_main_function, symbols);
    auto table = table_pointer.get_table_object();
    auto instructions = *table_pointer.get_table_instructions();
    credence::ir::detail::emit(out_to, instructions);
    REQUIRE(out_to.str() == expected_switch_main_function);
    REQUIRE(table->functions.at("main")->address_location[0] == 2);
    REQUIRE(table->functions.at("main")->address_location[1] == 76);
    REQUIRE(table->functions.at("main")->allocation == 32);
    REQUIRE(table->functions.size() == 4);
    REQUIRE(table->functions.at("main")->labels.size() == 19);
    REQUIRE(table->functions.at("main")->locals.size() == 10);
    out_to.str("");
    credence::ir::detail::emit_to(
        out_to, instructions[table->functions.at("main")->address_location[0]]);
    REQUIRE(out_to.str() == "LOCL m;\n");
    out_to.str("");
    credence::ir::detail::emit_to(
        out_to, instructions[table->functions.at("main")->address_location[1]]);
    REQUIRE(out_to.str() == "GOTO _L25;\n");
    out_to.str("");
    credence::ir::emit(out_to, VECTOR_SYMBOLS, vector_2);
    REQUIRE(out_to.str() == expected_2);
    out_to.str("");
    credence::ir::emit(out_to, VECTOR_SYMBOLS, bitwise_constant);
    REQUIRE(out_to.str() == expected_bitwise_constant);
}

TEST_CASE_FIXTURE(Table_Fixture, "from_globl_ita_instruction")
{
    auto table_pointer = make_table_with_frame(VECTOR_SYMBOLS);
    auto table = table_pointer.get_table_object();

    table->vectors["mess"] = std::make_shared<credence::ir::object::Vector>(
        credence::ir::object::Vector{ "mess", 10 });
    REQUIRE_THROWS(table_pointer.from_globl_ita_instruction("snide"));
    REQUIRE_NOTHROW(table_pointer.from_globl_ita_instruction("mess"));
}

TEST_CASE_FIXTURE(Table_Fixture, "from_locl_ita_instruction")
{
    auto table_pointer = make_table_with_frame(VECTOR_SYMBOLS);
    auto table = table_pointer.get_table_object();
    auto& locals = table->get_stack_frame_symbols();
    auto test1 = credence::ir::Quadruple{
        credence::ir::Instruction::LOCL, "snide", "", ""
    };
    auto test2 = credence::ir::Quadruple{
        credence::ir::Instruction::LOCL, "*ptr", "", ""
    };
    REQUIRE_NOTHROW(table_pointer.from_locl_ita_instruction(test1));
    REQUIRE_NOTHROW(table_pointer.from_locl_ita_instruction(test2));
    REQUIRE(locals.table_.contains("snide"));
    REQUIRE(locals.addr_.contains("ptr"));
}

TEST_CASE_FIXTURE(Table_Fixture, "build_vector_definitions_from_globals")
{
    auto ast = LOAD_JSON_FROM_STRING(
        "{\n  \"left\" : [{\n      \"left\" : [null],\n      \"node\" : "
        "\"function_definition\",\n      \"right\" : {\n        \"left\" "
        ": "
        "[{\n            \"left\" : [{\n                \"left\" : {\n    "
        "     "
        "         \"node\" : \"integer_literal\",\n                  "
        "\"root\" : "
        "50\n                },\n                \"node\" : "
        "\"vector_lvalue\",\n                \"root\" : \"x\"\n           "
        "   "
        "}, {\n                \"left\" : {\n                  \"node\" : "
        "\"lvalue\",\n                  \"root\" : \"y\"\n                "
        "},\n "
        "               \"node\" : \"indirect_lvalue\",\n                "
        "\"root\" : [\"*\"]\n              }, {\n                \"node\" "
        ": "
        "\"lvalue\",\n                \"root\" : \"z\"\n              "
        "}],\n    "
        "        \"node\" : \"statement\",\n            \"root\" : "
        "\"auto\"\n  "
        "        }, {\n            \"left\" : [{\n                "
        "\"node\" : "
        "\"lvalue\",\n                \"root\" : \"putchar\"\n            "
        "  "
        "}],\n            \"node\" : \"statement\",\n            \"root\" "
        ": "
        "\"extrn\"\n          }, {\n            \"left\" : [[{\n          "
        "     "
        "   \"left\" : {\n                    \"left\" : {\n              "
        "     "
        "   \"node\" : \"integer_literal\",\n                      "
        "\"root\" : "
        "49\n                    },\n                    \"node\" : "
        "\"vector_lvalue\",\n                    \"root\" : \"x\"\n       "
        "     "
        "      },\n                  \"node\" : "
        "\"assignment_expression\",\n   "
        "               \"right\" : {\n                    \"node\" : "
        "\"integer_literal\",\n                    \"root\" : 0\n          "
        "     "
        "   },\n                  \"root\" : [\"=\"]\n                "
        "}]],\n   "
        "         \"node\" : \"statement\",\n            \"root\" : "
        "\"rvalue\"\n          }],\n        \"node\" : \"statement\",\n   "
        "     "
        "\"root\" : \"block\"\n      },\n      \"root\" : \"main\"\n    "
        "}, {\n "
        "     \"left\" : [{\n          \"node\" : \"lvalue\",\n          "
        "\"root\" : \"errno\"\n        }],\n      \"node\" : "
        "\"function_definition\",\n      \"right\" : {\n        \"left\" "
        ": "
        "[{\n            \"left\" : [{\n                \"node\" : "
        "\"lvalue\",\n                \"root\" : \"t\"\n              "
        "}],\n    "
        "        \"node\" : \"statement\",\n            \"root\" : "
        "\"auto\"\n  "
        "        }, {\n            \"left\" : [{\n                "
        "\"node\" : "
        "\"lvalue\",\n                \"root\" : \"unit\"\n              "
        "}, "
        "{\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : "
        "\"mess\"\n              }],\n            \"node\" : "
        "\"statement\",\n  "
        "          \"root\" : \"extrn\"\n          }, {\n            "
        "\"left\" "
        ": [{\n                \"node\" : \"lvalue\",\n                "
        "\"root\" : \"u\"\n              }],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"auto\"\n          }, "
        "{\n     "
        "       \"left\" : [[{\n                  \"left\" : {\n          "
        "     "
        "     \"node\" : \"lvalue\",\n                    \"root\" : "
        "\"u\"\n   "
        "               },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"lvalue\",\n                    \"root\" "
        ": "
        "\"unit\"\n                  },\n                  \"root\" : "
        "[\"=\"]\n                }], [{\n                  \"left\" : "
        "{\n     "
        "               \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"unit\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"node\" : \"integer_literal\",\n                    "
        "\"root\" : 1\n                  },\n                  \"root\" : "
        "[\"=\"]\n                }], [{\n                  \"left\" : "
        "{\n     "
        "               \"node\" : \"lvalue\",\n                    "
        "\"root\" : "
        "\"t\"\n                  },\n                  \"node\" : "
        "\"assignment_expression\",\n                  \"right\" : {\n    "
        "     "
        "           \"left\" : {\n                      \"node\" : "
        "\"lvalue\",\n                      \"root\" : \"errno\"\n        "
        "     "
        "       },\n                    \"node\" : \"vector_lvalue\",\n   "
        "     "
        "            \"root\" : \"mess\"\n                  },\n          "
        "     "
        "   \"root\" : [\"=\"]\n                }], [{\n                  "
        "\"left\" : {\n                    \"node\" : \"lvalue\",\n       "
        "     "
        "        \"root\" : \"printf\"\n                  },\n            "
        "     "
        " \"node\" : \"function_expression\",\n                  "
        "\"right\" : "
        "[{\n                      \"node\" : \"string_literal\",\n       "
        "     "
        "          \"root\" : \"\\\"error number %d, "
        "%s*n'*,errno,mess[errno]\\\"\"\n                    }],\n        "
        "     "
        "     \"root\" : \"printf\"\n                }], [{\n             "
        "     "
        "\"left\" : {\n                    \"node\" : \"lvalue\",\n       "
        "     "
        "        \"root\" : \"unit\"\n                  },\n              "
        "    "
        "\"node\" : \"assignment_expression\",\n                  "
        "\"right\" : "
        "{\n                    \"node\" : \"lvalue\",\n                  "
        "  "
        "\"root\" : \"u\"\n                  },\n                  "
        "\"root\" : "
        "[\"=\"]\n                }]],\n            \"node\" : "
        "\"statement\",\n            \"root\" : \"rvalue\"\n          "
        "}],\n    "
        "    \"node\" : \"statement\",\n        \"root\" : \"block\"\n    "
        "  "
        "},\n      \"root\" : \"snide\"\n    }, {\n      \"left\" : [{\n  "
        "     "
        "   \"node\" : \"lvalue\",\n          \"root\" : \"s\"\n        "
        "}],\n  "
        "    \"node\" : \"function_definition\",\n      \"right\" : {\n   "
        "     "
        "\"left\" : [{\n            \"left\" : [{\n                "
        "\"node\" : "
        "\"lvalue\",\n                \"root\" : \"s\"\n              "
        "}],\n    "
        "        \"node\" : \"statement\",\n            \"root\" : "
        "\"return\"\n          }],\n        \"node\" : \"statement\",\n   "
        "     "
        "\"root\" : \"block\"\n      },\n      \"root\" : \"printf\"\n    "
        "}, "
        "{\n      \"node\" : \"vector_definition\",\n      \"right\" : "
        "[{\n    "
        "      \"node\" : \"string_literal\",\n          \"root\" : "
        "\"\\\"puts\\\"\"\n        }],\n      \"root\" : \"putchar\"\n    "
        "}, "
        "{\n      \"node\" : \"vector_definition\",\n      \"right\" : "
        "[{\n    "
        "      \"node\" : \"integer_literal\",\n          \"root\" : 10\n  "
        "     "
        " }],\n      \"root\" : \"unit\"\n    }, {\n      \"left\" : {\n  "
        "     "
        " \"node\" : \"integer_literal\",\n        \"root\" : 6\n      "
        "},\n     "
        " \"node\" : \"vector_definition\",\n      \"right\" : [{\n       "
        "   "
        "\"node\" : \"string_literal\",\n          \"root\" : \"\\\"too "
        "bad\\\"\"\n        }, {\n          \"node\" : "
        "\"string_literal\",\n   "
        "       \"root\" : \"\\\"tough luck\\\"\"\n        }, {\n         "
        " "
        "\"node\" : \"string_literal\",\n          \"root\" : "
        "\"\\\"sorry, "
        "Charlie\\\"\"\n        }, {\n          \"node\" : "
        "\"string_literal\",\n          \"root\" : \"\\\"that's the "
        "breaks\\\"\"\n        }, {\n          \"node\" : "
        "\"string_literal\",\n          \"root\" : \"\\\"what a "
        "shame\\\"\"\n  "
        "      }, {\n          \"node\" : \"string_literal\",\n          "
        "\"root\" : \"\\\"some days you can't win\\\"\"\n        }],\n    "
        "  "
        "\"root\" : \"mess\"\n    }],\n  \"node\" : \"program\",\n  "
        "\"root\" : "
        "\"definitions\"\n}\n");
    auto table_pointer = make_table_with_global_symbols(ast, VECTOR_SYMBOLS);
    auto table = table_pointer.get_table_object();
    auto& locals = table->functions["main"]->locals;
    locals.set_symbol_by_name("mess", credence::type::NULL_RVALUE_LITERAL);
    locals.set_symbol_by_name("putchar", credence::type::NULL_RVALUE_LITERAL);
    locals.set_symbol_by_name("unit", credence::type::NULL_RVALUE_LITERAL);
    REQUIRE(table->vectors.size() == 4);
    REQUIRE(table->vectors["mess"]->data.size() == 6);
    REQUIRE(table->vectors["putchar"]->data.size() == 1);
    REQUIRE(std::get<0>(table->vectors["putchar"]->data["0"]) == "puts");
    REQUIRE(std::get<0>(table->vectors["unit"]->data["0"]) == "10");
    REQUIRE(std::get<1>(table->vectors["unit"]->data["0"]) == "int");
    REQUIRE(std::get<0>(table->vectors["mess"]->data["0"]) == "too bad");
    REQUIRE(std::get<0>(table->vectors["mess"]->data["1"]) == "tough luck");
}

TEST_CASE_FIXTURE(Table_Fixture, "from_call_ita_instruction")
{
    auto table_pointer = make_table_with_frame(VECTOR_SYMBOLS);
    auto table = table_pointer.get_table_object();
    REQUIRE_NOTHROW(table_pointer.from_call_ita_instruction("snide"));
    REQUIRE_THROWS(table_pointer.from_call_ita_instruction("invalid"));
    table->labels.emplace("invalid");
    REQUIRE_NOTHROW(table_pointer.from_call_ita_instruction("invalid"));
}

TEST_CASE_FIXTURE(Table_Fixture, "from_label_ita_instruction")
{
    auto table_pointer = make_table_with_frame(make_node());
    auto table = table_pointer.get_table_object();
    auto frame = table->get_stack_frame();
    table_pointer.instruction_index = 5;
    auto test1 = credence::ir::Quadruple{
        credence::ir::Instruction::LABEL, "_L1", "", ""
    };
    REQUIRE_NOTHROW(table_pointer.from_label_ita_instruction(test1));
    REQUIRE_THROWS(table_pointer.from_label_ita_instruction(test1));
    REQUIRE(frame->labels.contains("_L1"));
    REQUIRE(frame->label_address.addr_["_L1"] == 5UL);
}

TEST_CASE_FIXTURE(Table_Fixture, "from_mov_ita_instruction")
{
    auto table_pointer = make_table_with_frame(VECTOR_SYMBOLS);
    auto table = table_pointer.get_table_object();
    auto frame = table->get_stack_frame();
    auto test1 = credence::ir::Quadruple{
        credence::ir::Instruction::MOV, "_t1", "(5:int:4)", ""
    };
    auto test2 = credence::ir::Quadruple{
        credence::ir::Instruction::MOV, "a", "(10:int:4)", ""
    };
    auto test3 = credence::ir::Quadruple{
        credence::ir::Instruction::MOV, "z", "mess", ""
    };
    auto test4 = credence::ir::Quadruple{
        credence::ir::Instruction::MOV, "y", "*mess", ""
    };
    auto test5 = credence::ir::Quadruple{
        credence::ir::Instruction::MOV,
        "z",
        "--",
        "x",
    };
    REQUIRE_NOTHROW(table_pointer.from_mov_ita_instruction(test1));
    REQUIRE_NOTHROW(table_pointer.from_mov_ita_instruction(test2));
    REQUIRE_THROWS(table_pointer.from_mov_ita_instruction(test3));
    REQUIRE_THROWS(table_pointer.from_mov_ita_instruction(test5));
    table->functions["main"]->locals.addr_["mess"] = "NULL";
    REQUIRE_THROWS(table_pointer.from_mov_ita_instruction(test4));
    table->functions["main"]->locals.table_["y"] = { "100", "int", 4UL };
    REQUIRE_THROWS(table_pointer.from_mov_ita_instruction(test4));
    REQUIRE_THROWS(table_pointer.from_mov_ita_instruction(test5));
    table->functions["main"]->locals.table_["z"] = { "100", "int", 4UL };
    REQUIRE_NOTHROW(table_pointer.from_mov_ita_instruction(test5));
}

TEST_CASE_FIXTURE(Table_Fixture, "vector and pointer-decay boundary checks")
{
    auto table_pointer = make_table_with_frame(VECTOR_SYMBOLS);
    auto table = table_pointer.get_table_object();
    auto frame = table->get_stack_frame();
    auto type_checker = credence::ir::Type_Checker{ table, frame };
    std::string test1 = "fail[10]";
    std::string test2 = "mess[1000000]";
    std::string test3 = "mess[z]";
    std::string test4 = "mess[2]";
    std::string test5 = "mess[10]";
    std::string test7 = "z";
    REQUIRE_THROWS(type_checker.is_boundary_out_of_range(test1));
    REQUIRE_THROWS(type_checker.is_boundary_out_of_range(test2));
    REQUIRE_THROWS(type_checker.is_boundary_out_of_range(test3));
    auto size = static_cast<std::size_t>(3);
    table->vectors["mess"] = std::make_shared<credence::ir::object::Vector>(
        credence::ir::object::Vector{ "mess", size });
    table->functions["main"]->locals.table_["mess"] =
        credence::type::NULL_RVALUE_LITERAL;
    table->vectors["mess"]->data["0"] = credence::type::NULL_RVALUE_LITERAL;
    table->vectors["mess"]->data["1"] = credence::type::NULL_RVALUE_LITERAL;
    table->vectors["mess"]->data["2"] = credence::type::NULL_RVALUE_LITERAL;
    REQUIRE_THROWS(type_checker.is_boundary_out_of_range(test3));
    table->functions["main"]->locals.table_["z"] = { "5", "int", 4UL };
    REQUIRE_NOTHROW(type_checker.is_boundary_out_of_range(test3));
    REQUIRE_NOTHROW(type_checker.is_boundary_out_of_range(test4));
    REQUIRE_THROWS(type_checker.is_boundary_out_of_range(test5));

    REQUIRE_THROWS(
        table_pointer.from_pointer_or_vector_assignment(test7, test4));
    table->vectors["mess"]->data["2"] = { "5", "int", 4UL };
    REQUIRE_NOTHROW(
        table_pointer.from_pointer_or_vector_assignment(test7, test4));

    REQUIRE_THROWS(
        table_pointer.from_pointer_or_vector_assignment(test2, test7));
}

TEST_CASE_FIXTURE(Table_Fixture, "from_decay_offset")
{
    auto table = make_table(make_node());
    REQUIRE(credence::type::from_decay_offset("sidno[errno]") == "errno");
    REQUIRE(credence::type::from_decay_offset("y[39]") == "39");
}

TEST_CASE_FIXTURE(Table_Fixture, "from_lvalue_offset")
{
    auto table = make_table(make_node());
    REQUIRE(credence::type::from_lvalue_offset("sidno[errno]") == "sidno");
    REQUIRE(credence::type::from_lvalue_offset("y[39]") == "y");
}

TEST_CASE_FIXTURE(Table_Fixture, "from_func_start_ita_instruction")
{
    auto table_pointer = make_table_with_frame(make_node());
    auto table = table_pointer.get_table_object();
    auto frame = table->get_stack_frame();
    REQUIRE_THROWS(table_pointer.from_func_start_ita_instruction("__main()"));
    table_pointer.instruction_index = 10;
    table_pointer.from_func_start_ita_instruction("__convert(x,y,z)");
    REQUIRE(table->get_stack_frame() == table->functions["convert"]);
    REQUIRE(table->get_stack_frame()->parameters.size() == 3);
    REQUIRE(table->get_stack_frame()->address_location[0] == 11);
}

TEST_CASE_FIXTURE(Table_Fixture, "from_func_end_ita_instruction")
{
    auto table_pointer = make_table_with_frame(make_node());
    auto table = table_pointer.get_table_object();
    auto frame = table->get_stack_frame();
    auto& locals = table->get_stack_frame_symbols();
    table_pointer.instruction_index = 10;
    table->functions["main"]->locals.table_["x"] = { "10", "int", 4UL };
    table_pointer.instruction_index = 10;
    frame->parameters.emplace_back("x");
    table_pointer.from_func_end_ita_instruction();
    REQUIRE(table->functions["main"]->address_location[1] == 9);
    REQUIRE(locals.table_.contains("x"));
}

TEST_CASE_FIXTURE(Table_Fixture, "set_parameters_from_symbolic_label")
{
    auto table_pointer = make_table_with_frame(make_node());
    auto table = table_pointer.get_table_object();
    auto frame = table->get_stack_frame();
    frame->set_parameters_from_symbolic_label("__main(x,y,z,j)");
    REQUIRE(frame->parameters.size() == 4);
    REQUIRE(frame->parameters[0] == "x");
    REQUIRE(frame->parameters[1] == "y");
    REQUIRE(frame->parameters[2] == "z");
    REQUIRE(frame->parameters[3] == "j");
}

TEST_CASE_FIXTURE(Table_Fixture, "from_push_instruction")
{
    auto table_pointer = make_table_with_frame(make_node());
    auto table = table_pointer.get_table_object();
    auto push_instruction = credence::ir::Quadruple{
        credence::ir::Instruction::PUSH, "_p1", "", ""
    };
    auto push_instruction2 = credence::ir::Quadruple{
        credence::ir::Instruction::PUSH, "_p2", "", ""
    };
    table->functions.at("main")->temporary["_p1"] = "_p1";
    table->functions.at("main")->temporary["_p2"] = "_p2";
    table_pointer.from_push_instruction(push_instruction);
    REQUIRE(table->stack.size() == 1);
    REQUIRE(table->stack.back() == "_p1");
    table_pointer.from_push_instruction(push_instruction2);
    REQUIRE(table->stack.size() == 2);
    REQUIRE(table->stack.back() == "_p2");
}

TEST_CASE_FIXTURE(Table_Fixture, "from_rvalue_unary_expression")
{
    using std::get;
    auto table_pointer = make_table_with_frame(make_node());
    auto table = table_pointer.get_table_object();
    auto frame = table->get_stack_frame();
    table->functions["main"]->locals.table_["a"] = { "5", "int", 4UL };
    table->functions["main"]->locals.addr_["b"] = "a";
    table->functions["main"]->locals.table_["c"] = { "5", "int", 4UL };
    std::string test_rvalue = "~ 5";
    std::string test_pointer = "b";
    std::string test_pointer2 = "a";
    std::string test_pointer3 = "*b";
    REQUIRE_NOTHROW(
        table_pointer.from_rvalue_unary_expression("c", test_rvalue, "~"));
    auto test =
        table_pointer.from_rvalue_unary_expression("--a", test_rvalue, "--");
    auto test2 =
        table_pointer.from_rvalue_unary_expression("a++", test_rvalue, "++");
    auto test3 =
        table_pointer.from_rvalue_unary_expression("+a", test_rvalue, "+");
    auto test4 =
        table_pointer.from_rvalue_unary_expression("b", test_pointer2, "&");
    auto test5 = table_pointer.from_rvalue_unary_expression(
        "a", test_pointer3, test_pointer3);
    REQUIRE(get<0>(test) == "5");
    REQUIRE(get<0>(test2) == "5");
    REQUIRE(get<0>(test3) == "5");
    REQUIRE(get<0>(test4) == "b");
    REQUIRE(get<0>(test5) == "5");
}

TEST_CASE_FIXTURE(Table_Fixture, "from_rvalue_binary_expression")
{
    auto table = make_table_with_frame(make_node());
    auto test1 = credence::type::from_rvalue_binary_expression("5 || 10");
    auto test2 = credence::type::from_rvalue_binary_expression("_t1 && _t2");
    auto test3 = credence::type::from_rvalue_binary_expression("~_t1 + *_t2");
    REQUIRE(test1 == credence::type::Binary_Expression{ "5", "10", "||" });
    REQUIRE(test2 == credence::type::Binary_Expression{ "_t1", "_t2", "&&" });
    REQUIRE(test3 == credence::type::Binary_Expression{ "~_t1", "*_t2", "+" });
}

TEST_CASE_FIXTURE(Table_Fixture, "lvalue_at_temporary_object_address")
{
    auto table_pointer = make_table_with_frame(make_node());
    auto table = table_pointer.get_table_object();
    auto frame = table->get_stack_frame();
    table->get_stack_frame()->temporary["_t1"] = "100";
    table->get_stack_frame()->temporary["_t2"] = "5";
    table->get_stack_frame()->temporary["_t3"] = "_t2";
    table->get_stack_frame()->temporary["_t4"] = "_t3";
    table->get_stack_frame()->temporary["_t5"] = "10";
    table->get_stack_frame()->temporary["_t6"] = "_t4 || _t5";
    REQUIRE(table->lvalue_at_temporary_object_address("_t1", frame) == "100");
    REQUIRE(table->lvalue_at_temporary_object_address("_t4", frame) == "5");
    REQUIRE(table->lvalue_at_temporary_object_address("_t6", frame) ==
            "_t4 || _t5");
}

TEST_CASE_FIXTURE(Table_Fixture, "from_temporary_reassignment")
{
    auto table_pointer = make_table_with_frame(make_node());
    auto table = table_pointer.get_table_object();
    auto frame = table->get_stack_frame();
    auto& locals = table->get_stack_frame_symbols();
    REQUIRE_NOTHROW(table_pointer.from_temporary_reassignment("_t1", "~ _t2"));
    table->get_stack_frame()->temporary["_t1"] = "100";
    table->get_stack_frame()->temporary["_t2"] = "5";
    table_pointer.from_temporary_reassignment("_t1", "50");
    table_pointer.from_temporary_reassignment("_t2", "~ _t1");
    table_pointer.from_temporary_reassignment("_t3", "_t1");
    table_pointer.from_temporary_reassignment("_t4", "_t1 || _t2");
    locals = table->functions["main"]->locals;
    auto test = locals.get_symbol_by_name("_t1");
    auto test2 = locals.get_symbol_by_name("_t2");
    auto test3 = locals.get_symbol_by_name("_t4");
    REQUIRE(std::get<0>(test) == "50");
    REQUIRE(std::get<0>(test2) == "~ _t1");
    REQUIRE(std::get<0>(test3) == "_t1 || _t2");
}

TEST_CASE_FIXTURE(Table_Fixture, "from_scaler_symbol_assignment")
{
    auto table_pointer = make_table_with_frame(make_node());
    auto table = table_pointer.get_table_object();
    auto& locals = table->get_stack_frame_symbols();
    REQUIRE_THROWS(table_pointer.from_scaler_symbol_assignment("a", "b"));
    locals.table_["a"] = { "5", "int", 4UL };
    REQUIRE_THROWS(table_pointer.from_scaler_symbol_assignment("a", "c"));
    locals.table_["c"] = { "5", "int", 4UL };
    table_pointer.from_scaler_symbol_assignment("a", "c");
    REQUIRE(std::get<0>(locals.table_["a"]) == "5");
}

TEST_CASE_FIXTURE(Table_Fixture, "from_integral_unary_expression")
{
    auto table_pointer = make_table_with_frame(make_node());
    auto table = table_pointer.get_table_object();
    auto& locals = table->get_stack_frame_symbols();
    credence::type::Data_Type expected1 = { "5", "int", 4UL };
    credence::type::Data_Type expected2 = { "5", "long", 8UL };
    credence::type::Data_Type expected3 = { "5", "double", 8UL };
    locals.set_symbol_by_name("a", expected1);
    locals.set_symbol_by_name("b", expected2);
    locals.set_symbol_by_name("c", expected3);
    locals.set_symbol_by_name("x", { "hello world", "string", 15UL });
    auto test = table_pointer.from_integral_unary_expression("a");
    auto test2 = table_pointer.from_integral_unary_expression("b");
    auto test3 = table_pointer.from_integral_unary_expression("c");
    CHECK_THROWS(table_pointer.from_integral_unary_expression("x"));
    REQUIRE(test == expected1);
    REQUIRE(test2 == expected2);
    REQUIRE(test3 == expected3);
}

TEST_CASE_FIXTURE(Table_Fixture, "is_unary")
{
    auto table = make_table(make_node());
    auto test1 = credence::type::is_unary_expression("*k");
    auto test2 = credence::type::is_unary_expression("!x");
    auto test3 = credence::type::is_unary_expression("~1000");
    auto test4 = credence::type::is_unary_expression("&z_1");
    auto test5 = credence::type::is_unary_expression("-100");
    auto test6 = credence::type::is_unary_expression("u++");
    auto test7 = credence::type::is_unary_expression("--u");
    auto test8 = credence::type::is_unary_expression("u");
    auto test9 = credence::type::is_unary_expression("500");
    auto test10 = credence::type::is_unary_expression("k[20]");

    REQUIRE(test1 == true);
    REQUIRE(test2 == true);
    REQUIRE(test3 == true);
    REQUIRE(test4 == true);
    REQUIRE(test5 == true);
    REQUIRE(test6 == true);
    REQUIRE(test7 == true);
    REQUIRE(test8 == false);
    REQUIRE(test9 == false);
    REQUIRE(test10 == false);
}

TEST_CASE_FIXTURE(Table_Fixture, "get_unary")
{
    auto table = make_table(make_node());
    auto test1 = credence::type::get_unary_operator("*k");
    auto test2 = credence::type::get_unary_operator("!x");
    auto test3 = credence::type::get_unary_operator("~1000");
    auto test4 = credence::type::get_unary_operator("&z_1");
    auto test5 = credence::type::get_unary_operator("-100");
    auto test6 = credence::type::get_unary_operator("u++");
    auto test7 = credence::type::get_unary_operator("--u");

    REQUIRE(test1 == "*");
    REQUIRE(test2 == "!");
    REQUIRE(test3 == "~");
    REQUIRE(test4 == "&");
    REQUIRE(test5 == "-");
    REQUIRE(test6 == "++");
    REQUIRE(test7 == "--");
}

TEST_CASE_FIXTURE(Table_Fixture, "get_unary_rvalue_reference")
{
    auto table = make_table(make_node());
    auto test1 = credence::type::get_unary_rvalue_reference("*k");
    auto test2 = credence::type::get_unary_rvalue_reference("!x");
    auto test3 = credence::type::get_unary_rvalue_reference("~1000");
    auto test4 = credence::type::get_unary_rvalue_reference("&z_1");
    auto test5 = credence::type::get_unary_rvalue_reference("-100");
    auto test6 = credence::type::get_unary_rvalue_reference("u++");

    REQUIRE(test1 == "k");
    REQUIRE(test2 == "x");
    REQUIRE(test3 == "1000");
    REQUIRE(test4 == "z_1");
    REQUIRE(test5 == "100");
    REQUIRE(test6 == "u");
}

TEST_CASE_FIXTURE(Table_Fixture, "floats and doubles")
{
    std::string expected = R"ita(__main():
 BeginFunc ;
    _p1 = ("%s %d %f %d %c":string:14);
    _p2 = ("hello":string:5);
    _p3 = (5:int:4);
    _p4 = (5.55:float:4);
    _p5 = (5.55556:double:8);
    _p6 = ('120':char:1);
    PUSH _p6;
    PUSH _p5;
    PUSH _p4;
    PUSH _p3;
    PUSH _p2;
    PUSH _p1;
    CALL printf;
    POP 48;
    _t2 = RET;
_L1:
    LEAVE;
 EndFunc ;

)ita";
    SETUP_TABLE_FIXTURE_AND_TEST_FROM_AST("floats", expected);
}

TEST_CASE("get_rvalue_datatype_from_string")
{
    auto [test1_1, test1_2, test1_3] =
        credence::type::get_rvalue_datatype_from_string("(10:int:4)");
    auto [test2_1, test2_2, test2_3] =
        credence::type::get_rvalue_datatype_from_string(
            std::format("(10.005:float:{})", sizeof(float)));
    auto [test3_1, test3_2, test3_3] =
        credence::type::get_rvalue_datatype_from_string(
            std::format("(10.000000000000000005:double:{})", sizeof(double)));
    auto [test4_1, test4_2, test4_3] =
        credence::type::get_rvalue_datatype_from_string(
            std::format("('0':byte:{})", sizeof(char)));
    auto [test5_1, test5_2, test5_3] =
        credence::type::get_rvalue_datatype_from_string(
            std::format("(__WORD__:word:{})", sizeof(void*)));
    auto [test6_1, test6_2, test6_3] =
        credence::type::get_rvalue_datatype_from_string(
            std::format("(\"hello this is a very long string\":string:{})",
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