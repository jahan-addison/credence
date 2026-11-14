#include <doctest/doctest.h> // for ResultBuilder, CHECK, TestCase

#include <credence/ir/ita.h>           // for ITA
#include <credence/ir/table.h>         // for emit
#include <credence/language/literal.h> // for Value_Type, TYPE_LITERAL, Byte, NULL_L...
#include <credence/language/parser.h>         // for Parser
#include <credence/language/symbol_builder.h> // for Symbol_Builder
#include <credence/symbol.h>                  // for Symbol_Table
#include <credence/util.h>                    // for AST_Node
#include <deque>                              // for deque
#include <easyjson.h>                         // for JSON
#include <map>                                // for map

#include <sstream>        // for basic_ostringstream, ostringstream
#include <string>         // for basic_string, allocator, char_traits
#include <string_view>    // for basic_string_view
#include <test/fixture.h> // for load_json_fixture
#include <tuple>          // for tuple
#include <utility>        // for pair, make_pair
#include <variant>        // for monostate

#define EMIT(os, inst) credence::ir::detail::emit_to(os, inst)

struct ITA_Fixture
{
    using NULL_symbols = std::deque<std::string>;
    using Node = credence::util::AST_Node;
    inline auto make_node() { return credence::util::AST::object(); }
    const Node definitions_symbols =
        credence::test::load_json_fixture("ir/ita/definitions_symbols");
    const Node global_symbols =
        credence::test::load_json_fixture("ir/ita/global_symbols");

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
                s, credence::language::literal::NULL_LITERAL);
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
                s, credence::language::literal::NULL_LITERAL);
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
                s, credence::language::literal::NULL_LITERAL);
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
                s, credence::language::literal::NULL_LITERAL);
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
    obj["switch_4"] = credence::test::load_json_fixture("ir/ita/switch_4");
    obj["ir"] = credence::test::load_json_fixture("ir/ita/ir");

    obj["switch_4-symbols"] =
        credence::test::load_json_fixture("ir/ita/switch_4_symbols");

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
_p6_1 = s;
j = ++j;
_p7_2 = j;
PUSH _p7_2;
PUSH _p6_1;
CALL char;
POP 16;
C = 16;
_t8 = CMP C;
JMP_E _t8 ('45':char:1) _L10;
JMP_E _t8 ('32':char:1) _L16;
JMP_E _t8 ('48':char:1) _L18;
JMP_E _t8 ('44':char:1) _L20;
_L19:
_L17:
_L15:
_L9:
_L25:
_t28 = ('48':char:1) <= c;
_t29 = c <= ('57':char:1);
_t30 = _t28 && _t29;
IF _t30 GOTO _L27;
_L26:
GOTO _L4;
_L10:
_L11:
_t14 = CMP sign;
IF _t14 GOTO _L13;
_L12:
s = (1:int:4);
GOTO _L9;
_L13:
loop = (0:int:4);
CALL error;
GOTO _L12;
_L16:
GOTO _L15;
_L18:
GOTO _L4;
_L20:
_L21:
_t24 = c == ('48':char:1);
IF _t24 GOTO _L23;
_L22:
GOTO _L19;
_L23:
RET i ;
GOTO _L1;
_L27:
_t31 = (10:int:4) * m;
_t32 = _t31 + c;
_t33 = _t32 - ('48':char:1);
m = _t33;
GOTO _L26;
 EndFunc ;
__char(a,b):
 BeginFunc ;
_L1:
LEAVE;
 EndFunc ;
__error():
 BeginFunc ;
