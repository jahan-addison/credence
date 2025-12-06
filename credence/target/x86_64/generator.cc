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
#include "instructions.h"      // for Operand_Size, add_inst_as, Register
#include "lib.h"               // for STDLIB_FUNCTIONS
#include "syscall.h"           // for syscall
#include <algorithm>           // for find_if, __find_if
#include <credence/error.h>    // for credence_assert, assert_nequal_impl
#include <credence/ir/ita.h>   // for Instruction, ITA, get_rvalue_fro...
#include <credence/ir/table.h> // for Table, Function
#include <credence/map.h>      // for Ordered_Map
#include <credence/types.h>    // for is_unary_expression, get_rvalue_...
#include <credence/util.h>     // for overload, AST_Node, is_variant
#include <cstddef>             // for size_t
#include <deque>               // for deque
#include <fmt/compile.h>       // for format, operator""_cf
#include <fmt/format.h>        // for format
#include <matchit.h>           // for pattern, Id, App, PatternHelper
#include <memory>              // for shared_ptr, make_unique, unique_ptr
#include <numeric>             // for accumulate
#include <ostream>             // for ostream, stringstream
#include <string_view>         // for basic_string_view
#include <tuple>               // for get, tuple
#include <utility>             // for pair, get, make_pair, move
#include <variant>             // for variant, visit, get, monostate

#define ir_i(name) credence::ir::Instruction::name

namespace credence::target::x86_64 {

namespace m = matchit;

/**
 * @brief Emit a complete set of x86-64 instructions
 */
void Code_Generator::emit(std::ostream& os)
{
    emit_syntax_directive(os);
    emit_data_section(os);
    if (!data_.empty())
        detail::newline(os);
    emit_text_section(os);
    detail::newline(os);
}

/**
 * @brief Emit the intel syntax directive
 */
inline void Code_Generator::emit_syntax_directive(std::ostream& os)
{
    os << std::endl << ".intel_syntax noprefix" << std::endl << std::endl;
}

/**
 * @brief Construct and emit the data section of a program
 */
void Code_Generator::emit_data_section(std::ostream& os)
{
    build_data_section_instructions();
    os << detail::Directive::data;
    detail::newline(os, 2);
    if (!data_.empty())
        for (std::size_t index = 0; index < data_.size(); index++) {
            auto data_item = data_[index];
            std::visit(
                util::overload{
                    [&](type::semantic::Label const& s) {
                        os << s << ":" << std::endl;
                    },
                    [&](detail::Data_Pair const& s) {
                        os << detail::tabwidth(4) << s.first;
                        if (s.first == Directive::asciz)
                            os << " " << "\"" << s.second << "\"";
                        else
                            os << " " << s.second;
                        detail::newline(os);
                        if (index < data_.size() - 1)
                            detail::newline(os);
                    },
                },
                data_item);
        }
}

/**
 * @brief Construct and emit the text section of a program
 */
void Code_Generator::emit_text_section(std::ostream& os)
{
    os << detail::Directive::text << std::endl;
    os << detail::tabwidth(4) << detail::Directive::start;
    detail::newline(os, 2);

    build_text_section_instructions();

    for (std::size_t index = 0; index < instructions_.size(); index++) {
        auto inst = instructions_[index];
        std::visit(
            util::overload{
                [&](detail::Instruction const& s) {
                    auto [mnemonic, dest, src] = s;
                    auto flags = instruction_flag.contains(index)
                                     ? instruction_flag[index]
                                     : 0UL;

                    os << detail::tabwidth(4) << mnemonic;

                    auto size = get_operand_size_from_storage(src);
                    // apply any flags to the dest storage device
                    if (!is_empty_storage(dest)) {
                        if (flags & detail::flag::Address)
                            size = get_operand_size_from_storage(dest);
                        os << " " << emit_storage_device(dest, size, flags);
                        if (flags & detail::flag::Indirect and
                            is_variant(Register, src))
                            flags &= ~detail::flag::Indirect;
                    }
                    // apply any flags to the source storage device
                    if (!is_empty_storage(src)) {
                        if (flags & detail::flag::Alloc)
                            src = detail::make_u32_int_immediate(
                                stack.get_stack_frame_allocation_size());
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
                    os << detail::make_label(s) << ":" << std::endl;
                } },
            inst);
    }
}

/**
 * @brief Code Generator factory
 *
 * Construct and emit a complete x86-64 source program from an AST and symbols
 */
void emit(std::ostream& os, util::AST_Node& symbols, util::AST_Node const& ast)
{
    auto [globals, instructions] = ir::make_ITA_instructions(symbols, ast);
    auto table = std::make_unique<ir::Table>(
        ir::Table{ symbols, instructions, globals });
    table->build_from_ita_instructions();
    auto generator = Code_Generator{ std::move(table) };
    generator.emit(os);
}

/**
 * @brief Get the intel-format prefix for storage device sizes
 */
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

/**
 * @brief Emit from a type::Data_Type as an immediate value
 */
constexpr std::string detail::emit_immediate_storage(Immediate const& immediate)
{
    return type::get_value_from_rvalue_data_type(immediate);
}

/**
 * @brief Emit an offset on the Stack as a storage device as a string
 */
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

/**
 * @brief Emit a register as a storage device as a string
 */
constexpr std::string detail::emit_register_storage(
    Register device,
    Operand_Size size,
    flag::flags flags)
{
    std::string as_str{};
    using namespace fmt::literals;
    auto prefix = storage_prefix_from_operand_size(size);
    if (flags & flag::Indirect)
        as_str =
            fmt::format("{} [{}]"_cf, prefix, get_storage_as_string(device));
    else
        as_str = detail::register_as_string(device);

    return as_str;
}

/**
 * @brief Emit a storage device as a string
 */
constexpr std::string Code_Generator::emit_storage_device(
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

/**
 * @brief Construct the text section of a program
 */
void Code_Generator::build_text_section_instructions()
{
    auto instructions = table->instructions;
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
                    set_stack_frame_from_table(name);
                    from_func_start_ita(name);
                },
            m::pattern | ir_i(FUNC_END) = [&] { from_func_end_ita(); },
            m::pattern | ir_i(MOV) = [&] { from_mov_ita(inst); },
            m::pattern | ir_i(PUSH) = [&] { from_push_ita(); },
            m::pattern | ir_i(CALL) = [&] { from_call_ita(inst); },
            m::pattern | ir_i(LOCL) = [&] { from_locl_ita(inst); },
            m::pattern | ir_i(RETURN) = [&] { from_return_ita(); },
            m::pattern | ir_i(LEAVE) = [&] { from_leave_ita(); },
            m::pattern | ir_i(LABEL) = [&] { from_label_ita(inst); });
    }
}
/**
 * @brief Construct the data section of a program
 */
