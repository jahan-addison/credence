#include <array>
#include <doctest/doctest.h>
#include <roxas/symbol.h>

using namespace roxas;

/*************************************************************************/
/* symbol tests */
/*************************************************************************/

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
    temp2.set_symbol_by_name("test", { "hello", "world" });
    CHECK(temp2.table_["test"][0] == "hello");
    CHECK(temp2.table_["test"][1] == "world");
}
