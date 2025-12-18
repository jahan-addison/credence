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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include "generator.h"

#include "assembly.h"          // for Instructions, inserter, add_asm...
#include "credence/symbol.h"   // for Symbol_Table
#include "memory.h"            // for Memory_Accessor, Instruction_Flag
#include "runtime.h"           // for is_stdlib_function, get_library...
#include "stack.h"             // for Stack
#include "syscall.h"           // for syscall_arguments_t, exit_syscall
#include <credence/error.h>    // for credence_assert, assert_nequal_...
#include <credence/ir/ita.h>   // for Instruction, Quadruple, Instruc...
#include <credence/ir/table.h> // for Table, Function, Vector
#include <credence/types.h>    // for get_rvalue_datatype_from_string
#include <credence/util.h>     // for is_variant, sv, AST_Node, overload
#include <cstddef>             // for size_t
#include <deque>               // for deque
#include <easyjson.h>          // for JSON
#include <fmt/base.h>          // for copy, println
#include <fmt/compile.h>       // for format, operator""_cf
#include <fmt/format.h>        // for format
#include <matchit.h>           // for App, pattern, Id, app, Wildcard
#include <memory>              // for shared_ptr, make_shared
#include <ostream>             // for basic_ostream, operator<<, endl
#include <ostream>             // for ostream
#include <source_location>     // for source_location
#include <string_view>         // for basic_string_view, string_view
#include <tuple>               // for get, tuple
#include <utility>             // for get, pair, make_pair, move
#include <variant>             // for variant, monostate, visit, get
#include <vector>              // for vector

#define credence_current_location std::source_location::current()