void Code_Generator::build_data_section_instructions()
{
    for (auto const& string : table->strings) {
        auto data_instruction = detail::asciz(&constant_index, string);
        string_storage.insert_or_assign(string, data_instruction.first);
        detail::insert(data_, data_instruction.second);
    }
    build_data_section_from_globals();
}

void Code_Generator::build_data_section_from_globals()
{
    for (auto const& global : table->globals.get_pointers()) {
        credence_assert(table->vectors.contains(global));
        auto& vector = table->vectors.at(global);
        data_.emplace_back(global);
        auto address = type::semantic::Address{ 0 };
        for (auto const& item : vector->data) {
            auto directive =
                detail::get_data_directive_from_rvalue_type(item.second);
            auto data = type::get_value_from_rvalue_data_type(item.second);
            vector->set_address_offset(item.first, address);
            address += detail::get_size_from_operand_size(
                detail::get_operand_size_from_rvalue_datatype(item.second));
            Directives instructions{};
            m::match(directive)(
                m::pattern | Directive::quad =
                    [&] {
                        credence_assert(string_storage.contains(data));
                        instructions = detail::quad(string_storage.at(data));
                    },
                m::pattern | Directive::long_ =
                    [&] { instructions = detail::long_(data); },
                m::pattern | Directive::float_ =
                    [&] { instructions = detail::float_(data); },
                m::pattern | Directive::double_ =
                    [&] { instructions = detail::double_(data); },
                m::pattern | Directive::byte_ =
                    [&] { instructions = detail::byte_(data); });
            detail::insert(data_, instructions);
        }
    }
}

/**
 * @brief Translate from the IR Instruction::FUNC_START
 *
 *    Generates code for the Function Prologue
 */
void Code_Generator::from_func_start_ita(type::semantic::Label const& name)
{
    credence_assert(table->functions.contains(name));
    stack.clear();
    current_frame = name;
    set_stack_frame_from_table(name);
    auto frame = table->functions[current_frame];
    // function prologue
    add_inst_ld(instructions_, push, rbp);
    add_inst_llrs(instructions_, mov_, rbp, rsp);
    if (table->stack_frame_contains_ita_instruction(
            current_frame, ir::Instruction::CALL)) {
        auto imm = detail::make_u32_int_immediate(0);
        set_instruction_flag(detail::flag::Alloc);
        add_inst_ll(instructions_, sub, rsp, imm);
    }
}

