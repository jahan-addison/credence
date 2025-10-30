#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase, TEST_CASE

#include <credence/ir/ita.h>
#include <credence/ir/table.h>
#include <credence/target/x86_64.h>
#include <credence/util.h> // for AST_Node, AST
#include <filesystem>
#include <simplejson.h>
#include <string>
#include <utility>

using namespace credence;
using namespace credence::target;

namespace fs = std::filesystem;

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

#define ROOT_PATH STRINGIFY(ROOT_TEST_PATH)

template<typename T, typename K>
inline void require_is_imm_instruction(
    x86_64::Code_Generator::Instruction& inst,
    x86_64::Mnemonic mn,
    x86_64::Code_Generator::Operand_Size size,
    x86_64::Code_Generator::Storage s1,
    x86_64::Code_Generator::Storage s2)
{
    auto s1_type = std::get<T>(s1);
    auto s2_type = std::get<K>(s2);

    auto inst_s1 = std::get<T>(std::get<2>(inst));
    auto inst_s2 = std::get<K>(std::get<3>(inst));

    REQUIRE(std::get<0>(inst) == mn);
    REQUIRE(std::get<1>(inst) == size);
    REQUIRE(inst_s1 == s1_type);
    REQUIRE(inst_s2 == s2_type);
}

#define REQUIRE_IS_IMM_INSTRUCTION(inst, mn, size, s1, s2) \
 do {                                                  \
    REQUIRE(std::get<0>(inst) == mn);                  \
    REQUIRE(std::get<1>(inst) == size);                \
    REQUIRE(std::get<2>(inst) == s1);                  \
    REQUIRE(std::get<3>(inst) == s2);                  \
 } while(0)

#define EMIT(os, inst) credence::ir::ITA::emit_to(os, inst)
#define LOAD_JSON_FROM_STRING(str) credence::util::AST_Node::load(str)

inline auto fixture_files_root_path()
{
    auto type_fixtures_path = fs::path(ROOT_PATH);
    type_fixtures_path.append("test/fixtures/x86_64/ast");
    return type_fixtures_path;
}

struct Table_Fixture
{
    using NULL_symbols = std::deque<std::string>;
    using Node = credence::util::AST_Node;
    const Node base_symbols = LOAD_JSON_FROM_STRING(
        "{\"x\": {\"type\": \"lvalue\", \"line\": 2, \"start_pos\": 16, "
        "\"column\": 8, \"end_pos\": 17, \"end_column\": 9}, \"y\": {\"type\": "
        "\"lvalue\", \"line\": 2, \"start_pos\": 19, \"column\": 11, "
        "\"end_pos\": 20, \"end_column\": 12}, \"main\": {\"type\": "
        "\"function_definition\", \"line\": 1, \"start_pos\": 0, \"column\": "
        "1, \"end_pos\": 4, \"end_column\": 5}}");

    static inline auto make_node() { return credence::util::AST::object(); }
    static inline auto make_table(Node const& symbols, Node const& node)
    {
        return credence::ir::Table::build_from_ast(symbols, node);
    }
    static inline credence::ir::Table make_table_with_global_symbols(
        Node const& node,
        Node const& symbols)
    {
        using namespace credence::ir;
        auto ita = ITA{ symbols };
        auto instructions = ita.build_from_definitions(node);
        auto table = Table{ symbols, instructions };
        table.build_vector_definitions_from_globals(ita.globals_);
        table.build_from_ita_instructions();
        return table;
    }
};

TEST_CASE_FIXTURE(Table_Fixture, "Code_Generator::imul")
{
    using namespace credence::target::x86_64;
    Node base_ast = LOAD_JSON_FROM_STRING(
        "{\n  \"left\" : [{\n      \"left\" : [null],\n      \"node\" : "
        "\"function_definition\",\n      \"right\" : {\n        \"left\" : "
        "[],\n        \"node\" : \"statement\",\n        \"root\" : "
        "\"block\"\n      },\n      \"root\" : \"main\"\n    }],\n  \"node\" : "
        "\"program\",\n  \"root\" : \"definitions\"\n}");
    auto table = make_table(base_symbols, base_ast);
    auto code = target::x86_64::Code_Generator{ std::move(table) };

    auto test_rvalue = ir::Table::RValue_Data_Type{ "5", "int", 4UL };
    auto test_rvalue_2 = ir::Table::RValue_Data_Type{ "10", "int", 4UL };

    auto test = code.imul(test_rvalue, test_rvalue_2);

    require_is_imm_instruction<std::string, Register>(
        test.second.at(0),
        Mnemonic::mov,
        Code_Generator::Operand_Size::Dword,
        "5",
        Register::eax);

    require_is_imm_instruction<Register, std::string>(
        test.second.at(1),
        Mnemonic::imul,
        Code_Generator::Operand_Size::Dword,
        Register::eax,
        "10");
}

TEST_CASE_FIXTURE(Table_Fixture, "Code_Generator::from_ita_binary_expression")
{
    Node base_ast = LOAD_JSON_FROM_STRING(
        "{\n  \"left\" : [{\n      \"left\" : [null],\n      \"node\" : "
        "\"function_definition\",\n      \"right\" : {\n        \"left\" : "
        "[],\n        \"node\" : \"statement\",\n        \"root\" : "
        "\"block\"\n      },\n      \"root\" : \"main\"\n    }],\n  \"node\" : "
        "\"program\",\n  \"root\" : \"definitions\"\n}");
    auto table = make_table(base_symbols, base_ast);
    auto code = target::x86_64::Code_Generator{ std::move(table) };
    auto test = ir::ITA::Quadruple{
        ir::ITA::Instruction::MOV, "x", "(5:int:4) * (5:int:4)", ""
    };
    code.from_ita_binary_expression(test);
}