namespace credence::target::x86_64 {

namespace m = matchit;

/**
 * @brief Assembly Emitter Factory
 *
 * Emit a complete x86-64 program from an AST and symbols
 */
void emit(std::ostream& os, util::AST_Node& symbols, util::AST_Node const& ast)
{
    auto [globals, instructions] = ir::make_ita_instructions(symbols, ast);
    auto table = std::make_shared<ir::Table>(
        ir::Table{ symbols, instructions, globals });
    auto stack = std::make_shared<assembly::Stack>();
    table->build_from_ita_instructions();
    auto accessor = std::make_shared<memory::Memory_Accessor>(table, stack);
    auto emitter = Assembly_Emitter{ accessor };
    emitter.emit(os);
}

/**
 * @brief Emit the x64 instructions of a B language source
 */
void Assembly_Emitter::emit(std::ostream& os)
{
    emit_x86_64_assembly_intel_prologue(os);
    data_.emit_data_section(os);
    if (!data_.instructions_.empty())
        assembly::newline(os);
    auto inserter = Instruction_Inserter{ accessor_ };
    inserter.insert(ir_instructions_);
    text_.emit_text_section(os);
    assembly::newline(os);
}

/**
 * @brief Emit from a type::Data_Type as an immediate value
 */
constexpr std::string emit_immediate_storage(Immediate const& immediate)
{
    auto [v_value, v_type, v_size] = immediate;
    m::match(sv(v_type))(m::pattern | sv("char") = [&] {
        v_value = util::strip_char(v_value, '\'');
    });
    return v_value;
}

/**
 * @brief Emit a stack offset based on size, prefix, and instruction flags
 */
constexpr std::string emit_stack_storage(assembly::Stack::Offset offset,
    assembly::Operand_Size size,
    flag::flags flags)
{
    using namespace fmt::literals;
    std::string as_str{};
    if (flags & flag::Argument)
        size = Operand_Size::Qword;
    std::string prefix = memory::storage_prefix_from_operand_size(size);
    if (flags & flag::Address)
        as_str = fmt::format("[rbp - {}]"_cf, offset);
    else
        as_str = fmt::format("{} [rbp - {}]"_cf, prefix, offset);
    return as_str;
}

/**
 * @brief Emit a register based on size, prefix, and instruction flags
 */
constexpr std::string emit_register_storage(assembly::Register device,
    assembly::Operand_Size size,
    flag::flags flags)
{
    std::string as_str{};
    using namespace fmt::literals;
    auto prefix = memory::storage_prefix_from_operand_size(size);
    if (flags & flag::Indirect)
        as_str =
            fmt::format("{} [{}]"_cf, prefix, get_storage_as_string(device));
    else
        as_str = assembly::register_as_string(device);
    return as_str;
}

/**
 * @brief Emit the intel syntax directive
 */
void emit_x86_64_assembly_intel_prologue(std::ostream& os)
{
    os << std::endl << ".intel_syntax noprefix" << std::endl << std::endl;
}

/**
 * @brief Emit the instructions for each directive type supported
 */
Directives Data_Emitter::get_instructions_from_directive_type(
    Directive directive,
    RValue const& rvalue)
{
    auto& address_storage = accessor_->address_accessor;
    assembly::Directives instructions{};
    m::match(directive)(
        m::pattern | Directive::quad =
            [&] {
                credence_assert(
                    address_storage.buffer_accessor.is_allocated_string(
                        rvalue));
                instructions = assembly::quad(
                    address_storage.buffer_accessor.get_string_address_offset(
                        rvalue));
            },
        m::pattern |
            Directive::long_ = [&] { instructions = assembly::long_(rvalue); },
        m::pattern | Directive::float_ =
            [&] { instructions = assembly::float_(rvalue); },
        m::pattern | Directive::double_ =
            [&] { instructions = assembly::double_(rvalue); },
        m::pattern |
            Directive::byte_ = [&] { instructions = assembly::byte_(rvalue); }

    );
    return instructions;
}

/**
 * @brief Set string in the data section awith .asciz directive
 */
void Data_Emitter::set_data_strings()
{
    auto& table = accessor_->table_accessor.table_;
    for (auto const& string : table->strings) {
        auto data_instruction = assembly::asciz(
            &accessor_->address_accessor.buffer_accessor.constant_size_index,
            string);
        accessor_->address_accessor.buffer_accessor.insert(
            string, data_instruction.first);
        assembly::inserter(instructions_, data_instruction.second);
    }
}

/**
 * @brief Set global data in the data section from the table vectors
 */
void Data_Emitter::set_data_globals()
{
    auto& table = accessor_->table_accessor.table_;
    for (auto const& global : table->globals.get_pointers()) {
        credence_assert(table->vectors.contains(global));
        auto& vector = table->vectors.at(global);
        instructions_.emplace_back(global);
        auto address = type::semantic::Address{ 0 };
        for (auto const& item : vector->data) {
            auto directive =
                assembly::get_data_directive_from_rvalue_type(item.second);
            auto data = type::get_value_from_rvalue_data_type(item.second);
            vector->set_address_offset(item.first, address);
            address += assembly::get_size_from_operand_size(
                assembly::get_operand_size_from_rvalue_datatype(item.second));

            auto instructions =
                get_instructions_from_directive_type(directive, data);

            assembly::inserter(instructions_, instructions);
        }
    }
}

/**
 * @brief Emit the data section x64 instructions of a B language source
 */
void Data_Emitter::emit_data_section(std::ostream& os)
{
    os << assembly::Directive::data;
    set_data_strings();
    set_data_globals();
    assembly::newline(os, 2);
    if (!instructions_.empty())
        for (std::size_t index = 0; index < instructions_.size(); index++) {
            auto data_item = instructions_[index];
            std::visit(util::overload{
                           [&](type::semantic::Label const& s) {
                               os << s << ":" << std::endl;
                           },
                           [&](assembly::Data_Pair const& s) {
                               os << assembly::tabwidth(4) << s.first;
                               if (s.first == Directive::asciz)
                                   os << " " << "\"" << s.second << "\"";
                               else
                                   os << " " << s.second;
                               assembly::newline(os);
                               if (index < instructions_.size() - 1)
                                   assembly::newline(os);
                           },
                       },
                data_item);
        }
}

/**
 * @brief Get the string representation of a storage device
 */
std::string Storage_Emitter::get_storage_device_as_string(
    assembly::Storage const& storage,
    Operand_Size size)
{
    m::Id<assembly::Stack::Offset> s;
    m::Id<assembly::Register> r;
    m::Id<assembly::Immediate> i;
    if (address_size != Operand_Size::Empty)
        size = address_size;
    auto flags = accessor_->flag_accessor.get_instruction_flags_at_index(
        instrunction_index_);
    return m::match(storage)(
        m::pattern | m::as<assembly::Stack::Offset>(s) =
            [&] { return emit_stack_storage(*s, size, flags); },
        m::pattern | m::as<assembly::Register>(r) =
            [&] { return emit_register_storage(*r, size, flags); },
        m::pattern | m::as<assembly::Immediate>(
                         i) = [&] { return emit_immediate_storage(*i); });
}

/**
 * @brief Emit the representation of a storage device based on type
 *
 *    Apply all flags set on the instruction index during code translation
 */
void Storage_Emitter::emit(std::ostream& os,
    assembly::Storage const& storage,
    assembly::Mnemonic mnemonic,
    memory::Operand_Type type_)
{
    auto& stack = accessor_->stack;
    auto& flag_accessor = accessor_->flag_accessor;
    auto flags =
        flag_accessor.get_instruction_flags_at_index(instrunction_index_);
    auto size =
        get_operand_size_from_storage(source_storage_, accessor_->stack);
    switch (type_) {
        case memory::Operand_Type::Destination: {
            if (flag_accessor.index_contains_flag(
                    instrunction_index_, flag::QWord_Dest))
                set_address_size(Operand_Size::Qword);
            if (flag_accessor.index_contains_flag(
                    instrunction_index_, flag::Address))
                set_address_size(
                    get_operand_size_from_storage(storage, accessor_->stack));
            os << " " << get_storage_device_as_string(storage, size);
            if (flags & flag::Indirect and
                is_variant(Register, source_storage_))
                flag_accessor.unset_instruction_flag(
                    flag::Indirect, instrunction_index_);
        } break;
        case memory::Operand_Type::Source: {
            auto source = storage;
            if (mnemonic == mn(movq_))
                flag_accessor.set_instruction_flag(
                    flag::Argument, instrunction_index_);
            if (mnemonic == mn(sub) or mnemonic == mn(add))
                if (flags & flag::Align)
                    source = assembly::make_u32_int_immediate(
                        stack->get_stack_frame_allocation_size());
            if (flags & flag::Indirect_Source)
                flag_accessor.set_instruction_flag(
                    flag::Indirect, instrunction_index_);
            os << ", " << get_storage_device_as_string(source, size);
            flag_accessor.unset_instruction_flag(
                flag::Indirect, instrunction_index_);
            reset_address_size();
        }
    }
}

/**
 * @brief Emit the text section x64 instructions of a B language source
 */
void Text_Emitter::emit_text_section(std::ostream& os)
{
    auto& table = accessor_->table_accessor.table_;
    auto instructions_accessor = accessor_->instruction_accessor;
    auto& instructions = instructions_accessor->get_instructions();
    emit_text_directives(os);
    for (std::size_t index = 0; index < instructions_accessor->size();
        index++) {
        auto flags =
            accessor_->flag_accessor.get_instruction_flags_at_index(index);
        auto inst = instructions[index];
        std::visit(
            util::overload{
                [&](assembly::Instruction const& s) {
                    auto [mnemonic, dest, src] = s;
                    auto storage_emitter =
                        Storage_Emitter{ accessor_, index, src };
                    if (flags & flag::Load and not is_variant(Register, src))
                        mnemonic = assembly::Mnemonic::lea;
                    os << assembly::tabwidth(4) << mnemonic;
                    if (!is_variant(std::monostate, dest))
                        storage_emitter.emit(os,
                            dest,
                            mnemonic,
                            memory::Operand_Type::Destination);
                    if (!is_variant(std::monostate, src))
                        storage_emitter.emit(
                            os, src, mnemonic, memory::Operand_Type::Source);
                    os << std::endl;
                },
                [&](type::semantic::Label const& s) {
                    if (table->hoisted_symbols.has_key(s) and
                        table->hoisted_symbols[s]["type"].to_string() ==
                            "function_definition" and
                        s != "main")
                        assembly::newline(os, 2);
                    if (s == "_L1")
                        return;
                    os << assembly::make_label(s) << ":" << std::endl;
                } },
            inst);
    }
}

/**
 * @brief Emit text section directives
 */
void Text_Emitter::emit_text_directives(std::ostream& os)
{
    os << assembly::Directive::text << std::endl;
    os << assembly::tabwidth(4) << assembly::Directive::start;
    assembly::newline(os, 1);
    emit_stdlib_externs(os);
}

/**
 * @brief Emit text section standard library `extern` directives
 */
void Text_Emitter::emit_stdlib_externs(std::ostream& os)
{
    if (!test_no_stdlib)
        for (auto const& stdlib_f : runtime::get_library_symbols()) {
            os << assembly::tabwidth(4) << assembly::Directive::extern_ << " ";
            os << stdlib_f;
            assembly::newline(os);
        }
    assembly::newline(os);
}

/**
 * @brief Primary IR instruction visitor to x64 instructions in memory
 */
void Instruction_Inserter::insert(ir::Instructions const& ir_instructions)
{
    auto stack_frame = memory::Stack_Frame{ accessor_ };
    auto ir_visitor = IR_Instruction_Visitor(accessor_, stack_frame);
    for (std::size_t index = 0; index < ir_instructions.size(); index++) {
        auto inst = ir_instructions[index];
        ir_visitor.set_iterator_index(index);
        accessor_->table_accessor.set_ir_iterator_index(index);
        ir::Instruction ita_inst = std::get<0>(inst);
        m::match(ita_inst)(
            m::pattern | ir::Instruction::FUNC_START =
                [&] {
                    auto symbol = std::get<1>(ir_instructions.at(index - 1));
                    auto name = type::get_label_as_human_readable(symbol);
                    stack_frame.set_stack_frame(name);
                    ir_visitor.from_func_start_ita(name);
                },
            m::pattern | ir::Instruction::FUNC_END =
                [&] { ir_visitor.from_func_end_ita(); },
            m::pattern |
                ir::Instruction::MOV = [&] { ir_visitor.from_mov_ita(inst); },
            m::pattern |
                ir::Instruction::PUSH = [&] { ir_visitor.from_push_ita(inst); },
            m::pattern |
                ir::Instruction::POP = [&] { ir_visitor.from_pop_ita(); },
            m::pattern |
                ir::Instruction::CALL = [&] { ir_visitor.from_call_ita(inst); },
            m::pattern |
                ir::Instruction::LOCL = [&] { ir_visitor.from_locl_ita(inst); },
            m::pattern |
                ir::Instruction::RETURN = [&] { ir_visitor.from_return_ita(); },
            m::pattern |
                ir::Instruction::LEAVE = [&] { ir_visitor.from_leave_ita(); },
            m::pattern | ir::Instruction::LABEL =
                [&] { ir_visitor.from_label_ita(inst); }

        );
    }
}

/**
 * @brief IR Instruction Instruction::FUNC_START
 */
void IR_Instruction_Visitor::from_func_start_ita(Label const& name)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& table = accessor_->table_accessor.table_;
    credence_assert(table->functions.contains(name));
    accessor_->stack->clear();
    stack_frame_.symbol = name;
    stack_frame_.set_stack_frame(name);
    auto frame = table->functions[name];
    // function prologue
    asm__dest_s(instruction_accessor->get_instructions(), push, rbp);
    asm__short(instruction_accessor->get_instructions(), mov_, rbp, rsp);
    if (table->stack_frame_contains_ita_instruction(
            name, ir::Instruction::CALL)) {
        auto imm = assembly::make_u32_int_immediate(0);
        accessor_->flag_accessor.set_instruction_flag(
            flag::Align, instruction_accessor->size());
        asm__dest_rs(instruction_accessor->get_instructions(), sub, rsp, imm);
    }
}

