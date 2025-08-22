// clang-format off
#include <doctest/doctest.h>  // for ResultBuilder, CHECK, TestCase, TEST_CASE
#include <roxas/ir/qaud.h>    // for build_from_auto_statement, build_from_r...
#include <roxas/json.h>       // for JSON
#include <roxas/symbol.h>     // for Symbol_Table
#include <roxas/types.h>      // for Type_, Value_Type, Byte, RValue
#include <array>              // for array
#include <deque>              // for operator==, _Deque_iterator
#include <iostream>           // for cout
#include <map>                // for map
#include <string>             // for allocator, operator<=>, operator==, bas...
#include <tuple>              // for tuple
#include <utility>            // for pair, make_pair, operator==
#include <variant>            // for operator==, monostate
// clang-format on

TEST_CASE("ir/qaud.cc: build_from_rvalue_statement")
{
    using namespace roxas;
    using namespace roxas::ir::qaud;
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
    obj["rvalue_statement"] = json::JSON::Load(
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
        "              \"right\" : {\n                      \"node\" : "
        "\"number_literal\",\n                      \"root\" : 3\n             "
        "       },\n                    \"root\" : [\"==\"]\n                  "
        "},\n                  \"root\" : [\"=\", null]\n                "
        "}]],\n            \"node\" : \"statement\",\n            \"root\" : "
        "\"rvalue\"\n          }");
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
    roxas::Symbol_Table<> symbols{};
    std::array<std::string, 2> tests = { "PUSH x", "CALL putchar" };
    type::RValue::Value null = { std::monostate(), type::Type_["null"] };
    symbols.table_.emplace("x", null);
    symbols.table_.emplace("double", null);
    symbols.table_.emplace("exp", null);
    symbols.table_.emplace("puts", null);
    symbols.table_.emplace("y", null);
    auto instructions =
        build_from_rvalue_statement(symbols, obj["nested_or"], obj);
    for (auto const& inst : instructions) {
        emit_quadruple(std::cout, inst);
    }
}

TEST_CASE("ir/qaud.cc: build_from_auto_statement")
{
    using namespace roxas;
    using namespace roxas::ir::qaud;
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
