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
    table_->set_stack_frame(current_frame);
}

void Code_Generator::from_leave_ita()
{
    // instructions_.emplace_back("leave");
    // instructions_.emplace_back("ret");
}

Code_Generator::Operand_Size Code_Generator::get_size_from_table_rvalue(
    ir::Table::RValue_Data_Type const& rvalue)
{
    using T = ir::Table::Type;
    ir::Table::Type type = table_->get_type_from_symbol(rvalue);
    return m::match(type)(
        m::pattern | m::or_(T{ "int" }, T{ "string" }) =
            [&] { return Operand_Size::Dword; },
        m::pattern | m::or_(T{ "double" }, T{ "long" }) =
            [&] { return Operand_Size::Qword; },
        m::pattern | T{ "float" } = [&] { return Operand_Size::Dword; },
        m::pattern | T{ "char" } = [&] { return Operand_Size::Byte; },
        m::pattern | m::_ = [&] { return Operand_Size::Dword; });
}

Register Code_Generator::get_acc_register_from_size(Operand_Size size)
{
    // clang-format off
    return m::match(size) (
        m::pattern | Operand_Size::Qword = [&] {
            return Register::rax;
        },
        m::pattern | m::_ = [&] {
            return Register::eax;
        }
    );
    // clang-format on
}

Code_Generator::Data_Location Code_Generator::imul(
    ir::Table::RValue_Data_Type const& lhs,
    ir::Table::RValue_Data_Type const& rhs)
{
    Instructions mul_inst{};
    auto lhs_size = get_size_from_table_rvalue(lhs);
    Immediate lhs_imm = std::get<0>(lhs);
    Immediate rhs_imm = std::get<0>(rhs);

    auto storage = get_acc_register_from_size(lhs_size);

    mul_inst.emplace_back(Mnemonic::mov, lhs_size, lhs_imm, storage);
    mul_inst.emplace_back(Mnemonic::imul, lhs_size, storage, rhs_imm);

    return std::make_pair(storage, mul_inst);
}

Code_Generator::Data_Location Code_Generator::from_ita_binary_expression(
    ir::ITA::Quadruple const& inst)
{
    CREDENCE_ASSERT(std::get<0>(inst) == ir::ITA::Instruction::MOV);
    Data_Location instructions{ Register::noop, {} };
    auto table_rvalue = table_->get_rvalue_from_mov_instruction(inst);
    auto expression = table_->from_rvalue_binary_expression(table_rvalue.first);
    std::string binary_op = std::get<2>(expression);
    Immediate lhs = util::contains(std::get<0>(expression), "_t")
                        ? table_->from_temporary_lvalue(std::get<0>(expression))
                        : std::get<0>(expression);
    Immediate rhs = util::contains(std::get<1>(expression), "_t")
                        ? table_->from_temporary_lvalue(std::get<1>(expression))
                        : std::get<1>(expression);
    m::match(binary_op)(
        // cppcheck-suppress syntaxError
        m::pattern | std::string{ "*" } =
            [&] {
                // @TODO: symbols
                auto lhs_rvalue = table_->get_rvalue_symbol_type_size(lhs);
                auto rhs_rvalue = table_->get_rvalue_symbol_type_size(rhs);
                instructions = imul(lhs_rvalue, rhs_rvalue);
            },
        m::pattern | m::_ =
            [&] {
                auto lhs_rvalue = table_->get_rvalue_symbol_type_size(lhs);
                auto rhs_rvalue = table_->get_rvalue_symbol_type_size(rhs);
                instructions = imul(lhs_rvalue, rhs_rvalue);
            });
    return instructions;
}

} // namespace x86_64