/**
 * @brief IR Instruction IR Instruction::FUNC_END
 */
void IR_Instruction_Visitor::from_func_end_ita()
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& table = accessor_->table_accessor.table_;
    auto frame = stack_frame_.get_stack_frame();
    if (table->stack_frame_contains_ita_instruction(
            frame->symbol, ir::Instruction::CALL)) {
        auto imm = assembly::make_u32_int_immediate(0);
        if (frame->symbol != "main") {
            accessor_->flag_accessor.set_instruction_flag(
                flag::Align, instruction_accessor->size());
            asm__dest_rs(
                instruction_accessor->get_instructions(), add, rsp, imm);
        }
    }
    accessor_->register_accessor.reset_available_registers();
}

/**
 * @brief IR Instruction Instruction::LOCL
 *
 *   Note that at this point we allocate local variables on the stack
 */
void IR_Instruction_Visitor::from_locl_ita(ir::Quadruple const& inst)
{
    auto locl_lvalue = std::get<1>(inst);
    auto frame = stack_frame_.get_stack_frame();
    auto& table = accessor_->table_accessor.table_;
    auto& stack = accessor_->stack;
    auto& locals = accessor_->table_accessor.table_->get_stack_frame_symbols();

    // The storage of an immediate (and, really, all) relational
    // expressions will be the `al` register, 1 for true, 0 for false
    auto is_immediate_relational_expression = [&](RValue const& rvalue) {
        return type::is_relation_binary_expression(
            type::get_value_from_rvalue_data_type(
                locals.get_symbol_by_name(rvalue)));
    };
    auto is_vector = [&](RValue const& rvalue) {
        return table->vectors.contains(type::from_lvalue_offset(rvalue));
    };
    m::match(locl_lvalue)(
        // Allocate a pointer on the stack
        m::pattern | m::app(type::is_dereference_expression, true) =
            [&] {
                auto lvalue =
                    type::get_unary_rvalue_reference(std::get<1>(inst));
                stack->set_address_from_address(lvalue);
            },
        // Allocate a vector (array), including all of its elements on
        // the stack
        m::pattern | m::app(is_vector, true) =
            [&] {
                auto vector = table->vectors.at(locl_lvalue);
                auto size = stack->get_stack_size_from_table_vector(*vector);
                stack->set_address_from_size(locl_lvalue, size);
            },
        // Allocate 1 byte on the stack for the al register
        m::pattern | m::app(is_immediate_relational_expression, true) =
            [&] {
                stack->set_address_from_accumulator(locl_lvalue, Register::al);
            },
        // Allocate on the stack from the size of the lvalue type
        m::pattern | m::_ =
            [&] {
                auto type = table->get_type_from_rvalue_data_type(locl_lvalue);
                stack->set_address_from_type(locl_lvalue, type);
            }

    );
}

/**
 * @brief IR Instruction Instruction::PUSH
 */
void IR_Instruction_Visitor::from_push_ita(ir::Quadruple const& inst)
{
    auto& table = accessor_->table_accessor.table_;
    stack_frame_.argument_stack.emplace_front(
        table->from_temporary_lvalue(std::get<1>(inst)));
}

/**
 * @brief IR Instruction Instruction::POP
 */
void IR_Instruction_Visitor::from_pop_ita()
{
    stack_frame_.size = 0;
    stack_frame_.argument_stack.clear();
    stack_frame_.call_stack.pop_back();
}

/**
 * @brief Get the storage devices of arguments on the stack from ir::Table
 */
syscall_ns::syscall_arguments_t
Invocation_Inserter::get_operands_storage_from_argument_stack()
{
    Operand_Inserter operands{ accessor_, stack_frame_ };
    syscall_ns::syscall_arguments_t arguments{};
    auto& table = accessor_->table_accessor.table_;
    for (auto const& rvalue : stack_frame_.argument_stack) {
        if (rvalue == "RET") {
            credence_assert(table->functions.contains(stack_frame_.tail));
            auto tail_frame = table->functions.at(stack_frame_.tail);
            arguments.emplace_back(operands.get_operand_storage_from_rvalue(
                tail_frame->ret->first));
        } else {
            arguments.emplace_back(
                operands.get_operand_storage_from_rvalue(rvalue));
        }
    }
    return arguments;
}

/**
 * @brief Operand inserter for immediate rvalue operand types
 */
void Operand_Inserter::insert_from_immediate_rvalues(Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs)
{
    auto instructions_accessor = accessor_->instruction_accessor;
    auto& instructions = instructions_accessor->get_instructions();
    auto& immediate_stack = accessor_->address_accessor.immediate_stack;
    auto accumulator = accessor_->accumulator_accessor;
    m::match(op)(
        m::pattern | m::app(type::is_binary_arithmetic_operator, true) =
            [&] {
                auto imm =
                    assembly::get_result_from_trivial_integral_expression(
                        lhs, op, rhs);
                auto acc = accumulator.get_accumulator_register_from_size(
                    assembly::get_operand_size_from_rvalue_datatype(lhs));
                add_asm__as(instructions, mov, acc, imm);
            },
        m::pattern | m::app(type::is_relation_binary_operator, true) =
            [&] {
                auto imm =
                    assembly::get_result_from_trivial_relational_expression(
                        lhs, op, rhs);
                auto acc = accumulator.get_accumulator_register_from_size(
                    Operand_Size::Byte);
                accessor_->set_signal_register(acc);
                add_asm__as(instructions, mov, acc, imm);
            },
        m::pattern | m::app(type::is_bitwise_binary_operator, true) =
            [&] {
                auto imm = assembly::get_result_from_trivial_bitwise_expression(
                    lhs, op, rhs);
                auto acc = accumulator.get_accumulator_register_from_size(
                    assembly::get_operand_size_from_rvalue_datatype(lhs));
                if (!accessor_->table_accessor.is_ir_instruction_temporary())
                    add_asm__as(instructions, mov, acc, imm);
                else
                    immediate_stack.emplace_back(imm);
            },
        m::pattern | m::_ = [&] { credence_error("unreachable"); });
}

/**
 * @brief Get the storage device of an rvalue operand
 */