/**
 * @brief Get an available storage device, reset for each stack frame
 */
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

/**
 * @brief Set the current stack frame from the address in the Table
 */
void Code_Generator::set_stack_frame_from_table(
    type::semantic::Label const& name)
{
    table->set_stack_frame(name);
}

/**
 * @brief Translate from the IR Instruction::FUNC_END
 */
void Code_Generator::from_func_end_ita()
{
    auto frame = table->functions[current_frame];
    if (table->stack_frame_contains_ita_instruction(
            current_frame, ir::Instruction::CALL)) {
        auto imm = detail::make_u32_int_immediate(0);
        if (current_frame != "main") {
            set_instruction_flag(detail::flag::Alloc);
            add_inst_ll(instructions_, add, rsp, imm);
        }
    }
    reset_avail_registers();
}

/**
 * @brief Translate from the IR Instruction::LOCL
 *
 * Note that at this point in code translation, we allocate local variables on
 * the stack
 */
void Code_Generator::from_locl_ita(ir::Quadruple const& inst)
{
    auto locl_lvalue = std::get<1>(inst);

    auto frame = table->functions[current_frame];
    auto& locals = table->get_stack_frame_symbols();

    // The storage of an immediate (and, really, all) relational
    // expression will be the `al` register, 1 for true, 0 for false
    Operand_Lambda is_immediate_relational_expression =
        [&](RValue const& rvalue) {
            return type::is_relation_binary_expression(
                type::get_value_from_rvalue_data_type(
                    locals.get_symbol_by_name(rvalue)));
        };

    m::match(locl_lvalue)(
        // Allocate a pointer on the stack
        m::pattern | m::app(type::is_dereference_expression, true) =
            [&] {
                auto lvalue =
                    type::get_unary_rvalue_reference(std::get<1>(inst));
                stack.set_address_from_address(lvalue);
            },
        // Allocate a vector (array), including all of its elements on the stack
        m::pattern | m::app(is_vector, true) =
            [&] {
                auto vector = table->vectors.at(locl_lvalue);
                auto size = stack.get_stack_size_from_table_vector(*vector);
                stack.set_address_from_size(locl_lvalue, size);
            },
        // Allocate 1 byte on the stack for the al register
        m::pattern | m::app(is_immediate_relational_expression, true) =
            [&] {
                stack.set_address_from_accumulator(locl_lvalue, Register::al);
            },
        // Allocate on the stack from the type size of the lvalue
        m::pattern | m::_ =
            [&] {
                auto type = table->get_type_from_rvalue_data_type(locl_lvalue);
                stack.set_address_from_type(locl_lvalue, type);
            }

    );
}

void Code_Generator::from_push_ita()
{
    call_size++;
}

type::Stack Code_Generator::get_arguments_from_call_stack()
{
    type::Stack reorder{};
    for (; call_size != 0UL;) {
        auto rvalue = table->from_temporary_lvalue(table->stack.front());
        reorder.emplace_front(rvalue);
        table->stack.pop_front();
        call_size--;
    }
    return reorder;
}

void Code_Generator::from_call_ita(ir::Quadruple const& inst)
{
    auto function_name = type::get_label_as_human_readable(std::get<1>(inst));
    if (library::is_syscall_function(function_name)) {
        syscall::syscall_arguments_t arguments{};
        auto parameters = get_arguments_from_call_stack();
        for (auto const& rvalue : parameters) {
            if (type::is_rvalue_data_type(rvalue)) {
                auto immediate = type::get_rvalue_datatype_from_string(rvalue);
                if (type::is_rvalue_data_type_string(immediate)) {
                    auto str_address = detail::make_asciz_immediate(
                        string_storage[type::get_value_from_rvalue_data_type(
                            immediate)]);
                    arguments.emplace_back(str_address);
                } else
                    arguments.emplace_back(
                        type::get_rvalue_datatype_from_string(rvalue));
            } else {
                arguments.emplace_back(get_lvalue_address(rvalue));
            }
        }
        syscall::common::make_syscall(instructions_, function_name, arguments);
    }
}

void Code_Generator::from_cmp_ita([[maybe_unused]] ir::Quadruple const& inst) {}
void Code_Generator::from_if_ita([[maybe_unused]] ir::Quadruple const& inst) {}
void Code_Generator::from_jmp_e_ita([[maybe_unused]] ir::Quadruple const& inst)
{
}

/**
 * @brief Translate from the IR Instruction::MOV
 *
 * The Instruction::MOV assigns an lvalue to an rvalue expression
 */
