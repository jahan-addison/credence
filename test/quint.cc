#include <array>
#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase, TEST_CA...
#include <format>            // For std::format
#include <map>               // for map
#include <roxas/ir/quint.h>  // for build_from_auto_statement
#include <roxas/json.h>      // for JSON
#include <roxas/symbol.h>    // for Symbol_Table
#include <roxas/types.h>     // for Value_Type, Type_, Byte
#include <string>            // for basic_string
#include <utility>           // for pair, make_pair
#include <variant>           // for monostate

// TEST_CASE("ir/quint.cc: build_from_rvalue_statement")
// {
//     using namespace roxas;
//     using namespace roxas::ir::quint;
//     json::JSON obj;
//     obj["test"] = json::JSON::Load(
//         "{\n  \"left\": [\n    [\n      {\n        \"left\": {\n          "
//         "\"node\": \"lvalue\",\n          \"root\": \"putchar\"\n        },\n
//         " "      \"node\": \"function_expression\",\n        \"right\": [\n "
//         "    {\n            \"node\": \"lvalue\",\n            \"root\": "
//         "\"x\"\n          }\n        ],\n        \"root\": \"putchar\"\n "
//         "}\n    ]\n  ],\n  \"node\": \"statement\",\n  \"root\": "
//         "\"rvalue\"\n}");

//     roxas::Symbol_Table<> symbols{};
//     std::array<std::string, 2> tests = { "PUSH x", "CALL putchar" };
//     type::RValue::Value null = { std::monostate(), type::Type_["null"] };
//     symbols.table_.emplace("x", null);
//     Instructions test = build_from_rvalue_statement(symbols, obj["test"],
//     obj);

//     CHECK(test.size() == 2);
//     int index = 0;
//     for (auto& instruction : test) {
//         CHECK(tests[index] ==
//               std::format("{} {}",
//                           instruction_to_string(std::get<0>(instruction)),
//                           std::get<1>(instruction)));
//         index++;
//     }
// }

TEST_CASE("ir/quint.cc: build_from_auto_statement")
{
    using namespace roxas;
    using namespace roxas::ir::quint;
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
