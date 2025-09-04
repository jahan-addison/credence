#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase, TEST_CASE

#include <array>             // for array
#include <credence/symbol.h> // for Symbol_Table
#include <map>               // for map
#include <string>            // for allocator, basic_string, operator==
#include <vector>            // for vector

using namespace credence;

TEST_CASE("symbol.cc: Symbol_table::get_symbol_by_name")
{
    auto temp = Symbol_Table<int>{};

    temp.table_.emplace("test", 10);

    CHECK(temp.get_symbol_by_name("test") == 10);

    auto temp2 = Symbol_Table<std::array<std::string, 2>>{};
    temp2.table_.emplace("test",
                         std::array<std::string, 2>({ "hello", "world" }));

    CHECK(temp2.get_symbol_by_name("test")[0] == "hello");
    CHECK(temp2.get_symbol_by_name("test")[1] == "world");
}

TEST_CASE("symbol.cc: Symbol_table::set_symbol_by_name")
{
    auto temp = Symbol_Table<int>{};
    temp.set_symbol_by_name("test", 10);
    CHECK(temp.table_["test"] == 10);
    auto temp2 = Symbol_Table<std::array<std::string, 2>>{};
    std::array<std::string, 2> test = { "hello", "world" };
    temp2.set_symbol_by_name("test", test);
    CHECK(temp2.table_["test"][0] == "hello");
    CHECK(temp2.table_["test"][1] == "world");
}
