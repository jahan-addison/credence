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
        "{\n  \"errno\": {\n    \"column\": 7,\n    \"end_column\": 12,\n    "
        "\"end_pos\": 73,\n    \"line\": 7,\n    \"start_pos\": 68,\n    "
        "\"type\": \"lvalue\"\n  },\n  \"main\": {\n    \"column\": 1,\n    "
        "\"end_column\": 5,\n    \"end_pos\": 4,\n    \"line\": 1,\n    "
        "\"start_pos\": 0,\n    \"type\": \"function_definition\"\n  },\n  "
        "\"mess\": {\n    \"column\": 1,\n    \"end_column\": 5,\n    "
        "\"end_pos\": 343,\n    \"line\": 29,\n    \"size\": 6,\n    "
        "\"start_pos\": 339,\n    \"type\": \"vector_definition\"\n  },\n  "
        "\"print\": {\n    \"column\": 10,\n    \"end_column\": 7,\n    "
        "\"end_pos\": 289,\n    \"line\": 20,\n    \"start_pos\": 283,\n    "
        "\"type\": \"function_definition\",\n    \"void\": false\n  },\n  "
        "\"printf\": {\n    \"column\": 1,\n    \"end_column\": 7,\n    "
        "\"end_pos\": 289,\n    \"line\": 20,\n    \"start_pos\": 283,\n    "
        "\"type\": \"function_definition\",\n    \"void\": false\n  },\n  "
        "\"putchar\": {\n    \"column\": 1,\n    \"end_column\": 8,\n    "
        "\"end_pos\": 318,\n    \"line\": 24,\n    \"start_pos\": 311,\n    "
        "\"type\": \"vector_definition\"\n  },\n  \"s\": {\n    \"column\": "
        "8,\n    \"end_column\": 9,\n    \"end_pos\": 291,\n    \"line\": "
        "20,\n    \"start_pos\": 290,\n    \"type\": \"lvalue\"\n  },\n  "
        "\"snide\": {\n    \"column\": 1,\n    \"end_column\": 6,\n    "
        "\"end_pos\": 67,\n    \"line\": 7,\n    \"start_pos\": 62,\n    "
        "\"type\": \"function_definition\"\n  },\n  \"t\": {\n    \"column\": "
        "8,\n    \"end_column\": 9,\n    \"end_pos\": 85,\n    \"line\": 8,\n  "
        "  \"start_pos\": 84,\n    \"type\": \"lvalue\"\n  },\n  \"u\": {\n    "
        "\"column\": 9,\n    \"end_column\": 10,\n    \"end_pos\": 117,\n    "
        "\"line\": 10,\n    \"start_pos\": 116,\n    \"type\": \"lvalue\"\n  "
        "},\n  \"unit\": {\n    \"column\": 1,\n    \"end_column\": 5,\n    "
        "\"end_pos\": 332,\n    \"line\": 26,\n    \"start_pos\": 328,\n    "
        "\"type\": \"vector_definition\"\n  },\n  \"x\": {\n    \"column\": "
        "8,\n    \"end_column\": 9,\n    \"end_pos\": 18,\n    \"line\": 2,\n  "
        "  \"size\": 50,\n    \"start_pos\": 17,\n    \"type\": "
        "\"vector_lvalue\"\n  },\n  \"y\": {\n    \"column\": 15,\n    "
        "\"end_column\": 16,\n    \"end_pos\": 25,\n    \"line\": 2,\n    "
        "\"start_pos\": 24,\n    \"type\": \"indirect_lvalue\"\n  },\n  \"z\": "
        "{\n    \"column\": 17,\n    \"end_column\": 18,\n    \"end_pos\": "
        "27,\n    \"line\": 2,\n    \"start_pos\": 26,\n    \"type\": "
        "\"lvalue\"\n  }\n}");
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
        " {\n    \"left\" : [{\n        \"left\" : [null],\n        \"node\" : "
        "\"function_definition\",\n        \"right\" : {\n          \"left\" : "
        "[{\n              \"left\" : [{\n                  \"left\" : {\n     "
        "               \"node\" : \"integer_literal\",\n                    "
        "\"root\" : 50\n                  },\n                  \"node\" : "
        "\"vector_lvalue\",\n                  \"root\" : \"x\"\n              "
        "  }, {\n                  \"left\" : {\n                    \"node\" "
        ": \"lvalue\",\n                    \"root\" : \"y\"\n                 "
        " },\n                  \"node\" : \"indirect_lvalue\",\n              "
        "    \"root\" : [\"*\"]\n                }, {\n                  "
        "\"node\" : \"lvalue\",\n                  \"root\" : \"z\"\n          "
        "      }],\n              \"node\" : \"statement\",\n              "
        "\"root\" : \"auto\"\n            }, {\n              \"left\" : [{\n  "
        "                \"node\" : \"lvalue\",\n                  \"root\" : "
        "\"putchar\"\n                }],\n              \"node\" : "
        "\"statement\",\n              \"root\" : \"extrn\"\n            }, "
        "{\n              \"left\" : [[{\n                    \"left\" : {\n   "
        "                   \"left\" : {\n                        \"node\" : "
        "\"integer_literal\",\n                        \"root\" : 10\n         "
        "             },\n                      \"node\" : "
        "\"vector_lvalue\",\n                      \"root\" : \"x\"\n          "
        "          },\n                    \"node\" : "
        "\"assignment_expression\",\n                    \"right\" : {\n       "
        "               \"node\" : \"integer_literal\",\n                      "
        "\"root\" : 0\n                    },\n                    \"root\" : "
        "[\"=\"]\n                  }]],\n              \"node\" : "
        "\"statement\",\n              \"root\" : \"rvalue\"\n            "
        "}],\n          \"node\" : \"statement\",\n          \"root\" : "
        "\"block\"\n        },\n        \"root\" : \"main\"\n      }, {\n      "
        "  \"left\" : [{\n            \"node\" : \"lvalue\",\n            "
        "\"root\" : \"errno\"\n          }],\n        \"node\" : "
        "\"function_definition\",\n        \"right\" : {\n          \"left\" : "
        "[{\n              \"left\" : [{\n                  \"node\" : "
        "\"lvalue\",\n                  \"root\" : \"t\"\n                "
        "}],\n              \"node\" : \"statement\",\n              \"root\" "
        ": \"auto\"\n            }, {\n              \"left\" : [{\n           "
        "       \"node\" : \"lvalue\",\n                  \"root\" : "
        "\"unit\"\n                }, {\n                  \"node\" : "
        "\"lvalue\",\n                  \"root\" : \"mess\"\n                "
        "}],\n              \"node\" : \"statement\",\n              \"root\" "
        ": \"extrn\"\n            }, {\n              \"left\" : [{\n          "
        "        \"node\" : \"lvalue\",\n                  \"root\" : \"u\"\n  "
        "              }],\n              \"node\" : \"statement\",\n          "
        "    \"root\" : \"auto\"\n            }, {\n              \"left\" : "
        "[[{\n                    \"left\" : {\n                      \"node\" "
        ": \"lvalue\",\n                      \"root\" : \"u\"\n               "
        "     },\n                    \"node\" : \"assignment_expression\",\n  "
        "                  \"right\" : {\n                      \"node\" : "
        "\"lvalue\",\n                      \"root\" : \"unit\"\n              "
        "      },\n                    \"root\" : [\"=\"]\n                  "
        "}], [{\n                    \"left\" : {\n                      "
        "\"node\" : \"lvalue\",\n                      \"root\" : \"unit\"\n   "
        "                 },\n                    \"node\" : "
        "\"assignment_expression\",\n                    \"right\" : {\n       "
        "               \"node\" : \"integer_literal\",\n                      "
        "\"root\" : 1\n                    },\n                    \"root\" : "
        "[\"=\"]\n                  }], [{\n                    \"left\" : {\n "
        "                     \"node\" : \"lvalue\",\n                      "
        "\"root\" : \"t\"\n                    },\n                    "
        "\"node\" : \"assignment_expression\",\n                    \"right\" "
        ": {\n                      \"left\" : {\n                        "
        "\"node\" : \"lvalue\",\n                        \"root\" : "
        "\"errno\"\n                      },\n                      \"node\" : "
        "\"vector_lvalue\",\n                      \"root\" : \"mess\"\n       "
        "             },\n                    \"root\" : [\"=\"]\n             "
        "     }], [{\n                    \"left\" : {\n                      "
        "\"node\" : \"lvalue\",\n                      \"root\" : \"printf\"\n "
        "                   },\n                    \"node\" : "
        "\"function_expression\",\n                    \"right\" : [{\n        "
        "                \"node\" : \"string_literal\",\n                      "
        "  \"root\" : \"\\\"error number %d, %s*n'*,errno,mess[errno]\\\"\"\n  "
        "                    }],\n                    \"root\" : \"printf\"\n  "
        "                }], [{\n                    \"left\" : {\n            "
        "          \"node\" : \"lvalue\",\n                      \"root\" : "
        "\"unit\"\n                    },\n                    \"node\" : "
        "\"assignment_expression\",\n                    \"right\" : {\n       "
        "               \"node\" : \"lvalue\",\n                      \"root\" "
        ": \"u\"\n                    },\n                    \"root\" : "
        "[\"=\"]\n                  }]],\n              \"node\" : "
        "\"statement\",\n              \"root\" : \"rvalue\"\n            "
        "}],\n          \"node\" : \"statement\",\n          \"root\" : "
        "\"block\"\n        },\n        \"root\" : \"snide\"\n      }, {\n     "
        "   \"left\" : [{\n            \"node\" : \"lvalue\",\n            "
        "\"root\" : \"s\"\n          }],\n        \"node\" : "
        "\"function_definition\",\n        \"right\" : {\n          \"left\" : "
        "[{\n              \"left\" : [{\n                  \"node\" : "
        "\"lvalue\",\n                  \"root\" : \"s\"\n                "
        "}],\n              \"node\" : \"statement\",\n              \"root\" "
        ": \"return\"\n            }],\n          \"node\" : \"statement\",\n  "
        "        \"root\" : \"block\"\n        },\n        \"root\" : "
        "\"printf\"\n      }, {\n        \"node\" : \"vector_definition\",\n   "
        "     \"right\" : [{\n            \"node\" : \"string_literal\",\n     "
        "       \"root\" : \"\\\"puts\\\"\"\n          }],\n        \"root\" : "
        "\"putchar\"\n      }, {\n        \"node\" : \"vector_definition\",\n  "
        "      \"right\" : [{\n            \"node\" : \"integer_literal\",\n   "
        "         \"root\" : 10\n          }],\n        \"root\" : \"unit\"\n  "
        "    }, {\n        \"left\" : {\n          \"node\" : "
        "\"integer_literal\",\n          \"root\" : 6\n        },\n        "
        "\"node\" : \"vector_definition\",\n        \"right\" : [{\n           "
        " \"node\" : \"string_literal\",\n            \"root\" : \"\\\"too "
        "bad\\\"\"\n          }, {\n            \"node\" : "
        "\"string_literal\",\n            \"root\" : \"\\\"tough luck\\\"\"\n  "
        "        }, {\n            \"node\" : \"string_literal\",\n            "
        "\"root\" : \"\\\"sorry, Charlie\\\"\"\n          }, {\n            "
        "\"node\" : \"string_literal\",\n            \"root\" : \"\\\"that's "
        "the breaks\\\"\"\n          }, {\n            \"node\" : "
        "\"string_literal\",\n            \"root\" : \"\\\"what a "
        "shame\\\"\"\n          }, {\n            \"node\" : "
        "\"string_literal\",\n            \"root\" : \"\\\"some days you can't "
        "win\\\"\"\n          }],\n        \"root\" : \"mess\"\n      }],\n    "
        "\"node\" : \"program\",\n    \"root\" : \"definitions\"\n  }");
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
    LOCL x;
    _p2_1 = ("%s %f %d %g %b %c":string:17);
    _p3_2 = ("hello":string:5);
    _p4_3 = (5.55:float:4);
    _p5_4 = (10:int:4);
    _p6_5 = (3.14155:double:8);
    _p7_6 = (1:int:4);
    _p8_7 = ('120':char:1);
    PUSH _p8_7;
    PUSH _p7_6;
    PUSH _p6_5;
    PUSH _p5_4;
    PUSH _p4_3;
    PUSH _p3_2;
    PUSH _p2_1;
    CALL printf;
    POP 56;
_L1:
    LEAVE;
 EndFunc ;


__printf():
 BeginFunc ;
_L1:
    LEAVE;
 EndFunc ;


__print():
 BeginFunc ;
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