_p2_3 = ("bad syntax*n":string:12);
PUSH _p2_3;
CALL printf;
POP 8;
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
_p2_1 = x;
_p4_3 = x;
_p5_4 = y;
PUSH _p5_4;
PUSH _p4_3;
CALL sub;
POP 16;
add = _p3_2;
PUSH _p3_2;
PUSH _p2_1;
CALL z;
POP 16;
_t6 = add - 16;
(2:int:4) = _t6;
_L7:
_t10 = x > y;
IF _t10 GOTO _L9;
_L8:
x = (0:int:4);
_L1:
LEAVE;
_L9:
_L11:
_L13:
_t14 = z > x;
IF _t14 GOTO _L12;
GOTO _L8;
_L12:
z = --z;
GOTO _L11;
 EndFunc ;
)ita";
    TEST_DEFINITIONS_WITH(obj["switch_4"], expected, obj["switch_4-symbols"]);
    TEST_FUNCTION_DEFINITION_WITH(obj["ir"], expected_2, { "add", "sub" });
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_definitions")
{
    auto obj = make_node();
    obj["call_3"] = credence::test::load_json_fixture("ir/ita/call_3");

    std::string expected = R"ita(__main():
 BeginFunc ;
CALL number;
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
_p2_1 = (5:int:4);
PUSH _p2_1;
CALL ret;
POP 8;
_L1:
LEAVE;
 EndFunc ;
)ita";
    TEST_DEFINITIONS_WITH(obj["call_3"], expected);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_function_definition")
{
    auto obj = make_node();
    obj["function_2"] = credence::test::load_json_fixture("ir/ita/function_2");

    std::string expected = R"ita(__main():
 BeginFunc ;
LOCL x;
_t4 = (5:int:4) * (5:int:4);
_p2_1 = (2:int:4);
_p3_2 = (5:int:4);
PUSH _p3_2;
PUSH _p2_1;
CALL exp;
POP 16;
_t5 = x + _t4;
_t5 = _t5;
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

    obj["recursion"] = credence::test::load_json_fixture("ir/ita/recursion");

    std::string expected = R"ita(__exp(x,y):
 BeginFunc ;
_L2:
_t5 = x == (1:int:4);
_t6 = y == (1:int:4);
_t7 = _t5 || _t6;
IF _t7 GOTO _L4;
_L3:
_t11 = x - (1:int:4);
_p9_1 = _t11;
_t12 = y - (1:int:4);
_p10_2 = _t12;
PUSH _p10_2;
PUSH _p9_1;
CALL exp;
POP 16;
_t13 = RET;
RET _t13;
_L1:
LEAVE;
_L4:
_t8 = x * y;
RET _t8;
GOTO _L1;
 EndFunc ;
)ita";
    TEST_FUNCTION_DEFINITION_WITH(obj["recursion"], expected);
}

TEST_CASE_FIXTURE(ITA_Fixture,
    "ir/ita.cc: nested function call and return rvalues")
{
    auto obj = make_node();

    obj["test"] =
        credence::test::load_json_fixture("ir/ita/nested_call_return");

    auto ita = ITA_hoisted(global_symbols);
    auto definitions = obj["test"]["left"].to_deque();
    auto expected = R"ita(LOCL x;
_p3_2 = (2:int:4);
_p4_3 = (5:int:4);
PUSH _p4_3;
PUSH _p3_2;
CALL exp;
POP 16;
exp = _p2_1;
_p5_4 = (2:int:4);
PUSH _p5_4;
PUSH _p2_1;
CALL x;
POP 16;
)ita";
    TEST_BLOCK_STATEMENT_NODE_WITH(
        global_symbols, definitions[0]["right"], expected, false, false);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_block_statement")
{
    auto obj = make_node();
    obj["test"] = credence::test::load_json_fixture("ir/ita/block_statement");
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

    obj["test"] = credence::test::load_json_fixture("ir/ita/extrn_statement");

    auto vectors = obj["test"].to_deque();
    auto ita = ITA_hoisted(obj);
    credence::ir::Instructions instructions{};

    CHECK_THROWS(ita.build_from_extrn_statement(obj["test"], instructions));

    ita.globals_.addr_.emplace("a",
        credence::language::literal::Array{
            credence::language::literal::NULL_LITERAL });
    ita.globals_.addr_.emplace("b",
        credence::language::literal::Array{
            credence::language::literal::NULL_LITERAL });
    ita.globals_.addr_.emplace("c",
        credence::language::literal::Array{
            credence::language::literal::NULL_LITERAL });

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
    obj["symbols"] =
        credence::test::load_json_fixture("ir/ita/vector_definition_symbols");

    obj["test"] = credence::test::load_json_fixture("ir/ita/vector_definition");

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
    auto internal_symbols =
        credence::test::load_json_fixture("ir/ita/return_statement_symbols");
    obj["test"] = credence::test::load_json_fixture("ir/ita/return_statement");

    std::string expected = R"ita(_t1 = x * y;
_t2 = _t1 * y;
RET _t2;
)ita";
    TEST_RETURN_STATEMENT_NODE_WITH(
        internal_symbols, { "x", "y" }, obj["test"], expected);

    // in a function, a return resumes at the end of the function
    std::ostringstream os_test;
    auto hoisted = ITA_hoisted(internal_symbols);
    hoisted.make_root_branch();
    for (auto const& s : { "x", "y" })
        hoisted.symbols_.table_.emplace(
            s, credence::language::literal::NULL_LITERAL);
    for (auto const& inst : hoisted.build_from_return_statement(obj["test"]))
        EMIT(os_test, inst);
    REQUIRE(os_test.str() == R"ita(_t2 = x * y;
_t3 = _t2 * y;
RET _t3;
GOTO _L1;
)ita");
}

