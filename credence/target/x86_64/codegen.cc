#include "codegen.h"
#include "instructions.h"      // for get_size_from_table_rvalue, Operation...
#include <credence/assert.h>   // for CREDENCE_ASSERT
#include <credence/ir/ita.h>   // for ITA
#include <credence/ir/table.h> // for Table
#include <credence/util.h>     // for contains
#include <deque>               // for deque
#include <matchit.h>           // for pattern, PatternHelper, PatternPipable
#include <string_view>         // for basic_string_view
#include <tuple>               // for get
#include <utility>             // for pair, make_pair
#include <variant>             // for get, variant

#include <format>

namespace credence::target::x86_64 {

namespace m = matchit;

void Code_Generator::setup_table()
{
    table_->set_stack_frame(current_frame);
}

Code_Generator::RValue_Operands
Code_Generator::resolve_immediate_operands_from_table(
    Immediate_Operands const& imm_value)
{
    RValue_Operands result = ir::Table::NULL_RVALUE_LITERAL;
    std::visit(
        util::overload{
            [&](ir::Table::Binary_Expression const& s) {
                auto symbols = table_->get_stack_frame_symbols();
                auto lhs =
                    util::substring_count_of(std::get<0>(s), ":") == 2
                        ? table_->get_symbol_type_size_from_rvalue_string(
                              std::get<0>(s))
                        : symbols.get_symbol_by_name(std::get<0>(s));
                auto rhs =
                    util::substring_count_of(std::get<1>(s), ":") == 2
                        ? table_->get_symbol_type_size_from_rvalue_string(
                              std::get<1>(s))
                        : symbols.get_symbol_by_name(std::get<1>(s));

                result = std::make_pair(lhs, rhs);
            },
            [&](ir::Table::RValue_Data_Type const& s) { result = s; } },
        imm_value);
    return result;
}

Code_Generator::Binary_Operands
Code_Generator::operands_from_binary_ita_operands(
    ir::ITA::Quadruple const& inst)
{
    auto table_rvalue = table_->get_rvalue_from_mov_instruction(inst);
    auto expression = table_->from_rvalue_binary_expression(table_rvalue.first);
    std::string binary_op = std::get<2>(expression);

    auto imm_operands =
        std::get<0>(resolve_immediate_operands_from_table(expression));

    Operands operands = { imm_operands.first, imm_operands.second };
    return std::make_pair(binary_op, operands);
}

Operation_Pair Code_Generator::from_ita_binary_arithmetic_expression(
    ir::ITA::Quadruple const& inst)
{
    CREDENCE_ASSERT(std::get<0>(inst) == ir::ITA::Instruction::MOV);
    Operation_Pair instructions{ Register::eax, {} };
    auto operands = operands_from_binary_ita_operands(inst);
    m::match(operands.first)(
        // cppcheck-suppress syntaxError
        m::pattern | std::string{ "*" } =
            [&] {
                auto size = get_size_from_table_rvalue(
                    std::get<Immediate>(operands.second.first));
                instructions =
                    imul(size, operands.second.first, operands.second.second);
            },
        m::pattern | std::string{ "/" } =
            [&] {
                auto size = get_size_from_table_rvalue(
                    std::get<Immediate>(operands.second.first));
                instructions =
                    idiv(size, operands.second.first, operands.second.second);
            },
        m::pattern | std::string{ "-" } =
            [&] {
                auto size = get_size_from_table_rvalue(
                    std::get<Immediate>(operands.second.first));
                instructions =
                    sub(size, operands.second.first, operands.second.second);
            },
        m::pattern | std::string{ "+" } =
            [&] {
                auto size = get_size_from_table_rvalue(
                    std::get<Immediate>(operands.second.first));
                instructions =
                    add(size, operands.second.first, operands.second.second);
            },
        m::pattern | std::string{ "%" } =
            [&] {
                auto size = get_size_from_table_rvalue(
                    std::get<Immediate>(operands.second.first));
                instructions =
                    mod(size, operands.second.first, operands.second.second);
            });

    return instructions;
}

} // namespace x86_64
