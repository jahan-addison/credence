#include <credence/target/x86_64.h>

#include <iostream>

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
    table_->set_stack_frame("main");
}

void Code_Generator::from_leave_ita()
{
    instructions_.emplace_back("leave");
    instructions_.emplace_back("ret");
}

Code_Generator::Data_Location Code_Generator::from_ita_binary_expression(
    ir::ITA::Quadruple const& inst)
{
    CREDENCE_ASSERT(std::get<0>(inst) == ir::ITA::Instruction::MOV);
    Instructions instructions{};
    Storage address{};
    auto table_rvalue = table_->get_rvalue_from_mov_instruction(inst);
    auto expression = table_->from_rvalue_binary_expression(table_rvalue.first);
    auto binary_op = std::get<2>(expression);
    Immediate lhs = util::contains(std::get<0>(expression), "_t")
                        ? table_->from_temporary_lvalue(std::get<0>(expression))
                        : std::get<0>(expression);

    Immediate rhs = util::contains(std::get<1>(expression), "_t")
                        ? table_->from_temporary_lvalue(std::get<1>(expression))
                        : std::get<1>(expression);

    // clang-format off
    m::match(binary_op) (
        // cppcheck-suppress syntaxError
        m::pattern | "*" = [&] {
            std::cout << "lhs: " << lhs << " rhs: " << rhs << std::endl;
        },
        m::pattern | m::_ = [&] {

        }
    );
    // clang-format on
    return { address, instructions };
}

} // namespace x86_64