TEST_CASE("ir/ita.cc: erase_unreachable_goto_instructions")
{
    using credence::ir::Instruction;
    using credence::ir::make_quadruple;
    auto instructions = credence::ir::Instructions{
        make_quadruple(Instruction::RETURN, "x"),
        make_quadruple(Instruction::GOTO, "_L1"),
        make_quadruple(Instruction::GOTO, "_L3"),
        make_quadruple(Instruction::GOTO, "_L4"),
        make_quadruple(Instruction::LABEL, "_L3"),
        make_quadruple(Instruction::GOTO, "_L1"),
    };
    credence::ir::detail::erase_unreachable_goto_instructions(instructions);
    REQUIRE(instructions.size() == 4);
    REQUIRE(std::get<1>(instructions[1]) == "_L1");
    REQUIRE(std::get<1>(instructions[2]) == "_L3");
    REQUIRE(std::get<0>(instructions[3]) == Instruction::GOTO);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_block_statement")
{
    auto obj = make_node();
    obj["test"] = credence::test::load_json_fixture("ir/ita/block_statement_2");

    auto expected = R"ita(LOCL x;
_t2 = (5:int:4) || (2:int:4);
x = _t2;
)ita";
    TEST_BLOCK_STATEMENT_NODE_WITH(obj, obj["test"], expected, false, false);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: switch and case statements")
{
    auto obj = make_node();
    obj["symbols"] = credence::test::load_json_fixture("ir/ita/switch_symbols");

    obj["switch"] = credence::test::load_json_fixture("ir/ita/switch");

    obj["switch_2"] = credence::test::load_json_fixture("ir/ita/switch_2");

    obj["switch_3"] = credence::test::load_json_fixture("ir/ita/switch_3");

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
    obj["symbols"] = credence::test::load_json_fixture("ir/ita/while_symbols");

    obj["while_4"] = credence::test::load_json_fixture("ir/ita/while_4");

    obj["while_2"] = credence::test::load_json_fixture("ir/ita/while_2");

    obj["while"] = credence::test::load_json_fixture("ir/ita/while");

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
GOTO _L14;
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
_t13 = y;
y = --y;
x = _t13;
GOTO _L9;
_L14:
x = ++x;
y = (5:int:4);
_t15 = x + y;
_t16 = x + x;
_t17 = _t15 * _t16;
x = _t17;
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
_L7:
_L9:
_t10 = x <= (100:int:4);
IF _t10 GOTO _L8;
x = (2:int:4);
_L1:
LEAVE;
_L3:
x = --x;
_t6 = y;
y = --y;
x = _t6;
GOTO _L2;
_L8:
x = ++x;
GOTO _L7;
)ita";
    std::string expected_3 = R"ita(x = (100:int:4);
y = (100:int:4);
_L2:
_t5 = x == (100:int:4);
IF _t5 GOTO _L4;
_L3:
x = ++x;
y = (5:int:4);
_t15 = x + y;
_t16 = x + x;
_t17 = _t15 * _t16;
x = _t17;
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
_t14 = y;
y = --y;
x = _t14;
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
    obj["if"] = credence::test::load_json_fixture("ir/ita/if");

    obj["if_2"] = credence::test::load_json_fixture("ir/ita/if_2");
    obj["if_5"] = credence::test::load_json_fixture("ir/ita/if_5");

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
    obj["symbols"] = credence::test::load_json_fixture("ir/ita/truthy_symbols");

    obj["if_4"] = credence::test::load_json_fixture("ir/ita/if_4");

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
    obj["unary_2"] = credence::test::load_json_fixture("ir/ita/unary_2");
    auto expected = R"ita(LOCL x;
LOCL C;
LOCL s;
LOCL j;
j = (0:int:4);
j = ++j;
_p2_1 = s;
j = ++j;
_p3_2 = j;
PUSH _p3_2;
PUSH _p2_1;
CALL char;
POP 16;
j = C;
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
    obj["symbols"] =
        credence::test::load_json_fixture("ir/ita/label_goto_symbols");

    obj["test"] = credence::test::load_json_fixture("ir/ita/label_goto");

    std::string expected = R"ita(LOCL x;
LOCL y;
__LADD:
_p2_1 = (2:int:4);
_p3_2 = (5:int:4);
PUSH _p3_2;
PUSH _p2_1;
CALL add;
POP 16;
x = 16;
y = (10:int:4);
GOTO __LADD;
)ita";
    TEST_BLOCK_STATEMENT_NODE_WITH(
        obj["symbols"], obj["test"], expected, false, false);
}

