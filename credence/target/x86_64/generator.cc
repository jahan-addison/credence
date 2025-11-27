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
#include "instructions.h"   // for get_operand_size_from_rvalue_datatype, Imm...
#include <credence/error.h> // for credence_assert
#include <credence/ir/ita.h>        // for ITA
#include <credence/ir/table.h>      // for Table
#include <credence/target/target.h> // for align_up_to_16
#include <credence/types.h>         // for LValue, RValue, Data_Type, ...
#include <credence/util.h>          // for substring_count_of, contains
#include <cstddef>                  // for size_t
#include <deque>                    // for deque
#include <fmt/compile.h>            // for fmt::literals
#include <fmt/format.h>             // for format
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
    build_instructions();
    os << std::endl;
    os << ".intel_syntax noprefix" << std::endl;
    os << std::endl;

    for (std::size_t index = 0; index < instructions_.size(); index++) {
        auto inst = instructions_[index];
        // clang-format off
        std::visit(
            util::overload{
                [&](detail::Instruction const& s) {
                    auto [mnemonic, dest, src] = s;

                    auto flags = instruction_flag.contains(index)
                                     ? instruction_flag[index]
                                     : 0UL;

                    os << detail::tabwidth(4) << mnemonic;

                    auto size = get_operand_size_from_storage(src);
                    if (!is_empty_storage(dest)) {
                        if (flags & detail::flag::Address)
                            size = get_operand_size_from_storage(dest);
                        os << " " << emit_storage_device(dest, size, flags);
                        if (flags & detail::flag::Indirect and
                            is_variant(Register, src))
                            flags &= ~detail::flag::Indirect;
                    }

                    if (!is_empty_storage(src)) {
                        if (flags & detail::flag::Indirect_Source)
                            flags |= detail::flag::Indirect;
                        os << ", " << emit_storage_device(src, size, flags);
                        flags &=
                            ~(detail::flag::Indirect |
                              detail::flag::Indirect_Source);
                    }

                    os << std::endl;
                },

                [&](type::semantic::Label const& s) {
                    os << s << ":" << std::endl;
                } },
            inst);
        // clang-format on
    }
    os << std::endl;
}

// clang-format off
void emit(std::ostream& os,
    util::AST_Node const& symbols,
    util::AST_Node const& ast)
{
    // clang-format on
    auto ita = ir::ITA{ symbols };
    auto instructions = ita.build_from_definitions(ast);
    auto table =
        std::make_unique<ir::Table>(ir::Table{ symbols, instructions });
    table->build_vector_definitions_from_globals(ita.globals_);
    table->build_from_ita_instructions();
    auto generator = Code_Generator{ std::move(table) };
    generator.emit(os);
}

constexpr std::string storage_prefix_from_operand_size(
    detail::Operand_Size size)
{
    return m::match(size)(
        m::pattern | detail::Operand_Size::Qword = [&] { return "qword ptr"; },
        m::pattern | detail::Operand_Size::Dword = [&] { return "dword ptr"; },
        m::pattern | detail::Operand_Size::Word = [&] { return "word ptr"; },
        m::pattern | detail::Operand_Size::Byte = [&] { return "byte ptr"; },
        m::pattern | m::_ = [&] { return "dword ptr"; });
}

constexpr std::string detail::emit_immediate_storage(Immediate const& immediate)
{
    return type::get_value_from_rvalue_data_type(immediate);
}

constexpr std::string detail::emit_stack_storage(
    Stack const& stack,
    Stack::Offset offset,
    flag::flags flags)
{
    using namespace fmt::literals;
    std::string as_str{};
    auto size = stack.get_operand_size_from_offset(offset);
    std::string prefix = storage_prefix_from_operand_size(size);
    if (flags & flag::Address)
        as_str = fmt::format("[rbp - {}]"_cf, offset);
    else
        as_str = fmt::format("{} [rbp - {}]"_cf, prefix, offset);
    return as_str;
}

inline std::string detail::emit_register_storage(
    Register device,
    Operand_Size size,
    flag::flags flags)
{
    std::stringstream as_str{};
    using namespace fmt::literals;
    auto prefix = storage_prefix_from_operand_size(size);
    if (flags & flag::Indirect)
        as_str << fmt::format(
            "{} [{}]"_cf, prefix, get_storage_as_string(device));
    else
        as_str << device;

    return as_str.str();
}

