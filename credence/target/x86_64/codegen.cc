
#include <iostream>

#include "codegen.h"
#include <credence/assert.h>   // for CREDENCE_ASSERT
#include <credence/ir/ita.h>   // for ITA
#include <credence/ir/table.h> // for Table
#include <credence/util.h>     // for contains
#include <matchit.h>           // for Wildcard, Ds, pattern, match, Id, _
#include <string_view>         // for basic_string_view
#include <tuple>               // for get
#include <utility>             // for pair

namespace credence::target::x86_64 {

namespace m = matchit;

inline void Code_Generator::setup_table()
{
    table_->set_stack_frame(current_frame);
}

Code_Generator::Operation_Pair Code_Generator::from_ita_binary_expression(
    ir::ITA::Quadruple const& inst)
{
    CREDENCE_ASSERT(std::get<0>(inst) == ir::ITA::Instruction::MOV);
    Operation_Pair instructions{ Register::noop, {} };
    auto table_rvalue = table_->get_rvalue_from_mov_instruction(inst);
    auto expression = table_->from_rvalue_binary_expression(table_rvalue.first);
    std::string binary_op = std::get<2>(expression);
    std::string lhs =
        util::contains(std::get<0>(expression), "_t")
            ? table_->from_temporary_lvalue(std::get<0>(expression))
            : std::get<0>(expression);
    std::string rhs =
        util::contains(std::get<1>(expression), "_t")
            ? table_->from_temporary_lvalue(std::get<1>(expression))
            : std::get<1>(expression);

    m::match(binary_op)(
        // cppcheck-suppress syntaxError
        m::pattern | std::string{ "*" } =
            [&] {
                // @TODO: symbols
                auto lhs_rvalue = table_->get_rvalue_symbol_type_size(lhs);
                auto rhs_rvalue = table_->get_rvalue_symbol_type_size(rhs);
                auto size = detail::get_size_from_table_rvalue(lhs_rvalue);
                instructions = detail::imul(size, lhs_rvalue, rhs_rvalue);
            },
        m::pattern | m::_ =
            [&] {
                auto lhs_rvalue = table_->get_rvalue_symbol_type_size(lhs);
                auto rhs_rvalue = table_->get_rvalue_symbol_type_size(rhs);
                auto size = detail::get_size_from_table_rvalue(lhs_rvalue);
                instructions = detail::imul(size, lhs_rvalue, rhs_rvalue);
            });
    return instructions;
}

} // namespace x86_64