void Code_Generator::from_mov_ita(ir::Quadruple const& inst)
{
    auto lhs = ir::get_lvalue_from_mov_qaudruple(inst);
    auto rhs = ir::get_rvalue_from_mov_qaudruple(inst).first;

    m::match(lhs, rhs)(
        // The stack frame is prepared in ir::Table, so skip ita parameters
        m::pattern | m::ds(m::app(is_parameter, true), m::_) = [] {},
        // Translate an rvalue from a mutual-recursive temporary lvalue
        m::pattern | m::ds(m::app(type::is_temporary, true), m::_) =
            [&] { insert_from_temporary_lvalue(lhs); },
        // Translate a unary-to-unary rvalue reference
        m::pattern | m::ds(
                         m::app(type::is_unary_expression, true),
                         m::app(type::is_unary_expression, true)) =
            [&] { insert_from_unary_to_unary_assignment(lhs, rhs); },
        // Translate from a vector in global scope
        m::pattern | m::ds(m::app(is_global_vector, true), m::_) =
            [&] { insert_from_global_vector_assignment(lhs, rhs); },
        m::pattern | m::ds(m::_, m::app(is_global_vector, true)) =
            [&] { insert_from_global_vector_assignment(lhs, rhs); },
        // Direct operand to mnemonic translation
        m::pattern | m::_ = [&] { insert_from_mnemonic_operand(lhs, rhs); });
}

/**
 * @brief Mnemonic operand pattern matching to code generation
 */
void Code_Generator::insert_from_mnemonic_operand(
    LValue const& lhs,
    RValue const& rhs)
{
    m::match(rhs)(
        // Translate from an immediate value assignment
        m::pattern | m::app(is_immediate, true) =
            [&] {
                auto imm = type::get_rvalue_datatype_from_string(rhs);
                Storage lhs_storage = get_lvalue_address(lhs);
                if (type::get_type_from_rvalue_data_type(imm) == "string") {
                    from_ita_string(type::get_value_from_rvalue_data_type(imm));
                    add_inst_as(instructions_, mov, lhs_storage, Register::rax);
                } else
                    add_inst_as(instructions_, mov, lhs_storage, imm);
            },
        // The storage of the rvalue from `insert_from_temporary_lvalue` in the
        // function above is in the accumulator register, use it to assign
        // to a local address
        m::pattern | m::app(is_temporary, true) =
            [&] {
                if (address_ir_assignment) {
                    address_ir_assignment = false;
                    Storage lhs_storage = stack.get(lhs).first;
                    add_inst_as(instructions_, mov, lhs_storage, Register::rax);
                } else {
                    auto acc = get_accumulator_register_from_size();
                    if (!type::is_unary_expression(lhs))
                        stack.set_address_from_accumulator(lhs, acc);
                    Storage lhs_storage = get_lvalue_address(lhs);
                    add_inst_as(instructions_, mov, lhs_storage, acc);
                }
            },
        // Local-to-local variable assignment
        m::pattern | m::app(is_address, true) =
            [&] {
                credence_assert(stack.get(rhs).second != Operand_Size::Empty);
                Storage lhs_storage = stack.get(lhs).first;
                Storage rhs_storage = stack.get(rhs).first;
                auto acc =
                    get_accumulator_register_from_size(stack.get(rhs).second);
                add_inst_as(instructions_, mov, acc, rhs_storage);
                add_inst_as(instructions_, mov, lhs_storage, acc);
            },
        // Unary expression assignment, including pointers to an address
        m::pattern | m::app(type::is_unary_expression, true) =
            [&] {
                Storage lhs_storage = get_lvalue_address(lhs);
                auto unary_op = type::get_unary_operator(rhs);
                from_ita_unary_expression(unary_op, lhs_storage);
            },
        // Translate from binary expressions in the IR
        m::pattern | m::app(type::is_binary_expression, true) =
            [&] { from_binary_operator_expression(rhs); },
        m::pattern | m::_ = [&] { credence_error("unreachable"); });
}

/**
 * @brief Translate from unary temporary expressions
 * See ir/temporary.h for details
 */
void Code_Generator::from_temporary_unary_operator_expression(
    RValue const& expr)
{
    credence_assert(type::is_unary_expression(expr));
    auto op = type::get_unary_operator(expr);
    RValue rvalue = type::get_unary_rvalue_reference(expr);
    if (stack.contains(rvalue)) {
        // This is the address-of operator, use a qword size register
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
        add_inst_as(instructions_, mov, acc, stack.get(rvalue).first);
        from_ita_unary_expression(op, acc);
    } else {
        auto immediate = type::get_rvalue_datatype_from_string(rvalue);
        auto size = detail::get_operand_size_from_rvalue_datatype(immediate);
        auto acc = next_ir_instruction_is_temporary() and
                           not last_ir_instruction_is_assignment()
                       ? get_second_register_from_size(size)
                       : get_accumulator_register_from_size(size);
        add_inst_as(instructions_, mov, acc, immediate);
        from_ita_unary_expression(op, acc);
    }
}