std::string Code_Generator::emit_storage_device(
    Storage const& storage,
    Operand_Size size,
    detail::flag::flags flags)
{
    m::Id<detail::Stack::Offset> s;
    m::Id<Register> r;
    m::Id<Immediate> i;
    return m::match(storage)(
        m::pattern | m::as<detail::Stack::Offset>(s) =
            [&] { return detail::emit_stack_storage(stack, *s, flags); },
        m::pattern | m::as<Register>(r) =
            [&] { return detail::emit_register_storage(*r, size, flags); },
        m::pattern | m::as<Immediate>(i) =
            [&] { return detail::emit_immediate_storage(*i); });
}

void Code_Generator::build_instructions()
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
    credence_assert(table_->functions.contains(name));
    stack.clear();
    current_frame = name;
    set_table_stack_frame(name);
    auto frame = table_->functions[current_frame];
    auto stack_alloc = credence::target::align_up_to_16(frame->allocation);
    add_i_ld(instructions_, push, rbp);
    add_i_llrs(instructions_, mov_, rbp, rsp);
    if (table_->stack_frame_contains_ita_instruction(
            current_frame, ir::Instruction::CALL)) {
        auto imm = detail::make_u32_int_immediate(stack_alloc);
        add_i_ll(instructions_, sub, rsp, imm);
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
        add_i_ll(instructions_, add, rsp, a_imm);
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

    add_i_s(instructions_, mov, storage, symbol);
}

void Code_Generator::from_locl_ita(IR_Instruction const& inst)
{
    auto locl_lvalue = std::get<1>(inst);
    if (type::is_dereference_expression(locl_lvalue)) {
        auto lvalue = type::get_unary_rvalue_reference(std::get<1>(inst));
        stack.set_address_from_address(lvalue);
        return;
    }
    auto frame = table_->functions[current_frame];
    auto& locals = table_->get_stack_frame_symbols();
    // immediate relational rvalue, the storage will be the al register
    if (type::is_relation_binary_expression(
            type::get_value_from_rvalue_data_type(
                locals.get_symbol_by_name(locl_lvalue)))) {
        stack.set_address_from_accumulator(locl_lvalue, Register::al);
    } else {
        // get storage size from type table
        auto type = table_->get_type_from_rvalue_data_type(locl_lvalue);
        stack.set_address_from_type(locl_lvalue, type);
    }
}

void Code_Generator::from_cmp_ita([[maybe_unused]] IR_Instruction const& inst)
{
}