Storage Operand_Inserter::get_operand_storage_from_rvalue(RValue const& rvalue)
{
    assembly::Storage storage{};
    auto& stack = accessor_->stack;
    auto& table = accessor_->table_accessor.table_;
    auto frame = stack_frame_.get_stack_frame();

    if (frame->is_parameter(rvalue)) {
        auto index_of = frame->get_index_of_parameter(rvalue);
        credence_assert_nequal(index_of, -1);
        if (frame->is_pointer_parameter(rvalue))
            return memory::registers::available_qword_register.at(index_of);
        else
            return memory::registers::available_dword_register.at(index_of);
    }

    if (stack->is_allocated(rvalue)) {
        auto [operand, operand_inst] =
            accessor_->address_accessor.get_lvalue_address_and_instructions(
                rvalue, accessor_->instruction_accessor->size());
        auto& instructions =
            accessor_->instruction_accessor->get_instructions();
        assembly::inserter(instructions, operand_inst);
        return operand;
    }

    if (!stack_frame_.tail.empty() and
        not runtime::is_stdlib_function(stack_frame_.tail)) {

        auto& tail_call = table->functions[stack_frame_.tail];
        if (tail_call->locals.is_pointer(tail_call->ret->first) or
            type::is_rvalue_data_type_string(tail_call->ret->first))
            return Register::rax;
        else
            return Register::eax;
    }

    if (type::is_rvalue_data_type(rvalue)) {
        auto immediate = type::get_rvalue_datatype_from_string(rvalue);
        if (type::is_rvalue_data_type_string(immediate)) {
            storage = assembly::make_asciz_immediate(accessor_->address_accessor
                    .buffer_accessor.get_string_address_offset(
                        type::get_value_from_rvalue_data_type(immediate)));
            return storage;

        } else {
            storage = type::get_rvalue_datatype_from_string(rvalue);
            return storage;
        }
    }

    auto [operand, operand_inst] =
        accessor_->address_accessor.get_lvalue_address_and_instructions(
            rvalue, accessor_->instruction_accessor->size());
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    assembly::inserter(instructions, operand_inst);

    return operand;
}

/**
 * @brief IR Instruction Instruction::CALL
 */
void IR_Instruction_Visitor::from_call_ita(ir::Quadruple const& inst)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto inserter = Invocation_Inserter{ accessor_, stack_frame_ };
    auto function_name = type::get_label_as_human_readable(std::get<1>(inst));
    m::match(function_name)(
        m::pattern | m::app(runtime::is_syscall_function, true) =
            [&] {
                inserter.insert_from_syscall_function(
                    function_name, instruction_accessor->get_instructions());
            },
        m::pattern | m::app(runtime::is_stdlib_function, true) =
            [&] {
                inserter.insert_from_standard_library_function(
                    function_name, instruction_accessor->get_instructions());
            },
        m::pattern | m::_ =
            [&] {
                inserter.insert_from_user_defined_function(
                    function_name, instruction_accessor->get_instructions());
            });
    stack_frame_.call_stack.emplace_back(function_name);
    stack_frame_.tail = function_name;
    accessor_->register_accessor.reset_available_registers();
}

/**
 * @brief Invocation inserter for kernel syscall function call
 */
void Invocation_Inserter::insert_from_syscall_function(std::string_view routine,
    assembly::Instructions& instructions)
{
    accessor_->address_accessor.buffer_accessor.set_buffer_size_from_syscall(
        routine, stack_frame_.argument_stack);
    auto operands = get_operands_storage_from_argument_stack();
    syscall_ns::common::make_syscall(instructions,
        routine,
        operands,
        accessor_->register_accessor.signal_register);
}

/**
 * @brief Invocation inserter for user defined functions and arguments
 */
void Invocation_Inserter::insert_from_user_defined_function(
    std::string_view routine,
    assembly::Instructions& instructions)
{
    auto operands = get_operands_storage_from_argument_stack();
    for (auto const& operand : operands) {
        auto size = get_operand_size_from_storage(operand, accessor_->stack);
        auto storage = accessor_->register_accessor.get_available_register(
            size, accessor_->stack);
        // todo: if the item is on the stack and a pointer, use the address
        if (is_variant(Immediate, operand)) {
            if (type::is_rvalue_data_type_string(std::get<Immediate>(operand)))
                accessor_->flag_accessor.set_instruction_flag(
                    flag::Load, accessor_->instruction_accessor->size());
        }
        if (size == Operand_Size::Qword)
            accessor_->flag_accessor.set_instruction_flag(
                flag::Argument, accessor_->instruction_accessor->size());
        add_asm__as(instructions, mov, storage, operand);
    }
    auto immediate = assembly::make_direct_immediate(routine);
    asm__dest(instructions, call, immediate);
}

/**
 * @brief Invocation inserter for standard library functions and arguments
 */
void Invocation_Inserter::insert_from_standard_library_function(
    std::string_view routine,
    assembly::Instructions& instructions)
{
    auto& table = accessor_->table_accessor.table_;
    auto& address_storage = accessor_->address_accessor;
    auto operands = get_operands_storage_from_argument_stack();
    auto& argument_stack = stack_frame_.argument_stack;
    m::match(routine)(
        m::pattern | sv("putchar") = [&] {},
        m::pattern | sv("print") =
            [&] {
                if (!address_storage.is_lvalue_storage_type(
                        argument_stack.front(), "string") and
                    not runtime::is_address_device_pointer_to_buffer(
                        operands.front(), table, accessor_->stack))
                    table->throw_compiletime_error(
                        fmt::format("argument '{}' is not a string",
                            argument_stack.front()),
                        routine,
                        credence_current_location,
                        "function invocation");
                auto buffer = operands.back();
                auto buffer_size =
                    address_storage.buffer_accessor.has_bytes()
                        ? address_storage.buffer_accessor
                              .get_lvalue_string_size(
                                  argument_stack.back(), stack_frame_)
                        : address_storage.buffer_accessor.read_bytes();
                operands.emplace_back(
                    assembly::make_u32_int_immediate(buffer_size));
                accessor_->flag_accessor.set_instruction_flag(
                    flag::Argument, accessor_->instruction_accessor->size());
            });
    runtime::make_library_call(instructions,
        routine,
        operands,
        accessor_->register_accessor.signal_register);
}

/**
 * @brief IR Instruction Instruction::MOV
 */
void IR_Instruction_Visitor::from_mov_ita(ir::Quadruple const& inst)
{
    auto& table = accessor_->table_accessor.table_;
    auto lhs = ir::get_lvalue_from_mov_qaudruple(inst);
    auto rhs = ir::get_rvalue_from_mov_qaudruple(inst).first;

    auto expression_inserter = Expression_Inserter{ accessor_, stack_frame_ };
    auto unary_inserter = Unary_Operator_Inserter{ accessor_, stack_frame_ };
    auto operand_inserter = Operand_Inserter{ accessor_, stack_frame_ };

    auto is_global_vector = [&](RValue const& rvalue) {
        auto rvalue_reference = type::from_lvalue_offset(rvalue);
        return table->vectors.contains(rvalue_reference) and
               table->globals.is_pointer(rvalue_reference);
    };

    m::match(lhs, rhs)(
        // Parameters are prepared in the table, so skip parameter lvalues
        m::pattern | m::ds(m::app(is_parameter, true), m::_) = [] {},
        // Translate an rvalue from a mutual-recursive temporary lvalue
        m::pattern | m::ds(m::app(type::is_temporary, true), m::_) =
            [&] { expression_inserter.insert_from_temporary_lvalue(lhs); },
        // Translate a unary-to-unary rvalue reference
        m::pattern | m::ds(m::app(type::is_unary_expression, true),
                         m::app(type::is_unary_expression, true)) =
            [&] {
                unary_inserter.insert_from_unary_to_unary_assignment(lhs, rhs);
            },
        // Translate from a vector in global scope
        m::pattern | m::ds(m::app(is_global_vector, true), m::_) =
            [&] {
                expression_inserter.insert_from_global_vector_assignment(
                    lhs, rhs);
            },
        m::pattern | m::ds(m::_, m::app(is_global_vector, true)) =
            [&] {
                expression_inserter.insert_from_global_vector_assignment(
                    lhs, rhs);
            },
        // Direct operand to mnemonic translation
        m::pattern | m::_ =
            [&] { operand_inserter.insert_from_mnemonic_operand(lhs, rhs); });
}

