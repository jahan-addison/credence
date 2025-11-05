#include "generator.h"
#include "instructions.h"           // for get_size_from_table_rvalue, Imm...
#include <credence/assert.h>        // for CREDENCE_ASSERT
#include <credence/ir/ita.h>        // for ITA
#include <credence/ir/table.h>      // for Table
#include <credence/target/target.h> // for align_up_to_16
#include <credence/util.h>          // for substring_count_of, contains
#include <cstddef>                  // for size_t
#include <deque>                    // for deque
#include <format>                   // for format
#include <matchit.h>                // for pattern, PatternHelper, Pattern...
#include <memory>                   // for shared_ptr
#include <ostream>                  // for basic_ostream, operator<<, endl
#include <sstream>                  // for basic_stringstream
#include <string_view>              // for basic_string_view
#include <tuple>                    // for get, tuple
#include <type_traits>              // for underlying_type_t
#include <utility>                  // for pair, make_pair
#include <variant>                  // for get, holds_alternative, monostate

#define ir_i(name) credence::ir::ITA::Instruction::name
#define is_variant(S, V) std::holds_alternative<S>(V)

namespace credence::target::x86_64 {

namespace m = matchit;

void Code_Generator::emit(std::ostream& os)
{
    build_from_ita_table();
    // clang-format off
    for (auto const& inst : instructions_) {
        std::visit(
            util::overload{
            [&](detail::Instruction const& s) {
                Mnemonic inst = std::get<0>(s);
                os << "    " << inst;
                if (inst != Mnemonic::mov)
                    os << suffix[std::get<1>(s)];
                Storage dest = std::get<2>(s);
                Storage src = std::get<3>(s);
                if (!std::holds_alternative<std::monostate>(dest))
                    os << " " << emit_storage_device(dest);
                if (!std::holds_alternative<std::monostate>(src))
                    os << ", " << emit_storage_device(src);
                os << std::endl;
            },
            [&](ir::Table::Label const& s) {
                os << s << ":" << std::endl;
            } },
                inst);
    }
    // clang-format on
}

void Code_Generator::build_from_ita_table()
{
    auto instructions = table_->instructions;
    std::size_t ita_index = 0;
    for (ita_index = 0; ita_index < instructions.size(); ita_index++) {
        auto inst = instructions[ita_index];
        ir::ITA::Instruction ita_inst = std::get<0>(inst);
        m::match(ita_inst)(
            m::pattern | ir_i(FUNC_START) =
                [&] {
                    auto symbol = std::get<1>(instructions.at(ita_index - 1));
                    auto name = ir::Table::get_label_as_human_readable(symbol);
                    current_frame = name;
                    set_table_stack_frame(name);
                    from_func_start_ita(name);
                },
            m::pattern | ir_i(FUNC_END) = [&] { from_func_end_ita(); },
            m::pattern | ir_i(MOV) = [&] { from_mov_ita(inst); },
            m::pattern | ir_i(LOCL) = [&] { from_locl_ita(inst); },
            m::pattern | ir_i(RETURN) = [&] { from_return_ita(Register::rax); },
            m::pattern | ir_i(LEAVE) = [&] { from_leave_ita(); },
            m::pattern | ir_i(LABEL) = [&] { from_label_ita(inst); },
            m::pattern | ir_i(PUSH) = [&] { from_push_ita(inst); }

        );
    }
}

std::string Code_Generator::emit_storage_device(Storage const& storage)
{
    std::string sloc;
    // @TODO: data storage symbol
    std::visit(
        util::overload{
            [&]([[maybe_unused]] std::monostate s) {},
            [&](detail::Stack_Offset s) {
                sloc = std::format("dword ptr [rbp - {}]", s);
            },
            [&](Register s) {
                std::stringstream ss{};
                ss << s;
                sloc = ss.str();
            },
            [&](detail::Immediate const& s) { sloc = std::get<0>(s); } },
        storage);
    return sloc;
}

void Code_Generator::from_func_start_ita(ir::Table::Label const& name)
{
    CREDENCE_ASSERT(table_->functions.contains(name));
    stack.clear();
    stack_offset = 0;
    current_frame = name;
    set_table_stack_frame(name);
    auto frame = table_->functions[current_frame];
    auto stack_alloc = credence::target::align_up_to_16(frame->allocation);
    add_inst_ld(instructions_, push, Operand_Size::Dword, rbp);
    add_inst_lrs(instructions_, push, Operand_Size::Dword, rbp, rsp);
    if (table_->stack_frame_contains_ita_instruction(
            current_frame, ir::ITA::Instruction::CALL)) {
        add_inst_ll(
            instructions_,
            sub,
            Operand_Size::Dword,
            rsp,
            detail::make_u32_integer_immediate(stack_alloc));
    }
}

Code_Generator::Storage Code_Generator::get_storage_device(Operand_Size size)
{
    auto& registers =
        size == Operand_Size::Dword ? o_dword_register : o_qword_register;
    if (registers.size() > 0) {
        Storage storage = registers.front();
        registers.pop_front();
        return storage;
    } else {
        return get_stack_address(size);
    }
}

Code_Generator::Storage Code_Generator::get_stack_address(Operand_Size size)
{
    return stack_offset +
           static_cast<std::underlying_type_t<Operand_Size>>(size);
}

void Code_Generator::set_table_stack_frame(ir::Table::Label const& name)
{
    table_->set_stack_frame(name);
}

void Code_Generator::from_func_end_ita()
{
    auto frame = table_->functions[current_frame];
    auto stack_alloc = credence::target::align_up_to_16(frame->allocation);
    if (table_->stack_frame_contains_ita_instruction(
            current_frame, ir::ITA::Instruction::CALL)) {
        add_inst_ll(
            instructions_,
            add,
            Operand_Size::Dword,
            rsp,
            detail::make_u32_integer_immediate(stack_alloc));
    }
    reset_o_register();
}

void Code_Generator::from_push_ita(ITA_Inst const& inst)
{
    auto lvalue = std::get<1>(inst);
    auto frame = table_->functions[current_frame];
    auto& locals = table_->get_stack_frame_symbols();
    auto symbol = locals.get_symbol_by_name(lvalue);
    auto storage = get_storage_device();

    add_inst_s(instructions_, mov, Operand_Size::Dword, storage, symbol);
}

void Code_Generator::from_locl_ita(ITA_Inst const& inst)
{
    ir::Table::LValue lvalue = std::get<1>(inst);
    stack_offset +=
        static_cast<std::underlying_type_t<Operand_Size>>(Operand_Size::Dword);
    stack[lvalue] = stack_offset;
}

void Code_Generator::from_cmp_ita([[maybe_unused]] ITA_Inst const& inst)
{
    table_->set_stack_frame(current_frame);
}

void Code_Generator::from_mov_ita(ITA_Inst const& inst)
{
    ir::Table::LValue lhs = std::get<1>(inst);
    auto symbols = table_->get_stack_frame_symbols();
    if (!ir::Table::is_temporary(lhs)) {
        temporary_expansion.reset();
        CREDENCE_ASSERT(stack.contains(lhs));
        ir::Table::RValue rhs =
            table_->get_rvalue_from_mov_instruction(inst).first;
        auto lhs_storage = stack[lhs];
        if (ir::Table::is_rvalue_data_type(rhs)) {
            auto imm = ir::Table::get_symbol_type_size_from_rvalue_string(rhs);
            add_inst_s(
                instructions_, mov, Operand_Size::Dword, lhs_storage, imm);
        } else if (ir::Table::is_temporary(rhs)) {
            insert_from_temporary_table_rvalue(
                table_->from_temporary_lvalue(rhs));
        } else {
            add_inst_s(
                instructions_,
                mov,
                Operand_Size::Dword,
                lhs_storage,
                symbols.get_symbol_by_name(rhs));
        }
    } else {
        if (!temporary_expansion.has_value()) {
            temporary_expansion = get_storage_device();
        }
        insert_from_temporary_table_rvalue(table_->from_temporary_lvalue(lhs));
    }
}

void Code_Generator::insert_from_temporary_table_rvalue(
    ir::Table::RValue const& expr)
{
    Storage lhs_s;
    Storage rhs_s;
    auto symbols = table_->get_stack_frame_symbols();
    if (is_binary_math_operator(expr)) {
        auto expression = table_->from_rvalue_binary_expression(expr);
        auto lhs = std::get<0>(expression);
        auto rhs = std::get<1>(expression);
        if (stack.contains(lhs))
            lhs_s = stack[lhs];
        else if (ir::Table::is_rvalue_data_type(lhs))
            lhs_s = ir::Table::get_symbol_type_size_from_rvalue_string(lhs);
        else
            lhs_s = get_accumulator_register_from_size(Operand_Size::Dword);

        if (stack.contains(rhs))
            rhs_s = stack[rhs];
        else if (ir::Table::is_rvalue_data_type(rhs))
            rhs_s = ir::Table::get_symbol_type_size_from_rvalue_string(rhs);
        else if (symbols.is_defined(rhs))
            rhs_s = symbols.get_symbol_by_name(rhs);
        else
            rhs_s = get_accumulator_register_from_size(Operand_Size::Dword);
        if (temporary_expansion.has_value()) {
            auto storage = temporary_expansion.value();
            add_inst_s(instructions_, mov, Operand_Size::Dword, storage, lhs_s);
            Storage_Operands operands = { storage, rhs_s };
            auto inst = from_storage_arithmetic_expression(
                operands, Operand_Size::Dword, std::get<2>(expression));
            detail::insert_inst(instructions_, inst.second);
        } else {
            Storage_Operands operands = { lhs_s, rhs_s };
            auto inst = from_storage_arithmetic_expression(
                operands, Operand_Size::Dword, std::get<2>(expression));
            detail::insert_inst(instructions_, inst.second);
        }
    } else if (is_relation_binary_operators(expr)) {
        auto expression = table_->from_rvalue_binary_expression(expr);
        auto lhs = std::get<0>(expression);
        auto rhs = std::get<1>(expression);

        if (stack.contains(lhs))
            lhs_s = stack[lhs];
        else if (ir::Table::is_rvalue_data_type(lhs))
            lhs_s = ir::Table::get_symbol_type_size_from_rvalue_string(lhs);
        else if (symbols.is_defined(rhs))
            lhs_s = symbols.get_symbol_by_name(rhs);
        else
            lhs_s = get_accumulator_register_from_size(Operand_Size::Dword);

        if (stack.contains(rhs))
            rhs_s = stack[rhs];
        else if (ir::Table::is_rvalue_data_type(rhs))
            rhs_s = ir::Table::get_symbol_type_size_from_rvalue_string(rhs);
        else
            rhs_s = get_accumulator_register_from_size(Operand_Size::Dword);
        if (temporary_expansion.has_value()) {
            auto storage = temporary_expansion.value();
            add_inst_s(instructions_, mov, Operand_Size::Dword, storage, lhs_s);
            Storage_Operands operands = { storage, rhs_s };
            auto inst = from_storage_relational_expression(
                operands, Operand_Size::Dword, std::get<2>(expression));
            detail::insert_inst(instructions_, inst.second);
        } else {
            Storage_Operands operands = { lhs_s, rhs_s };
            auto inst = from_storage_relational_expression(
                operands, Operand_Size::Dword, std::get<2>(expression));
            detail::insert_inst(instructions_, inst.second);
        }
    } else if (ir::Table::is_rvalue_data_type(expr)) {
        auto imm = ir::Table::get_symbol_type_size_from_rvalue_string(expr);
        add_inst_ll(instructions_, mov, Operand_Size::Dword, eax, imm);
    } else {
        auto imm = symbols.get_symbol_by_name(expr);
        add_inst_ll(instructions_, mov, Operand_Size::Dword, eax, imm);
    }
}

void Code_Generator::from_return_ita(Storage const& dest)
{
    add_inst_lr(instructions_, mov, Operand_Size::Dword, dest, eax);
}

void Code_Generator::from_leave_ita()
{
    if (current_frame == "main")
        add_inst_lrs(instructions_, xor_, Operand_Size::Dword, eax, eax);
    add_inst_ld(instructions_, pop, Operand_Size::Qword, rbp);
}

void Code_Generator::from_label_ita(ITA_Inst const& inst)
{
    ir::Table::Label label =
        ir::Table::get_label_as_human_readable(std::get<1>(inst));
    instructions_.emplace_back(label);
}

Code_Generator::Instruction_Pair
Code_Generator::from_storage_arithmetic_expression(
    Storage_Operands& operands,
    Operand_Size size,
    std::string const& binary_op)
{
    Instruction_Pair instructions{ Register::eax, {} };
    m::match(binary_op)(
        // cppcheck-suppress syntaxError
        m::pattern | std::string{ "*" } =
            [&] { instructions = mul(size, operands.first, operands.second); },
        m::pattern | std::string{ "/" } =
            [&] { instructions = div(size, operands.first, operands.second); },
        m::pattern | std::string{ "-" } =
            [&] { instructions = sub(size, operands.first, operands.second); },
        m::pattern | std::string{ "+" } =
            [&] { instructions = add(size, operands.first, operands.second); },
        m::pattern | std::string{ "%" } =
            [&] { instructions = mod(size, operands.first, operands.second); });
    return instructions;
}

Code_Generator::Instruction_Pair
Code_Generator::from_storage_relational_expression(
    Storage_Operands& operands,
    Operand_Size size,
    std::string const& binary_op)
{
    Instruction_Pair instructions{ Register::eax, {} };
    m::match(binary_op)(
        // cppcheck-suppress syntaxError
        m::pattern | std::string{ "==" } =
            [&] { instructions = r_eq(size, operands.first, operands.second); },
        m::pattern | std::string{ "!=" } =
            [&] {
                instructions = r_neq(size, operands.first, operands.second);
            },
        m::pattern | std::string{ "<" } =
            [&] { instructions = r_gt(size, operands.first, operands.second); },
        m::pattern | std::string{ ">" } =
            [&] { instructions = r_lt(size, operands.first, operands.second); },
        m::pattern | std::string{ "<=" } =
            [&] { instructions = r_le(size, operands.first, operands.second); },
        m::pattern | std::string{ ">=" } =
            [&] {
                instructions = r_ge(size, operands.first, operands.second);
            });

    return instructions;
}

} // namespace x86_64