void Code_Generator::from_mov_ita(IR_Instruction const& inst)
{
    auto lhs = std::get<1>(inst);
    auto rhs = get_rvalue_from_mov_qaudruple(inst).first;
    auto symbols = table_->get_stack_frame_symbols();

    if (type::is_temporary(lhs)) {
        insert_from_temporary_lvalue(lhs);
        return;
    }

    if (type::is_unary_expression(lhs) and type::is_unary_expression(rhs)) {
        insert_from_unary_to_unary_assignment(lhs, rhs);
        return;
    }

    m::match(rhs)(
        m::pattern | m::app(is_immediate, true) =
            [&] {
                auto imm = type::get_rvalue_datatype_from_string(rhs);
                Storage lhs_storage = get_lvalue_address(lhs);
                add_i_s(instructions_, mov, lhs_storage, imm);
            },
        m::pattern | m::app(is_temporary, true) =
            [&] {
                if (address_ir_assignment) {
                    address_ir_assignment = false;
                    Storage lhs_storage = stack.get(lhs).first;
                    add_i_s(instructions_, mov, lhs_storage, Register::rax);
                } else {
                    auto acc = get_accumulator_register_from_size();
                    if (!type::is_unary_expression(lhs))
                        stack.set_address_from_accumulator(lhs, acc);
                    Storage lhs_storage = get_lvalue_address(lhs);
                    add_i_s(instructions_, mov, lhs_storage, acc);
                }
            },
        m::pattern | m::app(is_address, true) =
            [&] {
                credence_assert(stack.get(rhs).second != Operand_Size::Empty);
                Storage lhs_storage = stack.get(lhs).first;
                Storage rhs_storage = stack.get(rhs).first;
                auto acc =
                    get_accumulator_register_from_size(stack.get(rhs).second);
                add_i_s(instructions_, mov, acc, rhs_storage);
                add_i_s(instructions_, mov, lhs_storage, acc);
            },
        m::pattern | m::app(type::is_unary_expression, true) =
            [&] {
                Storage lhs_storage = get_lvalue_address(lhs);
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
    credence_assert(type::is_unary_expression(expr));
    auto op = type::get_unary_operator(expr);
    type::semantic::RValue rvalue = type::get_unary_rvalue_reference(expr);
    if (stack.contains(rvalue)) {
        if (op == "&") {
            address_ir_assignment = true;
            from_ita_unary_expression(op, stack.get(rvalue).first);
            special_register = Register::rax;
            return;
        }
        auto size = stack.get(rvalue).second;
        auto acc = next_ir_instruction_is_temporary() and
                           not last_ir_instruction_is_assignment()
                       ? get_second_register_from_size(size)
                       : get_accumulator_register_from_size(size);
        add_i_s(instructions_, mov, acc, stack.get(rvalue).first);
        from_ita_unary_expression(op, acc);
    } else {
        auto immediate = type::get_rvalue_datatype_from_string(rvalue);
        auto size = detail::get_operand_size_from_rvalue_datatype(immediate);
        auto acc = next_ir_instruction_is_temporary() and
                           not last_ir_instruction_is_assignment()
                       ? get_second_register_from_size(size)
                       : get_accumulator_register_from_size(size);
        add_i_s(instructions_, mov, acc, immediate);
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
    credence_assert(type::is_binary_expression(expr));
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
                if (!last_ir_instruction_is_assignment()) {
                    lhs_s = get_storage_device(stack.get(lhs).second);
                    add_i_s(instructions_, mov, lhs_s, stack.get(lhs).first);
                    rhs_s = stack.get(rhs).first;
                } else {
                    lhs_s = get_accumulator_register_from_size(
                        stack.get(lhs).second);
                    add_i_s(instructions_, mov, lhs_s, stack.get(lhs).first);
                    rhs_s = stack.get(rhs).first;
                }
            },
        m::pattern |
            m::ds(m::app(is_temporary, true), m::app(is_temporary, true)) =
            [&] {
                auto size = detail::get_operand_size_from_register(
                    get_accumulator_register_from_size());
                auto acc = get_accumulator_register_from_size();
                lhs_s = acc;
                if (!immediate_stack.empty()) {
                    rhs_s = immediate_stack.back();
                    immediate_stack.pop_back();
                    if (!immediate_stack.empty()) {
                        add_i_s(
                            instructions_, mov, acc, immediate_stack.back());
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
                if (last_ir_instruction_is_assignment()) {
                    auto acc = get_accumulator_register_from_size(
                        stack.get(lhs).second);
                    add_i_s(instructions_, mov, acc, stack.get(lhs).first);
                }
                if (is_temporary(rhs)) {
                    lhs_s = get_accumulator_register_from_size(
                        stack.get(lhs).second);
                    rhs_s = stack.get(lhs).first;
                }

                if (is_ir_instruction_temporary()) {
                    if (type::is_bitwise_binary_operator(op)) {
                        auto storage =
                            get_storage_device(stack.get(lhs).second);
                        add_i_s(
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
                if (last_ir_instruction_is_assignment()) {
                    auto acc = get_accumulator_register_from_size(
                        stack.get(rhs).second);
                    add_i_s(instructions_, mov, acc, stack.get(rhs).first);
                }
                if (is_temporary(lhs) or is_ir_instruction_temporary())
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
    } else if (type::is_unary_expression(op)) {
        from_ita_unary_expression(op, operands.first);
    } else {
        credence_error(fmt::format("unreachable: operator '{}'", op));
    }
}

void Code_Generator::insert_from_temporary_lvalue(
    type::semantic::LValue const& lvalue)
{
    auto temporary = table_->from_temporary_lvalue(lvalue);
    insert_from_rvalue(temporary);
}

void Code_Generator::insert_from_unary_to_unary_assignment(
    type::semantic::LValue const& lhs,
    type::semantic::LValue const& rhs)
{
    auto lhs_lvalue = type::get_unary_rvalue_reference(lhs);
    auto rhs_lvalue = type::get_unary_rvalue_reference(rhs);

    auto lhs_op = type::get_unary_operator(lhs);
    auto rhs_op = type::get_unary_operator(rhs);

    auto frame = table_->functions[current_frame];
    auto& locals = table_->get_stack_frame_symbols();

    m::match(lhs_op, rhs_op)(
        // *t = *k deference to dereference assignment
        m::pattern | m::ds("*", "*") = [&] {
            credence_assert(
                stack.get(lhs_lvalue).second != Operand_Size::Empty);
            credence_assert(
                stack.get(rhs_lvalue).second != Operand_Size::Empty);
            auto acc = get_accumulator_register_from_size(Operand_Size::Qword);
            Storage lhs_storage = stack.get(lhs_lvalue).first;
            Storage rhs_storage = stack.get(rhs_lvalue).first;
            auto size = detail::get_operand_size_from_type(
                type::get_type_from_rvalue_data_type(locals.get_symbol_by_name(
                    locals.get_pointer_by_name(lhs_lvalue))));
            auto temp = get_second_register_from_size(size);
            add_i_s(instructions_, mov, acc, rhs_storage);
            set_instruction_flag(
                detail::flag::Indirect_Source | detail::flag::Address);
            add_i_s(instructions_, mov, temp, acc);
            add_i_s(instructions_, mov, acc, lhs_storage);
            set_instruction_flag(detail::flag::Indirect);
            add_i_s(instructions_, mov, acc, temp);
        });
}

void Code_Generator::insert_from_rvalue(type::semantic::RValue const& rvalue)
{
    if (type::is_binary_expression(rvalue)) {
        from_binary_operator_expression(rvalue);
    } else if (type::is_unary_expression(rvalue)) {
        from_temporary_unary_operator_expression(rvalue);
    } else if (type::is_rvalue_data_type(rvalue)) {
        auto imm = type::get_rvalue_datatype_from_string(rvalue);
        add_i_ll(instructions_, mov, eax, imm);
    } else {
        auto symbols = table_->get_stack_frame_symbols();
        auto imm = symbols.get_symbol_by_name(rvalue);
        add_i_ll(instructions_, mov, eax, imm);
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
        add_i_s(instructions_, mov, acc, imm);
        return;
    }

    if (type::is_relation_binary_operator(op)) {
        auto imm =
            detail::get_result_from_trivial_relational_expression(lhs, op, rhs);
        auto acc = get_accumulator_register_from_size(Operand_Size::Byte);
        special_register = acc;
        add_i_s(instructions_, mov, acc, imm);
        return;
    }

    if (type::is_bitwise_binary_operator(op)) {
        auto imm =
            detail::get_result_from_trivial_bitwise_expression(lhs, op, rhs);
        auto acc = get_accumulator_register_from_size(
            detail::get_operand_size_from_rvalue_datatype(lhs));
        if (!is_ir_instruction_temporary())
            add_i_s(instructions_, mov, acc, imm);
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
    Storage const& dest,
    Storage const& src)
{
    Instruction_Pair instructions{ Register::eax, {} };
    m::match(op)(
        m::pattern |
            std::string{ "++" } = [&] { instructions = detail::inc(dest); },
        m::pattern |
            std::string{ "--" } = [&] { instructions = detail::dec(dest); },
        m::pattern |
            std::string{ "~" } = [&] { instructions = detail::b_not(dest); },
        m::pattern | std::string{ "&" } =
            [&] {
                set_instruction_flag(detail::flag::Address);
                auto acc =
                    get_accumulator_register_from_size(Operand_Size::Qword);
                instructions = detail::lea(acc, dest);
            },
        m::pattern | std::string{ "*" } =
            [&] {
                auto acc = get_accumulator_register_from_storage(dest);
                add_i_s(instructions.second, mov, acc, dest);
                set_instruction_flag(detail::flag::Indirect);
                add_i_s(instructions.second, mov, acc, src);
            },
        m::pattern |
            std::string{ "-" } = [&] { instructions = detail::neg(dest); },
        m::pattern | std::string{ "+" } = [&] {});
    detail::insert(instructions_, instructions.second);
}

void Code_Generator::from_return_ita(Storage const& dest)
{
    add_i_llr(instructions_, mov, dest, eax);
}

void Code_Generator::from_leave_ita()
{
    if (current_frame == "main")
        add_i_llrs(instructions_, xor_, eax, eax);
    add_i_ld(instructions_, pop, rbp);
    add_i_e(instructions_, ret);
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
                add_i_s(instructions.second, mov, storage, operands.first);
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
                add_i_s(instructions.second, mov, storage, operands.first);
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

void Code_Generator::set_instruction_flag(
    detail::flag::Instruction_Flag set_flag)
{
    if (!instructions_.empty()) {
        unsigned int index = instructions_.size();
        if (!instruction_flag.contains(index))
            instruction_flag[index] = set_flag;
        else
            instruction_flag[index] |= set_flag;
    }
}

void Code_Generator::set_instruction_flag(detail::flag::flags flags)
{
    if (!instructions_.empty()) {
        unsigned int index = instructions_.size();
        if (!instruction_flag.contains(index))
            instruction_flag[index] = flags;
        else
            instruction_flag[index] |= flags;
    }
}

detail::Operand_Size Code_Generator::get_operand_size_from_storage(
    Storage const& storage)
{
    namespace m = matchit;
    m::Id<detail::Stack::Offset> s;
    m::Id<Immediate> i;
    m::Id<Register> r;
    return m::match(storage)(
        m::pattern | m::as<detail::Stack::Offset>(s) =
            [&] { return stack.get_operand_size_from_offset(*s); },
        m::pattern | m::as<Immediate>(i) =
            [&] { return detail::get_operand_size_from_rvalue_datatype(*i); },
        m::pattern | m::as<Register>(r) =
            [&] { return detail::get_operand_size_from_register(*r); },
        m::pattern | m::_ = [&] { return Operand_Size::Empty; });
}

Code_Generator::Storage Code_Generator::get_lvalue_address(
    type::semantic::LValue const& lvalue)
{
    if (type::is_unary_expression(lvalue)) {
        auto storage =
            stack.get(type::get_unary_rvalue_reference(lvalue)).first;
        add_i_s(instructions_, mov, Register::rax, storage);
        set_instruction_flag(detail::flag::Indirect);
        return Register::rax;
    } else {
        return stack.get(lvalue).first;
    }
}

constexpr detail::Stack::Entry detail::Stack::get(LValue const& lvalue)
{
    return stack_address[lvalue];
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

constexpr detail::Operand_Size detail::Stack::get_operand_size_from_offset(
    Offset offset) const
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
    if (stack_address[lvalue].second != Operand_Size::Empty)
        return;
    auto operand_size = get_operand_size_from_rvalue_datatype(rvalue);
    auto value_size = get_size_from_operand_size(operand_size);
    size += value_size;
    stack_address.insert(lvalue, { size, operand_size });
}

constexpr void detail::Stack::set_address_from_accumulator(
    LValue const& lvalue,
    Register acc)
{
    if (stack_address[lvalue].second != Operand_Size::Empty)
        return;
    auto register_size = get_operand_size_from_register(acc);
    size += detail::get_size_from_operand_size(register_size);
    stack_address.insert(lvalue, { size, register_size });
}

constexpr void detail::Stack::set_address_from_type(
    LValue const& lvalue,
    Type type)
{
    if (stack_address[lvalue].second != Operand_Size::Empty)
        return;

    auto operand_size = get_operand_size_from_type(type);
    auto value_size = get_size_from_operand_size(operand_size);

    if (get_lvalue_from_offset(size + value_size).empty()) {
        size += value_size;
        stack_address.insert(lvalue, { size, operand_size });
    } else {
        size = size + value_size + value_size;
    }
}

constexpr void detail::Stack::set_address_from_address(LValue const& lvalue)
{
    auto qword_size = Operand_Size::Qword;
    size = align_up_to_8(size + detail::get_size_from_operand_size(qword_size));
    stack_address.insert(lvalue, { size, qword_size });
}

constexpr void detail::Stack::set_address_from_stack(
    LValue const& lhs,
    LValue const& rhs)
{
    if (stack_address[lhs].second != Operand_Size::Empty)
        return;
    auto operand_size = get(rhs).second;
    size += detail::get_size_from_operand_size(operand_size);
    stack_address.insert(lhs, { size, operand_size });
}

constexpr inline std::string detail::Stack::get_lvalue_from_offset(
    Offset offset) const
{
    auto search = std::ranges::find_if(
        stack_address.begin(), stack_address.end(), [&](Pair const& pair) {
            return pair.second.first == offset;
        });
    if (search != stack_address.end())
        return search->first;
    else
        return "";
}

} // namespace x86_64