void IR_Instruction_Visitor::from_cmp_ita(
    [[maybe_unused]] ir::Quadruple const& inst)
{
}
void IR_Instruction_Visitor::from_if_ita(
    [[maybe_unused]] ir::Quadruple const& inst)
{
}
void IR_Instruction_Visitor::from_jmp_e_ita(
    [[maybe_unused]] ir::Quadruple const& inst)
{
}

/**
 * @brief Mnemonic operand default inserter via pattern matching
 */
void Operand_Inserter::insert_from_mnemonic_operand(LValue const& lhs,
    RValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto& stack = accessor_->stack;
    auto& address_storage = accessor_->address_accessor;
    auto accumulator = accessor_->accumulator_accessor;

    auto expression_inserter = Expression_Inserter{ accessor_, stack_frame_ };
    auto binary_inserter = Binary_Operator_Inserter{ accessor_, stack_frame_ };
    auto unary_inserter = Unary_Operator_Inserter{ accessor_, stack_frame_ };

    auto is_address = [&](RValue const& rvalue) {
        return stack->is_allocated(rvalue);
    };

    m::match(rhs)(
        // Translate from an immediate value assignment
        m::pattern | m::app(is_immediate, true) =
            [&] {
                auto imm = type::get_rvalue_datatype_from_string(rhs);
                auto [lhs_storage, storage_inst] =
                    address_storage.get_lvalue_address_and_instructions(
                        lhs, instruction_accessor->size());
                assembly::inserter(instructions, storage_inst);
                if (type::get_type_from_rvalue_data_type(imm) == "string") {
                    // strings decay into QWord pointers
                    expression_inserter.insert_from_string(
                        type::get_value_from_rvalue_data_type(imm));
                    accessor_->flag_accessor.set_instruction_flag(
                        flag::QWord_Dest, instruction_accessor->size());
                    accessor_->stack->set_address_from_accumulator(
                        lhs, Register::rcx);
                    add_asm__as(instructions, mov, lhs_storage, Register::rcx);
                } else
                    add_asm__as(instructions, mov, lhs_storage, imm);
            },
        // The storage of the rvalue from `insert_from_temporary_lvalue`
        // in the function above is in the accumulator register, use it
        // to assign to a local address
        m::pattern | m::app(is_temporary, true) =
            [&] {
                if (address_storage.address_ir_assignment) {
                    address_storage.address_ir_assignment = false;
                    Storage lhs_storage = stack->get(lhs).first;
                    add_asm__as(instructions, mov, lhs_storage, Register::rcx);
                } else {
                    auto acc = accumulator.get_accumulator_register_from_size();
                    if (!type::is_unary_expression(lhs))
                        stack->set_address_from_accumulator(lhs, acc);
                    auto [lhs_storage, storage_inst] =
                        address_storage.get_lvalue_address_and_instructions(
                            lhs, instruction_accessor->size());
                    assembly::inserter(instructions, storage_inst);
                    add_asm__as(instructions, mov, lhs_storage, acc);
                }
            },
        // Local-to-local variable assignment
        m::pattern | m::app(is_address, true) =
            [&] {
                credence_assert(stack->get(rhs).second != Operand_Size::Empty);
                Storage lhs_storage = stack->get(lhs).first;
                Storage rhs_storage = stack->get(rhs).first;
                auto acc = accumulator.get_accumulator_register_from_size(
                    stack->get(rhs).second);
                add_asm__as(instructions, mov, acc, rhs_storage);
                add_asm__as(instructions, mov, lhs_storage, acc);
            },
        // Unary expression assignment, including pointers to an address
        m::pattern | m::app(type::is_unary_expression, true) =
            [&] {
                auto [lhs_storage, storage_inst] =
                    address_storage.get_lvalue_address_and_instructions(
                        lhs, instruction_accessor->size());
                assembly::inserter(instructions, storage_inst);
                auto unary_op = type::get_unary_operator(rhs);
                unary_inserter.insert_from_unary_expression(
                    unary_op, lhs_storage);
            },
        // Translate from binary expressions in the IR
        m::pattern | m::app(type::is_binary_expression, true) =
            [&] { binary_inserter.from_binary_operator_expression(rhs); },
        m::pattern | m::_ = [&] { credence_error("unreachable"); });
}

/**
 * @brief Unary temporary expression inserter
 *
 * See ir/temporary.h for details
 */
void Unary_Operator_Inserter::from_temporary_unary_operator_expression(
    RValue const& expr)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto& stack = accessor_->stack;
    auto& address_space = accessor_->address_accessor;
    auto& table_accessor = accessor_->table_accessor;
    auto& register_accessor = accessor_->register_accessor;
    credence_assert(type::is_unary_expression(expr));
    auto op = type::get_unary_operator(expr);
    RValue rvalue = type::get_unary_rvalue_reference(expr);

    auto is_vector = [&](RValue const& rvalue) {
        return table_accessor.table_->vectors.contains(
            type::from_lvalue_offset(rvalue));
    };

    if (stack->contains(rvalue)) {
        // This is the address-of operator, use a qword size register
        if (op == "&") {
            address_space.address_ir_assignment = true;
            insert_from_unary_expression(op, stack->get(rvalue).first);
            accessor_->set_signal_register(Register::rcx);
            return;
        }
        auto size = stack->get(rvalue).second;
        auto acc =
            table_accessor.next_ir_instruction_is_temporary() and
                    not table_accessor.last_ir_instruction_is_assignment()
                ? register_accessor.get_second_register_from_size(size)
                : accessor_->accumulator_accessor
                      .get_accumulator_register_from_size(size);
        add_asm__as(instructions, mov, acc, stack->get(rvalue).first);
        insert_from_unary_expression(op, acc);
    } else if (is_vector(rvalue)) {
        auto [address, address_inst] =
            address_space.get_lvalue_address_and_instructions(rvalue, false);
        assembly::inserter(instructions, address_inst);
        auto size = get_operand_size_from_storage(address, stack);
        auto acc =
            table_accessor.next_ir_instruction_is_temporary() and
                    not table_accessor.last_ir_instruction_is_assignment()
                ? register_accessor.get_second_register_from_size(size)
                : accessor_->accumulator_accessor
                      .get_accumulator_register_from_size(size);
        address_space.address_ir_assignment = true;
        accessor_->set_signal_register(Register::rax);
        insert_from_unary_expression(op, acc, address);
    } else {
        auto immediate = type::get_rvalue_datatype_from_string(rvalue);
        auto size = assembly::get_operand_size_from_rvalue_datatype(immediate);
        auto acc =
            table_accessor.next_ir_instruction_is_temporary() and
                    not table_accessor.last_ir_instruction_is_assignment()
                ? register_accessor.get_second_register_from_size(size)
                : accessor_->accumulator_accessor
                      .get_accumulator_register_from_size(size);
        add_asm__as(instructions, mov, acc, immediate);
        insert_from_unary_expression(op, acc);
    }
}