/**
 * @brief Get the storage device of an IR binary expression operand
 */
Code_Generator::Storage Code_Generator::get_storage_for_binary_operator(
    RValue const& rvalue)
{
    if (type::is_rvalue_data_type(rvalue))
        return type::get_rvalue_datatype_from_string(rvalue);

    if (stack.contains(rvalue))
        return stack.get(rvalue).first;

    return get_accumulator_register_from_size();
}

/**
 * @brief Construct a pair of Immediates from 2 RValue's
 */
inline auto get_rvalue_pair_as_immediate(
    detail::Stack::RValue const& lhs,
    detail::Stack::RValue const& rhs)
{
    return std::make_pair(
        type::get_rvalue_datatype_from_string(lhs),
        type::get_rvalue_datatype_from_string(rhs));
}

/**
 * @brief Translate a string in the table to an .asciz directive
 *
 * 'string_storage' holds the %rip offset in the data section
 */
void Code_Generator::from_ita_string(RValue const& str)
{
    credence_assert(string_storage.contains(str));
    auto location = detail::make_asciz_immediate(string_storage[str]);
    add_inst_ll(instructions_, lea, rax, location);
}

/**
 * @brief Translate from binary operator expression operand types
 *
 *   Note that we pattern match on special pair cases
 */
void Code_Generator::from_binary_operator_expression(RValue const& expr)
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
                    add_inst_as(
                        instructions_, mov, lhs_s, stack.get(lhs).first);
                    rhs_s = stack.get(rhs).first;
                } else {
                    lhs_s = get_accumulator_register_from_size(
                        stack.get(lhs).second);
                    add_inst_as(
                        instructions_, mov, lhs_s, stack.get(lhs).first);
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
                        add_inst_as(
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
                    add_inst_as(instructions_, mov, acc, stack.get(lhs).first);
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
                        add_inst_as(
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
                    add_inst_as(instructions_, mov, acc, stack.get(rhs).first);
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

/**
 * @brief Translate from storage device operands and the operator type
 *
 * Note that the source storage device may be empty
 */
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

/**
 * @brief Translate from the rvalue at a temporary lvalue location
 */
void Code_Generator::insert_from_temporary_lvalue(LValue const& lvalue)
{
    auto temporary = table->from_temporary_lvalue(lvalue);
    insert_from_rvalue(temporary);
}

/**
 * @brief Translate from an rvalue expression
 *
 * Note that the storage is usually an accumulator register
 * to be assigned an address on the stack
 */
void Code_Generator::insert_from_rvalue(RValue const& rvalue)
{
    if (type::is_binary_expression(rvalue)) {
        from_binary_operator_expression(rvalue);
    } else if (type::is_unary_expression(rvalue)) {
        from_temporary_unary_operator_expression(rvalue);
    } else if (type::is_rvalue_data_type(rvalue)) {
        auto imm = type::get_rvalue_datatype_from_string(rvalue);
        add_inst_ll(instructions_, mov, eax, imm);
    } else {
        if (rvalue == "RET") {
            call_size = 0;
            if (table->functions[current_frame]->ret.empty())
                return;
        } else {
            auto symbols = table->get_stack_frame_symbols();
            auto imm = symbols.get_symbol_by_name(rvalue);
            add_inst_ll(instructions_, mov, eax, imm);
        }
    }
}

/**
 * @brief Translate vector assignment between global vectors
 */
void Code_Generator::insert_from_global_vector_assignment(
    LValue const& lhs,
    LValue const& rhs)
{

    auto lhs_storage = get_lvalue_address(lhs);
    auto rhs_storage = get_lvalue_address(rhs);

    auto acc = get_accumulator_register_from_storage(lhs_storage);
    add_inst_as(instructions_, mov, acc, rhs_storage);
    add_inst_as(instructions_, mov, lhs_storage, acc);
}

/**
 * @brief Translate from unary-to-unary rvalue expressions
 *
 * The only supported type is dereferenced pointers
 */
void Code_Generator::insert_from_unary_to_unary_assignment(
    LValue const& lhs,
    LValue const& rhs)
{
    auto lhs_lvalue = type::get_unary_rvalue_reference(lhs);
    auto rhs_lvalue = type::get_unary_rvalue_reference(rhs);

    auto lhs_op = type::get_unary_operator(lhs);
    auto rhs_op = type::get_unary_operator(rhs);

    auto frame = table->functions[current_frame];
    auto& locals = table->get_stack_frame_symbols();

    m::match(lhs_op, rhs_op)(
        // *t = *k deference to dereference assignment
        m::pattern | m::ds("*", "*") = [&] {
            credence_assert_nequal(
                stack.get(lhs_lvalue).second, Operand_Size::Empty);
            credence_assert_nequal(
                stack.get(rhs_lvalue).second, Operand_Size::Empty);

            auto acc = get_accumulator_register_from_size(Operand_Size::Qword);
            Storage lhs_storage = stack.get(lhs_lvalue).first;
            Storage rhs_storage = stack.get(rhs_lvalue).first;
            auto size = detail::get_operand_size_from_type(
                type::get_type_from_rvalue_data_type(locals.get_symbol_by_name(
                    locals.get_pointer_by_name(lhs_lvalue))));
            auto temp = get_second_register_from_size(size);

            add_inst_as(instructions_, mov, acc, rhs_storage);
            set_instruction_flag(
                detail::flag::Indirect_Source | detail::flag::Address);
            add_inst_as(instructions_, mov, temp, acc);
            add_inst_as(instructions_, mov, acc, lhs_storage);
            set_instruction_flag(detail::flag::Indirect);
            add_inst_as(instructions_, mov, acc, temp);
        });
}

/**
 * @brief Get a accumulator register from an Operand Size
 *
 * See instructions.h for size details
 */
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

/**
 * @brief Get a second accumulator register from an Operand Size
 *
 * See instructions.h for size details
 */
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

/**
 * @brief Get an accumulator register that will fit a storage device
 *
 * See instructions.h for storage details
 */
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

/**
 * @brief Translate and compute trivial immediate binary expressions
 */
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
        add_inst_as(instructions_, mov, acc, imm);
        return;
    }

    if (type::is_relation_binary_operator(op)) {
        auto imm =
            detail::get_result_from_trivial_relational_expression(lhs, op, rhs);
        auto acc = get_accumulator_register_from_size(Operand_Size::Byte);
        special_register = acc;
        add_inst_as(instructions_, mov, acc, imm);
        return;
    }

    if (type::is_bitwise_binary_operator(op)) {
        auto imm =
            detail::get_result_from_trivial_bitwise_expression(lhs, op, rhs);
        auto acc = get_accumulator_register_from_size(
            detail::get_operand_size_from_rvalue_datatype(lhs));
        if (!is_ir_instruction_temporary())
            add_inst_as(instructions_, mov, acc, imm);
        else
            immediate_stack.emplace_back(imm);
        return;
    }

    credence_error("unreachable");
}

/**
 * @brief Translate from the ir unary expression types
 */
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
                add_inst_as(instructions.second, mov, acc, dest);
                set_instruction_flag(detail::flag::Indirect);
                add_inst_as(instructions.second, mov, acc, src);
            },
        m::pattern |
            std::string{ "-" } = [&] { instructions = detail::neg(dest); },
        m::pattern | std::string{ "+" } = [&] {});
    detail::insert(instructions_, instructions.second);
}