TEST_CASE_FIXTURE(ITA_Fixture, "ir/ita.cc: build_from_rvalue_statement")
{
    auto obj = make_node();
    obj["test"] = credence::test::load_json_fixture("ir/ita/rvalue_statement");
    obj["nested_binary"] =
        credence::test::load_json_fixture("ir/ita/nested_binary");
    obj["nested_or"] = credence::test::load_json_fixture("ir/ita/nested_or");
    obj["complex_or"] = credence::test::load_json_fixture("ir/ita/complex_or");
    obj["or_with_call"] =
        credence::test::load_json_fixture("ir/ita/or_with_call");

    std::deque<std::string> symbols = {
        "x", "putchar", "getchar", "double", "exp", "puts", "y"
    };

    std::string expected_1 = R"ita(_t3 = (5:int:4) * (5:int:4);
_p1_1 = (2:int:4);
_p2_2 = (5:int:4);
PUSH _p2_2;
PUSH _p1_1;
CALL exp;
POP 16;
_t4 = ~ (4:int:4);
_t5 = _t4;
_t6 = _t3 / _t5;
_t7 = _t3 + _t6;
)ita";
    std::string expected_2 = R"ita(y = (3:int:4);
_t1 = y == (3:int:4);
_t2 = y > (2:int:4);
_t3 = _t1 && _t2;
x = _t3;
)ita";

    std::string expected_3 = R"ita(y = (3:int:4);
_t1 = (1:int:4) || (2:int:4);
_t2 = _t1 || (3:int:4);
x = _t2;
)ita";

    std::string expected_4 = R"ita(y = (3:int:4);
_t1 = (1:int:4) + (1:int:4);
_t2 = (2:int:4) + (2:int:4);
_t3 = _t1 || _t2;
_t4 = (3:int:4) + (3:int:4);
_t5 = _t3 || _t4;
x = _t5;
)ita";
    std::string expected_5 = R"ita(y = (3:int:4);
_p1_1 = (5:int:4);
PUSH _p1_1;
CALL putchar;
POP 8;
_t3 = (1:int:4) + (1:int:4);
_p2_2 = (1:int:4);
PUSH _p2_2;
CALL getchar;
POP 8;
_t4 = RET;
_t5 = _t4;
_t6 = (3:int:4) + (3:int:4);
_t7 = _t3 || _t5;
_t8 = _t7 || _t6;
x = _t8;
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
    obj["test"] = credence::test::load_json_fixture("ir/ita/auto_statement");

    credence::ir::Instructions instructions{};

    ita_.build_from_auto_statement(obj["test"], instructions);

    CHECK(ita_.symbols_.table_.size() == 3);
    CHECK(ita_.symbols_.table_.contains("x") == true);
    CHECK(ita_.symbols_.table_.contains("y") == true);
    CHECK(ita_.symbols_.table_.contains("z") == true);

    language::literal::Literal empty_value = std::make_pair(
        std::monostate(), language::literal::TYPE_LITERAL.at("null"));
    language::literal::Literal word_value =
        std::make_pair("__WORD__", language::literal::TYPE_LITERAL.at("word"));
    language::literal::Literal byte_value = std::make_pair(
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
    auto internal_symbols =
        credence::test::load_json_fixture("ir/ita/deep_evaluated_symbols");
    obj["test"] = credence::test::load_json_fixture("ir/ita/deep_evaluated");

    std::string expected = R"ita(_t1 = (5:int:4) + (5:int:4);
_t2 = (6:int:4) + (6:int:4);
_t3 = _t1 * _t2;
x = _t3;
)ita";
    TEST_RVALUE_STATEMENT_NODE_WITH(obj, { "x" }, obj["test"], expected);
}