/**
 * @brief Construct a pair of Immediates from 2 RValue's
 */
inline auto get_rvalue_pair_as_immediate(assembly::Stack::RValue const& lhs,
    assembly::Stack::RValue const& rhs)
{
    return std::make_pair(type::get_rvalue_datatype_from_string(lhs),
        type::get_rvalue_datatype_from_string(rhs));
}

/**
 * @brief Expression inserter from string rvalue to an .asciz directive
 *
 * The buffer_accessor holds the %rip offset in the data section
 */
void Expression_Inserter::insert_from_string(RValue const& str)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    credence_assert(
        accessor_->address_accessor.buffer_accessor.is_allocated_string(str));
    auto location = assembly::make_asciz_immediate(
        accessor_->address_accessor.buffer_accessor.get_string_address_offset(
            str));
    asm__dest_rs(instructions, lea, rcx, location);
}

/**
 * @brief Binary operator inserter of expression operands
 */
void Binary_Operator_Inserter::from_binary_operator_expression(
    RValue const& rvalue)
{
    credence_assert(type::is_binary_expression(rvalue));
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto& stack = accessor_->stack;
    auto& table_accessor = accessor_->table_accessor;
    auto registers = accessor_->register_accessor;
    auto accumulator = accessor_->accumulator_accessor;

    Storage lhs_s{};
    Storage rhs_s{};

    Operand_Inserter operand_inserter(accessor_, stack_frame_);
    auto expression = type::from_rvalue_binary_expression(rvalue);
    auto [lhs, rhs, op] = expression;
    auto immediate = false;
    auto is_address = [&](RValue const& rvalue) {
        return accessor_->stack->is_allocated(rvalue);
    };

    m::match(lhs, rhs)(
        m::pattern |
            m::ds(m::app(is_immediate, true), m::app(is_immediate, true)) =
            [&] {
                auto [lhs_i, rhs_i] = get_rvalue_pair_as_immediate(lhs, rhs);
                lhs_s = lhs_i;
                rhs_s = rhs_i;
                operand_inserter.insert_from_immediate_rvalues(
                    lhs_i, op, rhs_i);
                immediate = true;
            },
        m::pattern | m::ds(m::app(is_address, true), m::app(is_address, true)) =
            [&] {
                if (!table_accessor.last_ir_instruction_is_assignment()) {
                    lhs_s = registers.get_available_register(
                        stack->get(lhs).second, stack);
                    add_asm__as(
                        instructions, mov, lhs_s, stack->get(lhs).first);
                    rhs_s = stack->get(rhs).first;
                } else {
                    lhs_s = accumulator.get_accumulator_register_from_size(
                        stack->get(lhs).second);
                    add_asm__as(
                        instructions, mov, lhs_s, stack->get(lhs).first);
                    rhs_s = stack->get(rhs).first;
                }
            },
        m::pattern |
            m::ds(m::app(is_temporary, true), m::app(is_temporary, true)) =
            [&] {
                auto size = assembly::get_operand_size_from_register(
                    accumulator.get_accumulator_register_from_size());
                auto acc = accumulator.get_accumulator_register_from_size();
                lhs_s = acc;
                auto& immediate_stack =
                    accessor_->address_accessor.immediate_stack;
                if (!immediate_stack.empty()) {
                    rhs_s = immediate_stack.back();
                    immediate_stack.pop_back();
                    if (!immediate_stack.empty()) {
                        add_asm__as(
                            instructions, mov, acc, immediate_stack.back());
                        immediate_stack.pop_back();
                    }
                } else {
                    auto intermediate =
                        registers.get_second_register_from_size(size);
                    rhs_s = intermediate;
                }
            },
        m::pattern |
            m::ds(m::app(is_address, true), m::app(is_address, false)) =
            [&] {
                lhs_s = stack->get(lhs).first;
                rhs_s = registers.get_register_for_binary_operator(rhs, stack);
                if (table_accessor.last_ir_instruction_is_assignment()) {
                    auto acc = accumulator.get_accumulator_register_from_size(
                        stack->get(lhs).second);
                    add_asm__as(instructions, mov, acc, stack->get(lhs).first);
                }
                if (is_temporary(rhs)) {
                    lhs_s = accumulator.get_accumulator_register_from_size(
                        stack->get(lhs).second);
                    rhs_s = stack->get(lhs).first;
                }

                if (table_accessor.is_ir_instruction_temporary()) {
                    if (type::is_bitwise_binary_operator(op)) {
                        auto storage = registers.get_available_register(
                            stack->get(lhs).second, stack);
                        add_asm__as(
                            instructions, mov, storage, stack->get(lhs).first);
                        lhs_s = storage;
                    } else {
                        lhs_s =
                            accumulator.get_accumulator_register_from_storage(
                                lhs_s, stack);
                    }
                }
            },
        m::pattern |
            m::ds(m::app(is_address, false), m::app(is_address, true)) =
            [&] {
                lhs_s = registers.get_register_for_binary_operator(lhs, stack);
                rhs_s = stack->get(rhs).first;
                if (table_accessor.last_ir_instruction_is_assignment()) {
                    auto acc = accumulator.get_accumulator_register_from_size(
                        stack->get(rhs).second);
                    add_asm__as(instructions, mov, acc, stack->get(rhs).first);
                }
                if (is_temporary(lhs) or
                    table_accessor.is_ir_instruction_temporary())
                    rhs_s = accumulator.get_accumulator_register_from_size(
                        stack->get(rhs).second);
            },
        m::pattern |
            m::ds(m::app(is_temporary, true), m::app(is_temporary, false)) =
            [&] {
                lhs_s = accumulator.get_accumulator_register_from_size();
                rhs_s = registers.get_register_for_binary_operator(rhs, stack);
            },
        m::pattern |
            m::ds(m::app(is_temporary, false), m::app(is_temporary, true)) =
            [&] {
                lhs_s = registers.get_register_for_binary_operator(lhs, stack);
                rhs_s = accumulator.get_accumulator_register_from_size();
            },
        m::pattern | m::ds(m::_, m::_) =
            [&] {
                lhs_s = registers.get_register_for_binary_operator(lhs, stack);
                rhs_s = registers.get_register_for_binary_operator(rhs, stack);
            });
    if (!immediate) {
        auto operand_inserter = Operand_Inserter{ accessor_, stack_frame_ };
        Storage_Operands operands = { lhs_s, rhs_s };
        operand_inserter.insert_from_operands(operands, op);
    }
}

/**
 * @brief Operand Inserter mediator for expression mnemonics operands
 */
void Operand_Inserter::insert_from_operands(Storage_Operands& operands,
    std::string const& op)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    if (is_variant(Immediate, operands.first))
        std::swap(operands.first, operands.second);
    if (type::is_binary_arithmetic_operator(op)) {
        auto arithmetic = Arithemtic_Operator_Inserter{ accessor_ };
        assembly::inserter(instructions,
            arithmetic.from_arithmetic_expression_operands(operands, op)
                .second);
    } else if (type::is_relation_binary_operator(op)) {
        auto relational = Relational_Operator_Inserter{ accessor_ };
        assembly::inserter(instructions,
            relational.from_relational_expression_operands(operands, op)
                .second);
    } else if (type::is_bitwise_binary_operator(op)) {
        auto bitwise = Bitwise_Operator_Inserter{ accessor_ };
        assembly::inserter(instructions,
            bitwise.from_bitwise_expression_operands(operands, op).second);
    } else if (type::is_unary_expression(op)) {
        auto unary = Unary_Operator_Inserter{ accessor_, stack_frame_ };
        unary.insert_from_unary_expression(op, operands.first);
    } else {
        credence_error(fmt::format("unreachable: operator '{}'", op));
    }
}

