/*
 * Copyright (c) Jahan Addison
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "generator.h"
#include "instructions.h" // for get_operand_size_from_rvalue_datatype, Imm...
#include <credence/assert.h>        // for CREDENCE_ASSERT
#include <credence/ir/ita.h>        // for ITA
#include <credence/ir/table.h>      // for Table
#include <credence/target/target.h> // for align_up_to_16
#include <credence/types.h>         // for LValue, RValue, Data_Type, ...
#include <credence/util.h>          // for substring_count_of, contains
#include <cstddef>                  // for size_t
#include <deque>                    // for deque
#include <format>                   // for format
#include <functional>               // for std::function
#include <matchit.h>                // for pattern, PatternHelper, Pattern...
#include <memory>                   // for shared_ptr
#include <ostream>                  // for basic_ostream, operator<<, endl
#include <sstream>                  // for basic_stringstream
#include <string_view>              // for basic_string_view
#include <tuple>                    // for get, tuple
#include <type_traits>              // for underlying_type_t
#include <utility>                  // for pair, make_pair
#include <variant>                  // for get, holds_alternative, monostate

#define ir_i(name) credence::ir::Instruction::name

namespace credence::target::x86_64 {

namespace m = matchit;

void Code_Generator::emit(std::ostream& os)
{
    build();
    os << std::endl;
    os << ".intel_syntax noprefix" << std::endl;
    os << std::endl;
    // clang-format off
    for (auto const& inst : instructions_) {
        std::visit(
            util::overload{
            [&](detail::Instruction const& s) {
                Mnemonic inst = std::get<0>(s);
                os << "    " << inst;
                Storage dest = std::get<1>(s);
                Storage src = std::get<2>(s);
                if (!is_variant(std::monostate, dest))
                    os << " " << emit_storage_device(dest);
                if (!is_variant(std::monostate, src))
                    os << ", " << emit_storage_device(src);
                os << std::endl;
            },
            [&](type::semantic::Label const& s) {
                os << s << ":" << std::endl;
            } },
                inst);
    }
    // clang-format on
    os << std::endl;
}

void emit(
    std::ostream& os,
    util::AST_Node const& symbols,
    util::AST_Node const& ast)
{
    auto ita = ir::ITA{ symbols };
    auto instructions = ita.build_from_definitions(ast);
    auto table =
        std::make_unique<ir::Table>(ir::Table{ symbols, instructions });
    table->build_vector_definitions_from_globals(ita.globals_);
    table->build_from_ita_instructions();
    auto generator = Code_Generator{ std::move(table) };
    generator.emit(os);
}

std::string Code_Generator::emit_storage_device(Storage const& storage)
{
    std::string sloc;
    std::visit(
        util::overload{
            [&]([[maybe_unused]] std::monostate s) {},
            [&](detail::Stack::Offset s) {
                auto size = stack.get_operand_size_from_offset(s);
                CREDENCE_ASSERT(size != Operand_Size::Empty);
                std::string prefix = m::match(size)(
                    m::pattern |
                        Operand_Size::Qword = [&] { return "qword ptr"; },
                    m::pattern |
                        Operand_Size::Dword = [&] { return "dword ptr"; },
                    m::pattern |
                        Operand_Size::Word = [&] { return "word ptr"; },
                    m::pattern |
                        Operand_Size::Byte = [&] { return "byte ptr"; },
                    m::pattern | m::_ = [&] { return "dword ptr"; });

                sloc = std::format("{} [rbp - {}]", prefix, s);
            },
            [&](Register s) {
                std::stringstream ss{};
                ss << s;
                sloc = ss.str();
            },
            [&](Immediate const& s) { sloc = std::get<0>(s); } },
        storage);
    return sloc;
}

void Code_Generator::build()
{
    auto instructions = table_->instructions;
    ita_index = 0UL;
    for (; ita_index < instructions.size(); ita_index++) {
        auto inst = instructions[ita_index];
        ir::Instruction ita_inst = std::get<0>(inst);
        m::match(ita_inst)(
            m::pattern | ir_i(FUNC_START) =
                [&] {
                    auto symbol = std::get<1>(instructions.at(ita_index - 1));
                    auto name = type::get_label_as_human_readable(symbol);
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

void Code_Generator::from_func_start_ita(type::semantic::Label const& name)
{
    CREDENCE_ASSERT(table_->functions.contains(name));
    stack.clear();
    current_frame = name;
    set_table_stack_frame(name);
    auto frame = table_->functions[current_frame];
    auto stack_alloc = credence::target::align_up_to_16(frame->allocation);
    adiild(instructions_, push, rbp);
    addiilrs(instructions_, mov_, rbp, rsp);
    if (table_->stack_frame_contains_ita_instruction(
            current_frame, ir::Instruction::CALL)) {
        auto imm = detail::make_u32_int_immediate(stack_alloc);
        addiill(instructions_, sub, rsp, imm);
    }
}

Code_Generator::Storage Code_Generator::get_storage_device(Operand_Size size)
{
    auto& registers = size == Operand_Size::Qword ? available_qword_register
                                                  : available_dword_register;
    if (registers.size() > 0) {
        Storage storage = registers.front();
        registers.pop_front();
        return storage;
    } else {
        return stack.allocate(size);
    }
}

void Code_Generator::set_table_stack_frame(type::semantic::Label const& name)
{
    table_->set_stack_frame(name);
}

void Code_Generator::from_func_end_ita()
{
    auto frame = table_->functions[current_frame];
    auto stack_alloc = credence::target::align_up_to_16(frame->allocation);
    if (table_->stack_frame_contains_ita_instruction(
            current_frame, ir::Instruction::CALL)) {
        auto a_imm = detail::make_u32_int_immediate(stack_alloc);
        addiill(instructions_, add, rsp, a_imm);
    }
    reset_avail_registers();
}

void Code_Generator::from_push_ita(IR_Instruction const& inst)
{
    auto lvalue = std::get<1>(inst);
    auto frame = table_->functions[current_frame];
    auto& locals = table_->get_stack_frame_symbols();
    auto symbol = locals.get_symbol_by_name(lvalue);
    auto storage = get_storage_device();

    addiis(instructions_, mov, storage, symbol);
}

void Code_Generator::from_locl_ita(IR_Instruction const& inst)
{
    type::semantic::LValue lvalue = std::get<1>(inst);
    stack.make(lvalue);
}

void Code_Generator::from_cmp_ita([[maybe_unused]] IR_Instruction const& inst)
{
    table_->set_stack_frame(current_frame);
}

void Code_Generator::from_mov_ita(IR_Instruction const& inst)
{
    type::semantic::LValue lhs = std::get<1>(inst);
    auto symbols = table_->get_stack_frame_symbols();

    auto rhs = get_rvalue_from_mov_qaudruple(inst).first;

    if (type::is_temporary(lhs)) {
        insert_from_temporary_lvalue(lhs);
        return;
    }

    m::match(rhs)(
        m::pattern | m::app(is_immediate, true) =
            [&] {
                auto imm = type::get_rvalue_datatype_from_string(rhs);
                stack.set_address_from_immediate(lhs, imm);
                Storage lhs_storage = stack.get(lhs).first;
                addiis(instructions_, mov, lhs_storage, imm);
            },
        m::pattern | m::app(is_temporary, true) =
            [&] {
                auto acc = get_accumulator_register_from_size();
                stack.set_address_from_accumulator(lhs, acc);
                Storage lhs_storage = stack.get(lhs).first;
                addiis(instructions_, mov, lhs_storage, acc);
            },
        m::pattern | m::app(is_address, true) =
            [&] {
                Storage lhs_storage = stack.get(lhs).first;
                Storage rhs_storage = stack.get(rhs).first;
                auto acc =
                    get_accumulator_register_from_size(stack.get(rhs).second);
                addiis(instructions_, mov, acc, rhs_storage);
                addiis(instructions_, mov, lhs_storage, acc);
            },
        m::pattern | m::app(type::is_unary_operator, true) =
            [&] {
                Storage lhs_storage = stack.get(lhs).first;
                auto unary_op = type::get_unary_operator(rhs);
                from_ita_unary_expression(unary_op, lhs_storage);
            },
        m::pattern | m::app(type::is_binary_expression, true) =
            [&] { from_binary_operator_expression(rhs); },
        m::pattern | m::_ = [&] { credence_error("unreachable"); });
}

void Code_Generator::from_temporary_unary_operator_expression(
    type::semantic::RValue const& expr)
{
    CREDENCE_ASSERT(type::is_unary_operator(expr));
    auto op = type::get_unary_operator(expr);
    type::semantic::RValue rvalue = type::get_unary_rvalue_reference(expr);
    if (stack.contains(rvalue)) {
        auto size = stack.get(rvalue).second;
        auto acc = next_instruction_is_temporary() and
                           not last_instruction_is_assignment()
                       ? get_second_register_from_size(size)
                       : get_accumulator_register_from_size(size);
        addiis(instructions_, mov, acc, stack.get(rvalue).first);
        from_ita_unary_expression(op, acc);
    } else {
        auto immediate = type::get_rvalue_datatype_from_string(rvalue);
        auto size = detail::get_operand_size_from_rvalue_datatype(immediate);
        auto acc = next_instruction_is_temporary() and
                           not last_instruction_is_assignment()
                       ? get_second_register_from_size(size)
                       : get_accumulator_register_from_size(size);
        addiis(instructions_, mov, acc, immediate);
        from_ita_unary_expression(op, acc);
    }
}

Code_Generator::Storage Code_Generator::get_storage_for_binary_operator(
    type::semantic::RValue const& rvalue)
{
    if (type::is_rvalue_data_type(rvalue)) {
        return type::get_rvalue_datatype_from_string(rvalue);
    }
    if (stack.contains(rvalue))
        return stack.get(rvalue).first;

    return get_accumulator_register_from_size();
}

inline auto get_rvalue_pair_as_immediate(
    type::semantic::RValue const& lhs,
    type::semantic::RValue const& rhs)
{
    return std::make_pair(
        type::get_rvalue_datatype_from_string(lhs),
        type::get_rvalue_datatype_from_string(rhs));
}

void Code_Generator::from_binary_operator_expression(
    type::semantic::LValue const& expr)
{
    CREDENCE_ASSERT(type::is_binary_expression(expr));
    auto expression = type::from_rvalue_binary_expression(expr);
    Storage lhs_s{ std::monostate{} };
    Storage rhs_s{ std::monostate{} };
    auto [lhs, rhs, op] = expression;
    auto immediate = false;
    m::match(lhs, rhs)(
        m::pattern |
            m::ds(m::app(is_immediate, true), m::app(is_immediate, true)) =
            [&] {
                auto [lhs_i, rhs_i] = get_rvalue_pair_as_immediate(lhs, rhs);
                lhs_s = lhs_i;
                rhs_s = rhs_i;
                insert_from_immediate_rvalues(lhs_i, op, rhs_i);
                immediate = true;
            },
        m::pattern | m::ds(m::app(is_address, true), m::app(is_address, true)) =
            [&] {
                if (!last_instruction_is_assignment()) {
                    lhs_s = get_storage_device(stack.get(lhs).second);
                    addiis(instructions_, mov, lhs_s, stack.get(lhs).first);
                    rhs_s = stack.get(rhs).first;
                } else {
                    lhs_s = get_accumulator_register_from_size(
                        stack.get(lhs).second);
                    addiis(instructions_, mov, lhs_s, stack.get(lhs).first);
                    rhs_s = stack.get(rhs).first;
                }
            },
        m::pattern |
            m::ds(m::app(is_temporary, true), m::app(is_temporary, true)) =
            [&] {
                auto size = detail::get_size_from_accumulator_register(
                    get_accumulator_register_from_size());
                auto acc = get_accumulator_register_from_size();
                lhs_s = acc;
                if (!immediate_stack.empty()) {
                    rhs_s = immediate_stack.back();
                    immediate_stack.pop_back();
                    if (!immediate_stack.empty()) {
                        addiis(instructions_, mov, acc, immediate_stack.back());
                        immediate_stack.pop_back();
                    }
                } else {
                    auto intermediate = get_second_register_from_size(size);
                    rhs_s = intermediate;
                }
            },
        m::pattern |
            m::ds(m::app(is_address, true), m::app(is_address, false)) =
            [&] {
                lhs_s = stack.get(lhs).first;
                rhs_s = get_storage_for_binary_operator(rhs);
                if (last_instruction_is_assignment()) {
                    auto acc = get_accumulator_register_from_size(
                        stack.get(lhs).second);
                    addiis(instructions_, mov, acc, stack.get(lhs).first);
                }
                if (is_temporary(rhs)) {
                    lhs_s = get_accumulator_register_from_size(
                        stack.get(lhs).second);
                    rhs_s = stack.get(lhs).first;
                }

                if (is_instruction_temporary()) {
                    if (type::is_bitwise_binary_operator(op)) {
                        auto storage =
                            get_storage_device(stack.get(lhs).second);
                        addiis(
                            instructions_, mov, storage, stack.get(lhs).first);
                        lhs_s = storage;
                    } else {
                        lhs_s = get_accumulator_register_from_storage(lhs_s);
                    }
                }
            },
        m::pattern |
            m::ds(m::app(is_address, false), m::app(is_address, true)) =
            [&] {
                lhs_s = get_storage_for_binary_operator(lhs);
                rhs_s = stack.get(rhs).first;
                if (last_instruction_is_assignment()) {
                    auto acc = get_accumulator_register_from_size(
                        stack.get(rhs).second);
                    addiis(instructions_, mov, acc, stack.get(rhs).first);
                }
                if (is_temporary(lhs) or is_instruction_temporary())
                    rhs_s = get_accumulator_register_from_size(
                        stack.get(rhs).second);
            },
        m::pattern |
            m::ds(m::app(is_temporary, true), m::app(is_temporary, false)) =
            [&] {
                lhs_s = get_accumulator_register_from_size();
                rhs_s = get_storage_for_binary_operator(rhs);
            },
        m::pattern |
            m::ds(m::app(is_temporary, false), m::app(is_temporary, true)) =
            [&] {
                lhs_s = get_storage_for_binary_operator(lhs);
                rhs_s = get_accumulator_register_from_size();
            },
        m::pattern | m::ds(m::_, m::_) =
            [&] {
                lhs_s = get_storage_for_binary_operator(lhs);
                rhs_s = get_storage_for_binary_operator(rhs);
            });
    if (!immediate) {
        Storage_Operands operands = { lhs_s, rhs_s };
        insert_from_op_operands(operands, op);
    }
}

void Code_Generator::insert_from_op_operands(
    Storage_Operands& operands,
    std::string const& op)
{
    if (is_variant(Immediate, operands.first))
        std::swap(operands.first, operands.second);
    if (type::is_binary_arithmetic_operator(op)) {
        auto inst = from_arithmetic_expression_operands(operands, op);
        detail::insert(instructions_, inst.second);
    } else if (type::is_relation_binary_operator(op)) {
        auto inst = from_relational_expression_operands(operands, op);
        detail::insert(instructions_, inst.second);
    } else if (type::is_bitwise_binary_operator(op)) {
        auto inst = from_bitwise_expression_operands(operands, op);
        detail::insert(instructions_, inst.second);
    } else if (type::is_unary_operator(op)) {
        from_ita_unary_expression(op, operands.first);
    } else {
        credence_error(std::format("unreachable: operator {}", op));
    }
}

void Code_Generator::insert_from_temporary_lvalue(
    type::semantic::LValue const& lvalue)
{
    auto temporary = table_->from_temporary_lvalue(lvalue);
    if (type::is_binary_expression(temporary)) {
        from_binary_operator_expression(temporary);
    } else if (type::is_unary_operator(temporary)) {
        from_temporary_unary_operator_expression(temporary);
    } else if (type::is_rvalue_data_type(temporary)) {
        auto imm = type::get_rvalue_datatype_from_string(temporary);
        addiill(instructions_, mov, eax, imm);
    } else {
        auto symbols = table_->get_stack_frame_symbols();
        auto imm = symbols.get_symbol_by_name(temporary);
        addiill(instructions_, mov, eax, imm);
    }
}

constexpr Code_Generator::Register
Code_Generator::get_accumulator_register_from_size(Operand_Size size)
{
    namespace m = matchit;
    if (special_register != Register::eax) {
        auto special = special_register;
        special_register = Register::eax;
        return special;
    }
    return m::match(size)(
        m::pattern | Operand_Size::Qword = [&] { return Register::rax; },
        m::pattern | Operand_Size::Word = [&] { return Register::ax; },
        m::pattern | Operand_Size::Byte = [&] { return Register::al; },
        m::pattern | m::_ = [&] { return Register::eax; });
}

constexpr Code_Generator::Register
Code_Generator::get_second_register_from_size(Operand_Size size)
{
    namespace m = matchit;
    return m::match(size)(
        m::pattern | Operand_Size::Qword = [&] { return Register::rdi; },
        m::pattern | Operand_Size::Word = [&] { return Register::di; },
        m::pattern | Operand_Size::Byte = [&] { return Register::dil; },
        m::pattern | m::_ = [&] { return Register::edi; });
}

constexpr Code_Generator::Register
Code_Generator::get_accumulator_register_from_storage(Storage const& storage)
{
    namespace m = matchit;
    Register accumulator{};
    std::visit(
        util::overload{
            [&](std::monostate) { accumulator = Register::eax; },
            [&](detail::Stack_Offset const& offset) {
                auto size = stack.get_operand_size_from_offset(offset);
                accumulator = get_accumulator_register_from_size(size);
            },

            [&](Register const& device) { accumulator = device; },
            [&](Immediate const& immediate) {
                auto size =
                    detail::get_operand_size_from_rvalue_datatype(immediate);
                accumulator = get_accumulator_register_from_size(size);
            }

        },
        storage);
    return accumulator;
}

void Code_Generator::insert_from_immediate_rvalues(
    Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs)
{
    if (type::is_binary_arithmetic_operator(op)) {
        auto imm =
            detail::get_result_from_trivial_integral_expression(lhs, op, rhs);
        auto acc = get_accumulator_register_from_size(
            detail::get_operand_size_from_rvalue_datatype(lhs));
        addiis(instructions_, mov, acc, imm);
        return;
    }

    if (type::is_relation_binary_operator(op)) {
        auto imm =
            detail::get_result_from_trivial_relational_expression(lhs, op, rhs);
        auto acc = get_accumulator_register_from_size(Operand_Size::Byte);
        special_register = acc;
        addiis(instructions_, mov, acc, imm);
        return;
    }

    if (type::is_bitwise_binary_operator(op)) {
        auto imm =
            detail::get_result_from_trivial_bitwise_expression(lhs, op, rhs);
        auto acc = get_accumulator_register_from_size(
            detail::get_operand_size_from_rvalue_datatype(lhs));
        if (!is_instruction_temporary())
            addiis(instructions_, mov, acc, imm);
        else
            immediate_stack.emplace_back(imm);
        return;
    }

    credence_error("unreachable");
}

Code_Generator::Immediate Code_Generator::get_from_immediate_rvalues(
    Storage& lhs,
    std::string const& op,
    Storage& rhs)
{
    auto imm_l = std::get<Immediate>(lhs);
    auto imm_r = std::get<Immediate>(rhs);

    if (type::is_binary_arithmetic_operator(op))
        return detail::get_result_from_trivial_integral_expression(
            imm_l, op, imm_r);
    if (type::is_relation_binary_operator(op))
        return detail::get_result_from_trivial_relational_expression(
            imm_l, op, imm_r);
    if (type::is_bitwise_binary_operator(op))
        return detail::get_result_from_trivial_bitwise_expression(
            imm_l, op, imm_r);

    credence_error("unreachable");
    return detail::make_int_immediate<int>(0);
}

void Code_Generator::from_ita_unary_expression(
    std::string const& op,
    Storage const& dest)
{
    Instruction_Pair instructions{ Register::eax, {} };
    m::match(op)(
        m::pattern |
            std::string{ "++" } = [&] { instructions = detail::inc(dest); },
        m::pattern |
            std::string{ "--" } = [&] { instructions = detail::dec(dest); },
        m::pattern |
            std::string{ "~" } = [&] { instructions = detail::b_not(dest); },
        m::pattern |
            std::string{ "-" } = [&] { instructions = detail::neg(dest); },
        m::pattern | std::string{ "+" } = [&] {});
    detail::insert(instructions_, instructions.second);
}

void Code_Generator::from_return_ita(Storage const& dest)
{
    addiilr(instructions_, mov, dest, eax);
}

void Code_Generator::from_leave_ita()
{
    if (current_frame == "main")
        addiilrs(instructions_, xor_, eax, eax);
    adiild(instructions_, pop, rbp);
    addiie(instructions_, ret);
}

void Code_Generator::from_label_ita(IR_Instruction const& inst)
{
    instructions_.emplace_back(
        type::get_label_as_human_readable(std::get<1>(inst)));
}

Code_Generator::Instruction_Pair
Code_Generator::from_arithmetic_expression_operands(
    Storage_Operands const& operands,
    std::string const& binary_op)
{
    Instruction_Pair instructions{ Register::eax, {} };
    m::match(binary_op)(
        // cppcheck-suppress syntaxError
        m::pattern | std::string{ "*" } =
            [&] {
                instructions = detail::mul(operands.first, operands.second);
            },
        m::pattern | std::string{ "/" } =
            [&] {
                auto storage = get_storage_device(Operand_Size::Dword);
                addiis(instructions.second, mov, storage, operands.first);
                instructions = detail::div(storage, operands.second);
            },
        m::pattern | std::string{ "-" } =
            [&] {
                instructions = detail::sub(operands.first, operands.second);
            },
        m::pattern | std::string{ "+" } =
            [&] {
                instructions = detail::add(operands.first, operands.second);
            },
        m::pattern | std::string{ "%" } =
            [&] {
                auto storage = get_storage_device(Operand_Size::Dword);
                special_register = Register::edx;
                addiis(instructions.second, mov, storage, operands.first);
                instructions = detail::mod(storage, operands.second);
            });
    return instructions;
}

Code_Generator::Instruction_Pair
Code_Generator::from_bitwise_expression_operands(
    Storage_Operands const& operands,
    std::string const& binary_op)
{
    Instruction_Pair instructions{ Register::eax, {} };
    m::match(binary_op)(
        // cppcheck-suppress syntaxError
        m::pattern | std::string{ "<<" } =
            [&] {
                instructions = detail::lshift(operands.first, operands.second);
            },
        m::pattern | std::string{ ">>" } =
            [&] {
                instructions = detail::rshift(operands.first, operands.second);
            },
        m::pattern | std::string{ "^" } =
            [&] {
                auto acc =
                    get_accumulator_register_from_storage(operands.first);
                instructions = detail::b_xor(acc, operands.second);
            },
        m::pattern | std::string{ "&" } =
            [&] {
                instructions = detail::b_and(operands.first, operands.second);
            },
        m::pattern | std::string{ "|" } =
            [&] {
                instructions = detail::b_or(operands.first, operands.second);
            });
    return instructions;
}

Code_Generator::Instruction_Pair
Code_Generator::from_relational_expression_operands(
    Storage_Operands const& operands,
    std::string const& binary_op)
{
    Instruction_Pair instructions{ Register::eax, {} };
    m::match(binary_op)(
        // cppcheck-suppress syntaxError
        m::pattern | std::string{ "==" } =
            [&] {
                instructions = detail::r_eq(operands.first, operands.second);
            },
        m::pattern | std::string{ "!=" } =
            [&] {
                instructions = detail::r_neq(operands.first, operands.second);
            },
        m::pattern | std::string{ "<" } =
            [&] {
                instructions = detail::r_gt(operands.first, operands.second);
            },
        m::pattern | std::string{ ">" } =
            [&] {
                instructions = detail::r_lt(operands.first, operands.second);
            },
        m::pattern | std::string{ "<=" } =
            [&] {
                instructions = detail::r_le(operands.first, operands.second);
            },
        m::pattern | std::string{ ">=" } =
            [&] {
                instructions = detail::r_ge(operands.first, operands.second);
            });

    return instructions;
}

constexpr detail::Stack::Entry detail::Stack::get(LValue const& lvalue)
{
    return stack_address[lvalue];
}

constexpr void detail::Stack::make(LValue const& lvalue)
{
    stack_address[lvalue] = { 0, Operand_Size::Empty };
}

constexpr detail::Stack::Offset detail::Stack::allocate(Operand_Size operand)
{
    auto alloc = get_size_from_operand_size(operand);
    size += alloc;
    return size;
}

constexpr detail::Stack::Entry detail::Stack::get(Offset offset)
{
    auto find = std::find_if(
        stack_address.begin(), stack_address.end(), [&](Pair const& entry) {
            return entry.second.first == offset;
        });
    if (find == stack_address.end())
        return { 0, Operand_Size::Empty };
    else
        return find->second;
}

constexpr detail::Operand_Size detail::Stack::get_operand_size_from_offset(
    Offset offset)
{
    return std::accumulate(
        stack_address.begin(),
        stack_address.end(),
        Operand_Size::Empty,
        [&](Operand_Size size, Pair const& entry) {
            if (entry.second.first == offset)
                return entry.second.second;
            return size;
        });
}

constexpr void detail::Stack::set_address_from_immediate(
    LValue const& lvalue,
    Immediate const& rvalue)
{
    auto operand_size = detail::get_operand_size_from_rvalue_datatype(rvalue);
    auto value_size = detail::get_size_from_operand_size(operand_size);
    if (stack_address[lvalue].second != Operand_Size::Empty)
        return;
    size += value_size;
    stack_address[lvalue].first = size;
    stack_address[lvalue].second = operand_size;
}

constexpr void detail::Stack::set_address_from_accumulator(
    LValue const& lvalue,
    Register acc)
{
    auto register_size = detail::get_size_from_accumulator_register(acc);
    if (stack_address[lvalue].second != Operand_Size::Empty)
        return;
    size += detail::get_size_from_operand_size(register_size);
    stack_address[lvalue].first = size;
    stack_address[lvalue].second = register_size;
}

} // namespace x86_64