/**
 * @brief Translate from the IR Instruction::RET
 */
void Code_Generator::from_return_ita()
{
    auto frame = table->functions[current_frame];
    if (!frame->ret.empty())
        insert_from_rvalue(frame->ret);
}

/**
 * @brief Translate from the IR Instruction::LEAVE
 *
 *  Generates code for the Function Epilogue
 */
void Code_Generator::from_leave_ita()
{
    if (current_frame == "main") {
        syscall::common::exit_syscall(instructions_, 0);
    } else {
        // function epilogue
        add_inst_llrs(instructions_, xor_, eax, eax);
        add_inst_ld(instructions_, pop, rbp);
        add_inst_e(instructions_, ret);
    }
}

/**
 * @brief Translate from the IR Instruction::LABEL
 */
void Code_Generator::from_label_ita(ir::Quadruple const& inst)
{
    instructions_.emplace_back(
        type::get_label_as_human_readable(std::get<1>(inst)));
}

/**
 * @brief Translate arithmetic binary expressions
 * Instruction_Pair includes the destination storage device
 *
 * See instructions.h for details
 */
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
                add_inst_as(instructions.second, mov, storage, operands.first);
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
                add_inst_as(instructions.second, mov, storage, operands.first);
                instructions = detail::mod(storage, operands.second);
            });
    return instructions;
}

/**
 * @brief Translate bitwise binary expressions
 * Instruction_Pair includes the destination storage device
 *
 * See instructions.h for details
 */
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