/**
 * @brief Inserter from rvalue at a temporary lvalue location
 */
void Expression_Inserter::insert_from_temporary_lvalue(LValue const& lvalue)
{
    auto& table = accessor_->table_accessor.table_;
    auto temporary = table->from_temporary_lvalue(lvalue);
    insert_from_rvalue(temporary);
}

/**
 * @brief Inserter from ir unary expression types
 */
void Unary_Operator_Inserter::insert_from_unary_expression(
    std::string const& op,
    Storage const& dest,
    Storage const& src)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto index = accessor_->instruction_accessor->size();
    m::match(op)(
        m::pattern | std::string{ "++" } =
            [&] {
                assembly::inserter(instructions, assembly::inc(dest).second);
            },
        m::pattern | std::string{ "--" } =
            [&] {
                assembly::inserter(instructions, assembly::dec(dest).second);
            },
        m::pattern | std::string{ "~" } =
            [&] {
                assembly::inserter(instructions, assembly::b_not(dest).second);
            },
        m::pattern | std::string{ "&" } =
            [&] {
                accessor_->flag_accessor.set_instruction_flag(
                    flag::Address, index);
                auto acc = Register::rcx;
                if (src != assembly::O_NUL)
                    assembly::inserter(
                        instructions, assembly::lea(dest, src).second);
                else
                    assembly::inserter(
                        instructions, assembly::lea(acc, dest).second);
            },
        m::pattern | std::string{ "*" } =
            [&] {
                auto acc = accessor_->accumulator_accessor
                               .get_accumulator_register_from_storage(
                                   dest, accessor_->stack);
                add_asm__as(instructions, mov, acc, dest);
                accessor_->flag_accessor.set_instruction_flag(
                    flag::Indirect, index);
                add_asm__as(instructions, mov, acc, src);
            },
        m::pattern | std::string{ "-" } =
            [&] {
                assembly::inserter(instructions, assembly::neg(dest).second);
            },
        m::pattern | std::string{ "+" } = [&] {});
}

/**
 * @brief Expression inserter of rvalue expressions and rvalue references
 */
void Expression_Inserter::insert_from_rvalue(RValue const& rvalue)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& table = accessor_->table_accessor.table_;
    auto& instructions = instruction_accessor->get_instructions();

    auto binary_inserter = Binary_Operator_Inserter{ accessor_, stack_frame_ };
    auto unary_inserter = Unary_Operator_Inserter{ accessor_, stack_frame_ };
    auto operand_inserter = Operand_Inserter{ accessor_, stack_frame_ };

    m::match(rvalue)(
        m::pattern | m::app(type::is_binary_expression, true) =
            [&] { binary_inserter.from_binary_operator_expression(rvalue); },
        m::pattern | m::app(type::is_unary_expression, true) =
            [&] {
                unary_inserter.from_temporary_unary_operator_expression(rvalue);
            },
        m::pattern | m::app(type::is_rvalue_data_type, true) =
            [&] {
                Storage immediate =
                    operand_inserter.get_operand_storage_from_rvalue(rvalue);
                auto acc = accessor_->accumulator_accessor
                               .get_accumulator_register_from_storage(
                                   immediate, accessor_->stack);
                add_asm__as(instructions, mov, acc, immediate);
                if (type::get_type_from_rvalue_data_type(rvalue) == "string")
                    accessor_->flag_accessor.set_instruction_flag(
                        flag::Address, instruction_accessor->size());
            },
        m::pattern | RValue{ "RET" } =
            [&] {
                if (runtime::is_stdlib_function(stack_frame_.tail))
                    return;
                credence_assert(table->functions.contains(stack_frame_.tail));
                auto frame = table->functions[stack_frame_.tail];
                credence_assert(frame->ret.has_value());
                auto immediate =
                    operand_inserter.get_operand_storage_from_rvalue(
                        frame->ret->first);
                if (get_operand_size_from_storage(
                        immediate, accessor_->stack) == Operand_Size::Qword) {
                    accessor_->flag_accessor.set_instruction_flag(
                        flag::QWord_Dest, instruction_accessor->size());
                    accessor_->set_signal_register(Register::rax);
                }
            },
        m::pattern | m::_ =
            [&] {
                auto symbols = table->get_stack_frame_symbols();
                Storage immediate = symbols.get_symbol_by_name(rvalue);
                auto acc = accessor_->accumulator_accessor
                               .get_accumulator_register_from_storage(
                                   immediate, accessor_->stack);
                add_asm__as(instructions, mov, acc, immediate);
            });
}

/**
 * @brief Inserter of a return value from a function body in the stack frame
 *
 *  test(*y) {
 *   return(y); <---
 *  }
 */
void Expression_Inserter::insert_from_return_rvalue(
    ir::detail::Function::Return_RValue const& ret)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto operand_inserter = Operand_Inserter{ accessor_, stack_frame_ };
    auto immediate =
        operand_inserter.get_operand_storage_from_rvalue(ret->second);
    if (get_operand_size_from_storage(immediate, accessor_->stack) ==
        Operand_Size::Qword)
        asm__dest_rs(instructions, mov, rax, immediate);
    else
        asm__dest_rs(instructions, mov, eax, immediate);
}

/**
 * @brief Inserter for vector assignment between global vectors
 */
void Expression_Inserter::insert_from_global_vector_assignment(
    LValue const& lhs,
    LValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto [lhs_storage, lhs_storage_inst] =
        accessor_->address_accessor.get_lvalue_address_and_instructions(
            lhs, instruction_accessor->size());
    assembly::inserter(instructions, lhs_storage_inst);
    auto [rhs_storage, rhs_storage_inst] =
        accessor_->address_accessor.get_lvalue_address_and_instructions(
            rhs, instruction_accessor->size());
    assembly::inserter(instructions, rhs_storage_inst);
    auto acc =
        accessor_->accumulator_accessor.get_accumulator_register_from_storage(
            lhs_storage, accessor_->stack);
    add_asm__as(instructions, mov, acc, rhs_storage);
    add_asm__as(instructions, mov, lhs_storage, acc);
}

/**
 * @brief Inserter from unary-to-unary rvalue expressions
 *
 * The only supported type is dereferenced pointers
 */