/**
 * @brief Emit the IR of a B program
 */
inline std::string emit_ir_from_source(std::string const& source)
{
    auto ast = credence::language::Parser::parse(source);
    auto symbols = credence::language::Symbol_Builder::build(ast);
    std::ostringstream os;
    credence::ir::emit(os, symbols, ast);
    return os.str();
}

TEST_CASE("ir/temporary.cc: indirection in a relation chain")
{
    // the dereference is an operand of the binary expression, `*p + 1`
    REQUIRE(emit_ir_from_source(
                "main() { auto x, *p; x = 1; p = &x; x = *p + 1; }") ==
            R"ita(__main():
 BeginFunc ;
    LOCL x;
    LOCL *p;
    x = (1:int:4);
    _t2 = & x;
    p = _t2;
    _t3 = *p + (1:int:4);
    x = _t3;
_L1:
    LEAVE;
 EndFunc ;

)ita");
}

TEST_CASE("ir/temporary.cc: post-increment and decrement value")
{
    // the value of a post-increment is the operand before the increment
    REQUIRE(emit_ir_from_source("main() { auto x, y; x = 1; y = x++; x--; }") ==
            R"ita(__main():
 BeginFunc ;
    LOCL x;
    LOCL y;
    x = (1:int:4);
    _t2 = x;
    x = ++x;
    y = _t2;
    x = --x;
_L1:
    LEAVE;
 EndFunc ;

)ita");
}

TEST_CASE("ir/temporary.cc: binary operand order")
{
    // the left-hand-side of a binary expression stays on the left
    REQUIRE(emit_ir_from_source("main() { auto x, y; x = 1; y = -x - 1; }") ==
            R"ita(__main():
 BeginFunc ;
    LOCL x;
    LOCL y;
    x = (1:int:4);
    _t2 = - x;
    _t3 = _t2;
    y = _t3 - (1:int:4);
_L1:
    LEAVE;
 EndFunc ;

)ita");
    // each operand of a logical chain is its own temporary
    REQUIRE(
        emit_ir_from_source("main() { auto x; x = 1 + 1 || f(1) || 3 + 3; } "
                            "f(a) { return(a); }") ==
        R"ita(__main():
 BeginFunc ;
    LOCL x;
    _t3 = (1:int:4) + (1:int:4);
    _p2_1 = (1:int:4);
    PUSH _p2_1;
    CALL f;
    POP 8;
    _t4 = RET;
    _t5 = _t4;
    _t6 = (3:int:4) + (3:int:4);
    _t7 = _t3 || _t5;
    _t8 = _t7 || _t6;
    x = _t8;
_L1:
    LEAVE;
 EndFunc ;


__f(a):
 BeginFunc ;
    RET a ;
_L1:
    LEAVE;
 EndFunc ;

)ita");
}

TEST_CASE("ir/temporary.cc: insert_expression_value")
{
    // the value of a return is the whole expression, `-x - 1`
    REQUIRE(emit_ir_from_source("f(x) { return(-x - 1); } main() { }") ==
            R"ita(__f(x):
 BeginFunc ;
    _t2 = - x;
    _t3 = _t2;
    _t4 = _t3 - (1:int:4);
    RET _t4;
_L1:
    LEAVE;
 EndFunc ;


__main():
 BeginFunc ;
_L1:
    LEAVE;
 EndFunc ;

)ita");
    // the value of a return of a call is the return value of the call
    REQUIRE(emit_ir_from_source("f(x) { return(f(x)); } main() { }") ==
            R"ita(__f(x):
 BeginFunc ;
    _p2_1 = x;
    PUSH _p2_1;
    CALL f;
    POP 8;
    _t3 = RET;
    RET _t3;
_L1:
    LEAVE;
 EndFunc ;


__main():
 BeginFunc ;
_L1:
    LEAVE;
 EndFunc ;

)ita");
}