/**
 * @brief Translate relational binary expressions
 * Instruction_Pair includes the destination storage device
 *
 * See instructions.h for details
 */
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

/**
 * @brief Set a flag on the current instruction to be used during emission
 */
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

/**
 * @brief Set flags on the current instruction to be used during emission
 */
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

/**
 * @brief Get the operand size (word size) of a storage device
 *
 * See instructions.h for storage details
 */
detail::Operand_Size Code_Generator::get_operand_size_from_storage(
    Storage const& storage)
{
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

/**
 * @brief Get the %rip offset of a vector index and operand size
 *
 *   Note: Out-of-range is a compiletime error
 */
Code_Generator::Vector_Entry_Pair Code_Generator::get_rip_offset_address(
    LValue const& lvalue,
    RValue const& offset)
{
    auto vector = type::from_lvalue_offset(lvalue);
    if (!is_vector_offset(lvalue))
        return std::make_pair(
            0UL,
            detail::get_operand_size_from_rvalue_datatype(
                table->vectors.at(vector)->data.at("0")));
    if (!table->hoisted_symbols.has_key(offset) and
        not value::is_integer_string(offset))
        table->table_compiletime_error(
            fmt::format("Invalid index '{} on vector lvalue", offset), vector);
    if (table->hoisted_symbols.has_key(offset)) {
        auto index = table->get_rvalue_data_type_at_pointer(offset);
        auto key = std::string{ type::get_value_from_rvalue_data_type(index) };
        if (!table->vectors.at(vector)->data.contains(key))
            table->table_compiletime_error(
                fmt::format(
                    "Invalid out-of-range index '{}' on vector lvalue", key),
                vector);
        return std::make_pair(
            table->vectors.at(vector)->offset.at(key),
            detail::get_operand_size_from_rvalue_datatype(
                table->vectors.at(vector)->data.at(key)));
    } else if (value::is_integer_string(offset)) {
        if (!table->vectors.at(vector)->data.contains(offset))
            table->table_compiletime_error(
                fmt::format(
                    "Invalid out-of-range index '{}' on vector lvalue", offset),
                vector);
        return std::make_pair(
            table->vectors.at(vector)->offset.at(offset),
            detail::get_operand_size_from_rvalue_datatype(
                table->vectors.at(vector)->data.at(offset)));
    } else
        return std::make_pair(
            0UL,
            detail::get_operand_size_from_rvalue_datatype(
                table->vectors.at(vector)->data.at("0")));
}

/**
 * @brief Get the storage address of an lvalue
 *
 *  Note: including vector (array) and/or global vector indices
 */
Code_Generator::Storage Code_Generator::get_lvalue_address(
    LValue const& lvalue,
    bool use_prefix)
{
    auto lhs = type::from_lvalue_offset(lvalue);
    auto offset = type::from_decay_offset(lvalue);

    if (type::is_dereference_expression(lvalue)) {
        auto storage =
            stack.get(type::get_unary_rvalue_reference(lvalue)).first;
        add_inst_as(instructions_, mov, Register::rax, storage);
        set_instruction_flag(detail::flag::Indirect);
        return Register::rax;
    } else if (is_global_vector(lhs)) {
        auto vector = table->vectors.at(lhs);
        if (table->globals.is_pointer(lhs)) {
            auto rip_storage = get_rip_offset_address(lvalue, offset);
            auto prefix = storage_prefix_from_operand_size(rip_storage.second);
            auto rip_arithmetic =
                rip_storage.first == 0UL
                    ? lhs
                    : fmt::format("{}+{}", lhs, rip_storage.first);
            rip_arithmetic =
                use_prefix
                    ? fmt::format("{} [rip + {}]", prefix, rip_arithmetic)
                    : fmt::format("[rip + {}]", rip_arithmetic);
            return detail::make_array_immediate(rip_arithmetic);
        }
    } else if (is_vector_offset(lvalue)) {
        credence_assert(table->vectors.contains(lhs));
        auto vector = table->vectors.at(lhs);
        return stack.get_stack_offset_from_table_vector_index(
            lhs, offset, *vector);
    }
    return stack.get(lvalue).first;
}

/**
 * @brief Get the stack location offset and size from an lvalue
 */
constexpr detail::Stack::Entry detail::Stack::get(LValue const& lvalue)
{
    return stack_address[lvalue];
}

/**
 * @brief Get the stack location lvalue and size from an offset
 */
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

/**
 * @brief Allocate space on the stack from a word size
 *
 * See instructions.h for details
 */
constexpr detail::Stack::Offset detail::Stack::allocate(Operand_Size operand)
{
    auto alloc = get_size_from_operand_size(operand);
    size += alloc;
    return size;
}

/**
 * @brief Get the word size of an offset address
 *
 * See instructions.h for details
 */
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

/**
 * @brief Set and allocate an address from an immediate
 */
constexpr void detail::Stack::set_address_from_immediate(
    LValue const& lvalue,
    Immediate const& rvalue)
{
    if (stack_address[lvalue].second != Operand_Size::Empty)
        return;
    auto operand_size = get_operand_size_from_rvalue_datatype(rvalue);
    auto value_size = get_size_from_operand_size(operand_size);
    allocate_aligned_lvalue(lvalue, value_size, operand_size);
}

/**
 * @brief Set and allocate an address from an accumulator register size
 */
constexpr void detail::Stack::set_address_from_accumulator(
    LValue const& lvalue,
    Register acc)
{
    if (stack_address[lvalue].second != Operand_Size::Empty)
        return;
    auto register_size = get_operand_size_from_register(acc);
    auto allocation = detail::get_size_from_operand_size(register_size);
    allocate_aligned_lvalue(lvalue, allocation, register_size);
}

/**
 * @brief Set and allocate an address from a type in the Table
 */
constexpr void detail::Stack::set_address_from_type(
    LValue const& lvalue,
    Type type)
{
    if (stack_address[lvalue].second != Operand_Size::Empty)
        return;

    auto operand_size = get_operand_size_from_type(type);
    auto value_size = get_size_from_operand_size(operand_size);

    allocate_aligned_lvalue(lvalue, value_size, operand_size);
}

/**
 * @brief
 * In some cases address space was loaded in chunks for memory alignment
 *
 * So skip any previously allocated offsets as we push downwards
 */
constexpr void detail::Stack::allocate_aligned_lvalue(
    LValue const& lvalue,
    Size value_size,
    Operand_Size operand_size)
{
    if (get_lvalue_from_offset(size + value_size).empty()) {
        size += value_size;
        stack_address.insert(lvalue, { size, operand_size });
    } else {
        size = size + value_size + value_size;
    }
}

/**
 * @brief Set and allocate an address from another addres (pointer)
 *
 * Memory align to multiples of 8 bytes per the ABI
 */
constexpr void detail::Stack::set_address_from_address(LValue const& lvalue)
{
    auto qword_size = Operand_Size::Qword;
    size = util::align_up_to_8(
        size + detail::get_size_from_operand_size(qword_size));
    stack_address.insert(lvalue, { size, qword_size });
}

constexpr detail::Stack::Size detail::Stack::get_stack_frame_allocation_size()
{
    return util::align_up_to_16(size);
}

/**
 * @brief Get the stack address of an index in a vector (array)
 *
 * The vector was allocated in a chunk and we allocate each index downward
 */
constexpr detail::Stack::Size
detail::Stack::get_stack_offset_from_table_vector_index(
    LValue const& lvalue,
    std::string const& key,
    ir::detail::Vector const& vector)
{
    bool search{ false };
    auto vector_offset = get(lvalue).first;
    return std::accumulate(
        vector.data.begin(),
        vector.data.end(),
        vector_offset,
        [&](type::semantic::Size offset,
            ir::detail::Vector::Entry const& entry) {
            if (entry.first == key)
                search = true;
            if (!search)
                offset -= detail::get_size_from_operand_size(
                    detail::get_operand_size_from_rvalue_datatype(
                        entry.second));
            return offset;
        });
}

/**
 * @brief Get the size of a vector (array)
 *
 * Memory align to multiples of 16 bytes per the ABI
 */
constexpr detail::Stack::Size detail::Stack::get_stack_size_from_table_vector(
    ir::detail::Vector const& vector)
{
    // clang-format off
    auto vector_size = size + std::accumulate(
            vector.data.begin(),
            vector.data.end(),
            0UL,
    [&](type::semantic::Size offset,
        ir::detail::Vector::Entry const& entry) {
        return offset +
            detail::get_size_from_operand_size(
                detail::get_operand_size_from_rvalue_datatype(
                entry.second));
    });

    return vector_size < 16UL ? vector_size :
        util::align_up_to_16(vector_size);
    // clang-format on
}

/**
 * @brief Set and allocate an address from an arbitrary offset
 */
constexpr void detail::Stack::set_address_from_size(
    LValue const& lvalue,
    Offset allocate,
    Operand_Size operand)
{
    if (stack_address[lvalue].second != Operand_Size::Empty)
        return;
    size += allocate;
    stack_address.insert(lvalue, { size, operand });
}

/**
 * @brief Get the lvalue of a local variable allocated at an offset
 */
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