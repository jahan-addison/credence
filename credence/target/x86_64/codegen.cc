#include "codegen.h"
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

namespace credence::target::x86_64 {

namespace m = matchit;

void Code_Generator::emit(std::ostream& os)
{
    build_from_ita_table();
    for (auto const& inst : instructions_) {
        std::visit(
            util::overload{
                [&](detail::Instruction const& s) {
                    Mnemonic inst = std::get<0>(s);
                    os << "    " << inst;
                    os << suffix[std::get<1>(s)];
                    Storage dest = std::get<2>(s);
                    Storage src = std::get<3>(s);
                    if (!std::holds_alternative<std::monostate>(dest))
                        os << " " << emit_storage_device(dest) << ", ";
                    if (!std::holds_alternative<std::monostate>(src))
                        os << emit_storage_device(src);
                    os << std::endl;
                },
                [&](ir::Table::Label const& s) {
                    os << s << ":" << std::endl;
                } },
            inst);
    }
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
    auto registers =
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
    ir::Table::RValue rhs = table_->get_rvalue_from_mov_instruction(inst).first;
    if (!util::contains(lhs, "_t") and
        util::substring_count_of(rhs, " ") == 2) {
        if (is_binary_math_operator(rhs)) {
            auto inst_pair = from_ita_binary_arithmetic_expression(inst);
            detail::insert_inst(instructions_, inst_pair.second);
            add_inst_s(
                instructions_,
                mov,
                Operand_Size::Dword,
                inst_pair.first,
                Register::eax);
        }
        if (is_relation_binary_operators(rhs)) {
            auto inst_pair = from_ita_trivial_relational_expression(inst);
            detail::insert_inst(instructions_, inst_pair.second);
            add_inst_s(
                instructions_,
                mov,
                Operand_Size::Dword,
                inst_pair.first,
                Register::eax);
        }
    } else {
        if (!util::contains(lhs, "_t")) {
            CREDENCE_ASSERT(stack.contains(lhs));
            auto symbols = table_->get_stack_frame_symbols();
            Storage storage = stack[lhs];
            auto imm = symbols.get_symbol_by_name(lhs);
            add_inst_s(instructions_, mov, Operand_Size::Dword, storage, imm);
        }
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

Code_Generator::RValue_Operands
Code_Generator::resolve_immediate_operands_from_table(
    Immediate_Operands const& imm_value)
{
    RValue_Operands result = ir::Table::NULL_RVALUE_LITERAL;
    auto symbols = table_->get_stack_frame_symbols();
    std::visit(
        util::overload{
            [&](ir::Table::Binary_Expression const& s) {
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
            [&](ir::Table::RValue_Data_Type const& s) { result = s; },
            [&](ir::Table::LValue const& s) {
                if (util::contains(s, "_t")) {
                    auto temp = table_->from_temporary_lvalue(s);
                    if (util::substring_count_of(temp, " ") == 2) {
                        auto temp_r =
                            table_->from_rvalue_binary_expression(temp);
                        result = resolve_immediate_operands_from_table(temp_r);
                    }
                } else {
                    result = symbols.get_symbol_by_name(s);
                }
            } },
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

Code_Generator::Instruction_Pair
Code_Generator::from_ita_binary_arithmetic_expression(
    ir::ITA::Quadruple const& inst)
{
    CREDENCE_ASSERT(std::get<0>(inst) == ir::ITA::Instruction::MOV);
    Instruction_Pair instructions{ Register::eax, {} };
    auto operands = operands_from_binary_ita_operands(inst);
    m::match(operands.first)(
        // cppcheck-suppress syntaxError
        m::pattern | std::string{ "*" } =
            [&] {
                auto size = get_size_from_table_rvalue(
                    std::get<detail::Immediate>(operands.second.first));
                instructions =
                    mul(size, operands.second.first, operands.second.second);
            },
        m::pattern | std::string{ "/" } =
            [&] {
                auto size = get_size_from_table_rvalue(
                    std::get<detail::Immediate>(operands.second.first));
                instructions =
                    div(size, operands.second.first, operands.second.second);
            },
        m::pattern | std::string{ "-" } =
            [&] {
                auto size = get_size_from_table_rvalue(
                    std::get<detail::Immediate>(operands.second.first));
                instructions =
                    sub(size, operands.second.first, operands.second.second);
            },
        m::pattern | std::string{ "+" } =
            [&] {
                auto size = get_size_from_table_rvalue(
                    std::get<detail::Immediate>(operands.second.first));
                instructions =
                    add(size, operands.second.first, operands.second.second);
            },
        m::pattern | std::string{ "%" } =
            [&] {
                auto size = get_size_from_table_rvalue(
                    std::get<detail::Immediate>(operands.second.first));
                instructions =
                    mod(size, operands.second.first, operands.second.second);
            });

    return instructions;
}

Code_Generator::Instruction_Pair
Code_Generator::from_ita_trivial_relational_expression(
    ir::ITA::Quadruple const& inst)
{
    CREDENCE_ASSERT(std::get<0>(inst) == ir::ITA::Instruction::MOV);
    Instruction_Pair instructions{ Register::eax, {} };
    auto operands = operands_from_binary_ita_operands(inst);
    m::match(operands.first)(
        // cppcheck-suppress syntaxError
        m::pattern | std::string{ "==" } =
            [&] {
                auto size = get_size_from_table_rvalue(
                    std::get<detail::Immediate>(operands.second.first));
                instructions =
                    r_eq(size, operands.second.first, operands.second.second);
            },
        m::pattern | std::string{ "!=" } =
            [&] {
                auto size = get_size_from_table_rvalue(
                    std::get<detail::Immediate>(operands.second.first));
                instructions =
                    r_neq(size, operands.second.first, operands.second.second);
            },
        m::pattern | std::string{ "<" } =
            [&] {
                auto size = get_size_from_table_rvalue(
                    std::get<detail::Immediate>(operands.second.first));
                instructions =
                    r_gt(size, operands.second.first, operands.second.second);
            },
        m::pattern | std::string{ ">" } =
            [&] {
                auto size = get_size_from_table_rvalue(
                    std::get<detail::Immediate>(operands.second.first));
                instructions =
                    r_lt(size, operands.second.first, operands.second.second);
            },
        m::pattern | std::string{ "<=" } =
            [&] {
                auto size = get_size_from_table_rvalue(
                    std::get<detail::Immediate>(operands.second.first));
                instructions =
                    r_le(size, operands.second.first, operands.second.second);
            },
        m::pattern | std::string{ ">=" } =
            [&] {
                auto size = get_size_from_table_rvalue(
                    std::get<detail::Immediate>(operands.second.first));
                instructions =
                    r_ge(size, operands.second.first, operands.second.second);
            });

    return instructions;
}

} // namespace x86_64