void Unary_Operator_Inserter::insert_from_unary_to_unary_assignment(
    LValue const& lhs,
    LValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto lhs_lvalue = type::get_unary_rvalue_reference(lhs);
    auto rhs_lvalue = type::get_unary_rvalue_reference(rhs);
    auto& stack = accessor_->stack;
    auto registers = accessor_->register_accessor;
    auto& table = accessor_->table_accessor.table_;
    auto lhs_op = type::get_unary_operator(lhs);
    auto rhs_op = type::get_unary_operator(rhs);

    auto frame = table->functions[stack_frame_.symbol];
    auto& locals = table->get_stack_frame_symbols();

    m::match(lhs_op, rhs_op)(m::pattern | m::ds("*", "*") = [&] {
        credence_assert_nequal(
            stack->get(lhs_lvalue).second, Operand_Size::Empty);
        credence_assert_nequal(
            stack->get(rhs_lvalue).second, Operand_Size::Empty);

        auto acc =
            accessor_->accumulator_accessor.get_accumulator_register_from_size(
                Operand_Size::Qword);
        Storage lhs_storage = stack->get(lhs_lvalue).first;
        Storage rhs_storage = stack->get(rhs_lvalue).first;
        auto size = assembly::get_operand_size_from_type(
            type::get_type_from_rvalue_data_type(locals.get_symbol_by_name(
                locals.get_pointer_by_name(lhs_lvalue))));
        auto temp = registers.get_second_register_from_size(size);

        add_asm__as(instructions, mov, acc, rhs_storage);
        accessor_->flag_accessor.set_instruction_flag(
            flag::Indirect_Source | flag::Address,
            accessor_->instruction_accessor->size());
        add_asm__as(instructions, mov, temp, acc);
        add_asm__as(instructions, mov, acc, lhs_storage);
        accessor_->flag_accessor.set_instruction_flag(
            flag::Indirect, instruction_accessor->size());
        add_asm__as(instructions, mov, acc, temp);
    });
}

/**
 * @brief IR Instruction Instruction::RET
 */
void IR_Instruction_Visitor::from_return_ita()
{
    auto inserter = Expression_Inserter{ accessor_, stack_frame_ };
    auto frame =
        accessor_->table_accessor.table_->functions[stack_frame_.symbol];
    if (frame->ret.has_value())
        inserter.insert_from_return_rvalue(frame->ret);
}

/**
 * @brief IR Instruction Instruction::LEAVE
 */
void IR_Instruction_Visitor::from_leave_ita()
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    if (stack_frame_.symbol == "main") {
        syscall_ns::common::exit_syscall(instructions, 0);
    } else {
        // function epilogue
        asm__dest(instructions, pop, Register::rbp);
        asm__zero_o(instructions, ret);
    }
}

/**
 * @brief IR Instruction Instruction::LABEL
 */
void IR_Instruction_Visitor::from_label_ita(ir::Quadruple const& inst)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    instructions.emplace_back(
        type::get_label_as_human_readable(std::get<1>(inst)));
}

/**
 * @brief Inserter of arithmetic expressions and their storage device
 */
Instruction_Pair
Arithemtic_Operator_Inserter::from_arithmetic_expression_operands(
    Storage_Operands const& operands,
    std::string const& binary_op)
{
    Instruction_Pair instructions{ Register::eax, {} };
    m::match(binary_op)(
        m::pattern | std::string{ "*" } =
            [&] {
                instructions = assembly::mul(operands.first, operands.second);
            },
        m::pattern | std::string{ "/" } =
            [&] {
                auto storage =
                    accessor_->register_accessor.get_available_register(
                        Operand_Size::Dword, accessor_->stack);
                add_asm__as(instructions.second, mov, storage, operands.first);
                instructions = assembly::div(storage, operands.second);
            },
        m::pattern | std::string{ "-" } =
            [&] {
                instructions = assembly::sub(operands.first, operands.second);
            },
        m::pattern | std::string{ "+" } =
            [&] {
                instructions = assembly::add(operands.first, operands.second);
            },
        m::pattern | std::string{ "%" } =
            [&] {
                auto storage =
                    accessor_->register_accessor.get_available_register(
                        Operand_Size::Dword, accessor_->stack);
                accessor_->set_signal_register(Register::edx);
                add_asm__as(instructions.second, mov, storage, operands.first);
                instructions = assembly::mod(storage, operands.second);
            });
    return instructions;
}

/**
 * @brief Inserter of bitwise expressions and their storage device
 */
Instruction_Pair Bitwise_Operator_Inserter::from_bitwise_expression_operands(
    Storage_Operands const& operands,
    std::string const& binary_op)
{
    Instruction_Pair instructions{ Register::eax, {} };
    m::match(binary_op)(
        m::pattern | std::string{ "<<" } =
            [&] {
                instructions =
                    assembly::lshift(operands.first, operands.second);
            },
        m::pattern | std::string{ ">>" } =
            [&] {
                instructions =
                    assembly::rshift(operands.first, operands.second);
            },
        m::pattern | std::string{ "^" } =
            [&] {
                auto acc = accessor_->accumulator_accessor
                               .get_accumulator_register_from_storage(
                                   operands.first, accessor_->stack);
                instructions = assembly::b_xor(acc, operands.second);
            },
        m::pattern | std::string{ "&" } =
            [&] {
                instructions = assembly::b_and(operands.first, operands.second);
            },
        m::pattern | std::string{ "|" } =
            [&] {
                instructions = assembly::b_or(operands.first, operands.second);
            });
    return instructions;
}

/**
 * @brief Inserter of relational expressions and their storage device
 */
Instruction_Pair
Relational_Operator_Inserter::from_relational_expression_operands(
    Storage_Operands const& operands,
    std::string const& binary_op)
{
    Instruction_Pair instructions{ Register::eax, {} };
    m::match(binary_op)(
        m::pattern | std::string{ "==" } =
            [&] {
                instructions = assembly::r_eq(operands.first, operands.second);
            },
        m::pattern | std::string{ "!=" } =
            [&] {
                instructions = assembly::r_neq(operands.first, operands.second);
            },
        m::pattern | std::string{ "<" } =
            [&] {
                instructions = assembly::r_gt(operands.first, operands.second);
            },
        m::pattern | std::string{ ">" } =
            [&] {
                instructions = assembly::r_lt(operands.first, operands.second);
            },
        m::pattern | std::string{ "<=" } =
            [&] {
                instructions = assembly::r_le(operands.first, operands.second);
            },
        m::pattern | std::string{ ">=" } =
            [&] {
                instructions = assembly::r_ge(operands.first, operands.second);
            });

    return instructions;
}

/**
 * @brief Get the operand size (word size) of a storage device
 */
assembly::Operand_Size get_operand_size_from_storage(Storage const& storage,
    memory::Stack_Pointer& stack)
{
    m::Id<assembly::Stack::Offset> s;
    m::Id<Immediate> i;
    m::Id<Register> r;
    return m::match(storage)(
        m::pattern | m::as<assembly::Stack::Offset>(s) =
            [&] { return stack->get_operand_size_from_offset(*s); },
        m::pattern | m::as<Immediate>(i) =
            [&] { return assembly::get_operand_size_from_rvalue_datatype(*i); },
        m::pattern | m::as<Register>(r) =
            [&] { return assembly::get_operand_size_from_register(*r); },
        m::pattern | m::_ = [&] { return Operand_Size::Empty; });
}

/**
 * @brief Code Generator
 *
 * Test emit factory
 */
#ifdef CREDENCE_TEST
void emit(std::ostream& os,
    util::AST_Node& symbols,
    util::AST_Node const& ast,
    bool no_stdlib)
{
    auto [globals, instructions] = ir::make_ita_instructions(symbols, ast);
    auto table = std::make_shared<ir::Table>(
        ir::Table{ symbols, instructions, globals });
    auto stack = std::make_shared<assembly::Stack>();
    table->build_from_ita_instructions();
    auto accessor = std::make_shared<memory::Memory_Accessor>(table, stack);
    auto emitter = Assembly_Emitter{ accessor };
    emitter.text_.test_no_stdlib = no_stdlib;
    emitter.emit(os);
}
#endif

} // namespace x86_64