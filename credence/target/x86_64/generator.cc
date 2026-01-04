/*****************************************************************************
 * Copyright (c) Jahan Addison
 *
 * This software is dual-licensed under the Apache License, Version 2.0 or
 * the GNU General Public License, Version 3.0 or later.
 *
 * You may use this work, in part or in whole, under the terms of either
 * license.
 *
 * See the LICENSE.Apache-v2 and LICENSE.GPL-v3 files in the project root
 * for the full text of these licenses.
 ****************************************************************************/

#include "generator.h"
#include "assembly.h"                        // for inserter, newline, x64_...
#include "memory.h"                          // for Memory_Accessor, Instru...
#include "runtime.h"                         // for Library_Call_Inserter
#include "stack.h"                           // for Stack
#include "syscall.h"                         // for syscall_arguments_t
#include <credence/error.h>                  // for credence_assert, assert...
#include <credence/ir/checker.h>             // for Type_Checker
#include <credence/ir/ita.h>                 // for Instruction, Quadruple
#include <credence/ir/object.h>              // for RValue, Object, Function
#include <credence/ir/table.h>               // for Table
#include <credence/map.h>                    // for Ordered_Map
#include <credence/symbol.h>                 // for Symbol_Table
#include <credence/target/common/accessor.h> // for Buffer_Accessor
#include <credence/target/common/assembly.h> // for make_direct_immediate
#include <credence/target/common/memory.h>   // for is_temporary, Operand_Type
#include <credence/target/common/runtime.h>  // for is_stdlib_function, thr...
#include <credence/target/common/types.h>    // for Table_Pointer, Immediate
#include <credence/types.h>                  // for get_rvalue_datatype_fro...
#include <credence/util.h>                   // for is_variant, sv, AST_Node
#include <cstddef>                           // for size_t
#include <deque>                             // for deque
#include <easyjson.h>                        // for JSON
#include <fmt/base.h>                        // for copy
#include <fmt/compile.h>                     // for format, operator""_cf
#include <fmt/format.h>                      // for format
#include <matchit.h>                         // for App, pattern, Id, app
#include <memory>                            // for shared_ptr, make_shared
#include <ostream>                           // for basic_ostream, operator<<
#include <ostream>                           // for ostream
#include <string_view>                       // for basic_string_view, stri...
#include <tuple>                             // for get, tuple
#include <utility>                           // for get, pair, make_pair
#include <variant>                           // for variant, get, visit
#include <vector>                            // for vector

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
    table->build_from_ir_instructions();
    auto stack = std::make_shared<assembly::Stack>();
    auto accessor = std::make_shared<memory::Memory_Accessor>(
        table->get_table_object(), stack);
    auto emitter = Assembly_Emitter{ accessor };
    emitter.emit(os);
}

/**
 * @brief Emit a complete x86-64 program
 */
void Assembly_Emitter::emit(std::ostream& os)
{
    emit_x86_64_assembly_intel_prologue(os);
    data_.emit_data_section(os);
    if (!data_.instructions_.empty())
        assembly::newline(os);
    auto inserter = IR_Inserter{ accessor_ };
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
    common::flag::flags flags)
{
    using namespace fmt::literals;
    std::string as_str{};
    if (flags & common::flag::Argument)
        size = Operand_Size::Qword;
    std::string prefix = memory::storage_prefix_from_operand_size(size);
    if (flags & common::flag::Address)
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
    common::flag::flags flags)
{
    std::string as_str{};
    using namespace fmt::literals;
    auto prefix = memory::storage_prefix_from_operand_size(size);
    if (flags & common::flag::Indirect)
        as_str = fmt::format("{} [{}]"_cf,
            prefix,
            common::assembly::get_storage_as_string<assembly::Register>(
                device));
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
            [&] {
                credence_assert(
                    address_storage.buffer_accessor.is_allocated_float(rvalue));
                instructions = assembly::float_(
                    address_storage.buffer_accessor.get_float_address_offset(
                        rvalue));
            },
        m::pattern | Directive::double_ =
            [&] {
                credence_assert(
                    address_storage.buffer_accessor.is_allocated_double(
                        rvalue));
                instructions = assembly::double_(
                    address_storage.buffer_accessor.get_double_address_offset(
                        rvalue));
            },
        m::pattern |
            Directive::byte_ = [&] { instructions = assembly::byte_(rvalue); }

    );
    return instructions;
}

/**
 * @brief Set string in the data section with .asciz directive
 */
void Data_Emitter::set_data_strings()
{
    auto& table = accessor_->table_accessor.table_;
    for (auto const& string : table->strings) {
        auto data_instruction = assembly::asciz(
            &accessor_->address_accessor.buffer_accessor.constant_size_index,
            string);
        accessor_->address_accessor.buffer_accessor.insert_string_literal(
            string, data_instruction.first);
        assembly::inserter(instructions_, data_instruction.second);
    }
}

/**
 * @brief Set floats in the data section with .float directive
 */
void Data_Emitter::set_data_floats()
{
    auto& table = accessor_->table_accessor.table_;
    for (auto const& floatz : table->floats) {
        auto data_instruction = assembly::floatz(
            &accessor_->address_accessor.buffer_accessor.constant_size_index,
            floatz);
        accessor_->address_accessor.buffer_accessor.insert_float_literal(
            floatz, data_instruction.first);
        assembly::inserter(instructions_, data_instruction.second);
    }
}

/**
 * @brief Set double in the data section with .double directive
 */
void Data_Emitter::set_data_doubles()
{
    auto& table = accessor_->table_accessor.table_;
    for (auto const& doublez : table->doubles) {
        auto data_instruction = assembly::doublez(
            &accessor_->address_accessor.buffer_accessor.constant_size_index,
            doublez);
        accessor_->address_accessor.buffer_accessor.insert_double_literal(
            doublez, data_instruction.first);
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
    set_data_floats();
    set_data_doubles();
    set_data_globals();

    assembly::newline(os, 2);

    if (!instructions_.empty())
        for (std::size_t index = 0; index < instructions_.size(); index++) {
            auto data_item = instructions_[index];
            std::visit(
                util::overload{
                    [&](Label const& s) { os << s << ":" << std::endl; },
                    [&](assembly::Data_Pair const& s) {
                        os << assembly::tabwidth(4) << s.first;
                        if (s.first == Directive::asciz)
                            os << " " << "\""
                               << assembly::literal_type_to_string(s.second)
                               << "\"";
                        else
                            os << " "
                               << assembly::literal_type_to_string(s.second);
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
        instruction_index_);
    return m::match(storage)(
        m::pattern | m::as<assembly::Stack::Offset>(s) =
            [&] {
                // size = accessor_->stack->get_operand_size_from_offset(*s);
                return emit_stack_storage(*s, size, flags);
            },
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
        flag_accessor.get_instruction_flags_at_index(instruction_index_);
    auto size =
        get_operand_size_from_storage(source_storage_, accessor_->stack);
    if (is_variant(std::monostate, storage))
        return;
    switch (type_) {
        case memory::Operand_Type::Destination: {
            if (flag_accessor.index_contains_flag(
                    instruction_index_, common::flag::QWord_Dest))
                set_address_size(Operand_Size::Qword);
            if (flag_accessor.index_contains_flag(
                    instruction_index_, common::flag::Address))
                set_address_size(
                    get_operand_size_from_storage(storage, accessor_->stack));
            os << " " << get_storage_device_as_string(storage, size);
            if (flags & common::flag::Indirect and
                is_variant(Register, source_storage_))
                flag_accessor.unset_instruction_flag(
                    common::flag::Indirect, instruction_index_);
        } break;
        case memory::Operand_Type::Source: {
            auto source = storage;
            if (mnemonic == x64_mn(sub) or mnemonic == x64_mn(add))
                if (flags & common::flag::Align)
                    source = u32_int_immediate(
                        stack->get_stack_frame_allocation_size());
            if (flags & common::flag::Indirect_Source)
                flag_accessor.set_instruction_flag(
                    common::flag::Indirect, instruction_index_);
            os << ", " << get_storage_device_as_string(source, size);
            flag_accessor.unset_instruction_flag(
                common::flag::Indirect, instruction_index_);
            reset_address_size();
        }
    }
}

void Text_Emitter::emit_epilogue_jump(std::ostream& os)
{
    os << assembly::tabwidth(4) << assembly::Mnemonic::goto_ << " ";
    os << assembly::make_label("_L1", frame_);
    assembly::newline(os, 1);
}

/**
 * @brief Emit a local or stack frame label in the text section
 */
void Text_Emitter::emit_assembly_label(std::ostream& os,
    Label const& s,
    bool set_label)
{
    auto& table = accessor_->table_accessor.table_;
    // function labels
    if (table->hoisted_symbols.has_key(s) and
        table->hoisted_symbols[s]["type"].to_string() ==
            "function_definition") {
        // this is a new frame, emit the last frame function epilogue
        if (frame_ != s)
            emit_function_epilogue(os);
        frame_ = s;
        if (set_label)
            label_size_ = accessor_->table_accessor.table_->functions.at(s)
                              ->labels.size();
        if (s != "main")
            assembly::newline(os, 2);
        os << assembly::make_label(s) << ":";
        assembly::newline(os, 1);
        return;
    }
    // branch labels
    if (set_label and label_size_ > 1) {
        branch_ = s;
        auto label = util::get_numbers_from_string(s);
        auto label_before_reserved =
            table->functions.at(frame_)->label_before_reserved;
        if (set_label and s == "_L1") {
            // In the IR, labels are linear until _L1 and then branching starts.
            // So as soon as _L1 would be emitted, add a jump to _L1 instead
            emit_epilogue_jump(os);
            return;
        }
        os << assembly::make_label(s, frame_) << ":";
        assembly::newline(os, 1);
    }
}

/**
 * @brief Emit a mnemonic and its possible operands in the text section
 */
void Text_Emitter::emit_assembly_instruction(std::ostream& os,
    std::size_t index,
    assembly::Instruction const& s)
{
    auto flags = accessor_->flag_accessor.get_instruction_flags_at_index(index);
    auto [mnemonic, dest, src] = s;
    auto storage_emitter = Storage_Emitter{ accessor_, index, src };
    // The IR does not keep the epilogue at the end, we move it ourselves
    if (branch_ == "_L1" and label_size_ > 0) {
        return_instructions_.emplace_back(s);
        return;
    }

    if (flags & common::flag::Load and not is_variant(Register, src))
        mnemonic = assembly::Mnemonic::lea;

    os << assembly::tabwidth(4) << mnemonic;

    storage_emitter.emit(os, dest, mnemonic, memory::Operand_Type::Destination);
    storage_emitter.emit(os, src, mnemonic, memory::Operand_Type::Source);

    assembly::newline(os, 1);
}

/**
 * @brief Emit the text instruction for either a label or mnemonic
 */
void Text_Emitter::emit_text_instruction(std::ostream& os,
    std::variant<Label, assembly::Instruction> const& instruction,
    std::size_t index,
    bool set_label)
{
    // clang-format off
    std::visit(util::overload{
        [&](assembly::Instruction const& s) {
            emit_assembly_instruction(os, index, s);
        },
        [&](Label const& s) {
            emit_assembly_label(os, s, set_label);
        }
    }, instruction);
    // clang-format on
}

/**
 * @brief Emit the the function epilogue at the end if a frame has branches
 */
void Text_Emitter::emit_function_epilogue(std::ostream& os)
{
    if (!return_instructions_.empty()) {
        if (label_size_ > 1) {
            // the _L1 label is reserved in the frame for the epilogue
            os << assembly::make_label("_L1", frame_) << ":";
            assembly::newline(os, 1);
        }
        for (std::size_t index = 0; index < return_instructions_.size();
            index++) {
            emit_text_instruction(
                os, return_instructions_[index], index, false);
        }
        return_instructions_.clear();
        label_size_ = 0;
    }
}

/**
 * @brief Emit the text section instructions
 */
void Text_Emitter::emit_text_section(std::ostream& os)
{
    auto instructions_accessor = accessor_->instruction_accessor;
    auto& instructions = instructions_accessor->get_instructions();
    emit_text_directives(os);
    for (std::size_t index = 0; index < instructions_accessor->size(); index++)
        emit_text_instruction(os, instructions[index], index);
    if (!return_instructions_.empty() and frame_ == "main")
        emit_function_epilogue(os);
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
        for (auto const& stdlib_f : common::runtime::get_library_symbols()) {
            os << assembly::tabwidth(4) << assembly::Directive::extern_ << " ";
            os << stdlib_f;
            assembly::newline(os);
        }
    assembly::newline(os);
}

/**
 * @brief Setup the stack frame for a function during instruction insertion
 *
 *  Note: %r15 is reserved for the argc address and argv offsets in memory
 */
void IR_Inserter::setup_stack_frame_in_function(
    ir::Instructions const& ir_instructions,
    Visitor& visitor,
    int index)
{
    auto stack_frame = memory::Stack_Frame{ accessor_ };
    auto symbol = std::get<1>(ir_instructions.at(index - 1));
    auto name = type::get_label_as_human_readable(symbol);
    stack_frame.set_stack_frame(name);
    if (name == "main") {
        // setup argc, argv
        auto argc_argv = common::runtime::argc_argv_kernel_runtime_access<
            memory::Stack_Frame>(stack_frame);
        if (argc_argv.first) {
            auto& instructions =
                accessor_->instruction_accessor->get_instructions();
            auto argc_address = direct_immediate("[rsp]");
            x8664_add__asm(instructions, lea, r15, argc_address);
        }
    }
    visitor.from_func_start_ita(name);
}

/**
 * @brief IR instruction visitor to map x64 instructions in memory
 */
void IR_Inserter::insert(ir::Instructions const& ir_instructions)
{
    auto stack_frame = memory::Stack_Frame{ accessor_ };
    auto ir_visitor = Visitor(accessor_, stack_frame);
    for (std::size_t index = 0; index < ir_instructions.size(); index++) {
        auto inst = ir_instructions[index];
        ir_visitor.set_iterator_index(index);
        accessor_->table_accessor.set_ir_iterator_index(index);
        ir::Instruction ita_inst = std::get<0>(inst);
        m::match(ita_inst)(
            m::pattern | ir::Instruction::FUNC_START =
                [&] {
                    setup_stack_frame_in_function(
                        ir_instructions, ir_visitor, index);
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
            m::pattern | ir::Instruction::JMP_E =
                [&] { ir_visitor.from_jmp_e_ita(inst); },
            m::pattern |
                ir::Instruction::LOCL = [&] { ir_visitor.from_locl_ita(inst); },
            m::pattern |
                ir::Instruction::GOTO = [&] { ir_visitor.from_goto_ita(inst); },
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
void Visitor::from_func_start_ita(Label const& name)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& table = accessor_->table_accessor.table_;
    credence_assert(table->functions.contains(name));
    accessor_->stack->clear();
    stack_frame_.symbol = name;
    stack_frame_.set_stack_frame(name);
    auto frame = table->functions[name];
    auto& inst = instruction_accessor->get_instructions();
    // function prologue
    x8664_add__asm(inst, push, rbp);
    x8664_add__asm(inst, mov_, rbp, rsp);
    // align %rbp if there's a CALL in this stack frame
    if (table->stack_frame_contains_call_instruction(
            name, *accessor_->table_accessor.table_->ir_instructions)) {
        auto imm = u32_int_immediate(0);
        accessor_->flag_accessor.set_instruction_flag(
            common::flag::Align, instruction_accessor->size());
        x8664_add__asm(inst, sub, rsp, imm);
    }
}

/**
 * @brief IR Instruction IR Instruction::FUNC_END
 */
void Visitor::from_func_end_ita()
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& table = accessor_->table_accessor.table_;
    auto frame = stack_frame_.get_stack_frame();
    if (table->stack_frame_contains_call_instruction(frame->symbol,
            *accessor_->table_accessor.table_->ir_instructions)) {
        auto imm = u32_int_immediate(0);
        accessor_->flag_accessor.set_instruction_flag(
            common::flag::Align, instruction_accessor->size());
    }
    accessor_->register_accessor.reset_available_registers();
}

/**
 * @brief IR Instruction Instruction::LOCL
 *
 *   Note that at this point we allocate local variables on the stack
 */
void Visitor::from_locl_ita(ir::Quadruple const& inst)
{
    auto locl_lvalue = std::get<1>(inst);
    auto frame = stack_frame_.get_stack_frame();
    auto& table = accessor_->table_accessor.table_;
    auto& stack = accessor_->stack;
    auto& locals = accessor_->table_accessor.table_->get_stack_frame_symbols();

    auto type_checker =
        ir::Type_Checker{ accessor_->table_accessor.table_, frame };

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
                auto type =
                    type_checker.get_type_from_rvalue_data_type(locl_lvalue);
                stack->set_address_from_type(locl_lvalue, type);
            }

    );
}

/**
 * @brief IR Instruction Instruction::PUSH
 */
void Visitor::from_push_ita(ir::Quadruple const& inst)
{
    auto& table = accessor_->table_accessor.table_;
    auto frame = stack_frame_.get_stack_frame();
    stack_frame_.argument_stack.emplace_front(
        table->lvalue_at_temporary_object_address(std::get<1>(inst), frame));
}

/**
 * @brief IR Instruction Instruction::POP
 */
void Visitor::from_pop_ita()
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
    auto caller_frame = stack_frame_.get_stack_frame();
    auto& table = accessor_->table_accessor.table_;
    for (auto const& rvalue : stack_frame_.argument_stack) {
        if (rvalue == "RET") {
            credence_assert(table->functions.contains(stack_frame_.tail));
            auto tail_frame = table->functions.at(stack_frame_.tail);
            if (accessor_->address_accessor.is_lvalue_storage_type(
                    tail_frame->ret->first, "string") or
                caller_frame->is_pointer_in_stack_frame(tail_frame->ret->first))
                arguments.emplace_back(Register::rax);
            else
                arguments.emplace_back(Register::eax);
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
                auto imm = common::assembly::
                    get_result_from_trivial_integral_expression(lhs, op, rhs);
                auto acc = accumulator.get_accumulator_register_from_size(
                    assembly::get_operand_size_from_rvalue_datatype(lhs));
                x8664_add__asm(instructions, mov, acc, imm);
            },
        m::pattern | m::app(type::is_relation_binary_operator, true) =
            [&] {
                auto imm = common::assembly::
                    get_result_from_trivial_relational_expression(lhs, op, rhs);
                auto acc = accumulator.get_accumulator_register_from_size(
                    Operand_Size::Byte);
                accessor_->set_signal_register(acc);
                x8664_add__asm(instructions, mov, acc, imm);
            },
        m::pattern | m::app(type::is_bitwise_binary_operator, true) =
            [&] {
                auto imm = common::assembly::
                    get_result_from_trivial_bitwise_expression(lhs, op, rhs);
                auto acc = accumulator.get_accumulator_register_from_size(
                    assembly::get_operand_size_from_rvalue_datatype(lhs));
                if (!accessor_->table_accessor.is_ir_instruction_temporary())
                    x8664_add__asm(instructions, mov, acc, imm);
                else
                    immediate_stack.emplace_back(imm);
            },
        m::pattern | m::_ = [&] { credence_error("unreachable"); });
}

/**
 * @brief Get the storage device of an parameter rvalue in the stack frame
 */
Storage Operand_Inserter::get_operand_storage_from_parameter(
    RValue const& rvalue)
{
    auto frame = stack_frame_.get_stack_frame();
    auto index_of = frame->get_index_of_parameter(rvalue);
    credence_assert_nequal(index_of, -1);
    // the argc and argv special cases
    if (frame->symbol == "main") {
        if (index_of == 0)
            return direct_immediate("[r15]");
        if (index_of == 1) {
            if (!is_vector_offset(rvalue))
                common::runtime::throw_runtime_error(
                    "invalid argv access, argv is a vector to strings", rvalue);
            auto offset = type::from_decay_offset(rvalue);
            if (!util::is_numeric(offset) and
                not accessor_->address_accessor.is_lvalue_storage_type(
                    offset, "int"))
                common::runtime::throw_runtime_error(
                    fmt::format(
                        "invalid argv access, argv has malformed offset '{}'",
                        offset),
                    rvalue);
            auto offset_integer = type::integral_from_type_ulint(offset) + 1;
            return direct_immediate(
                fmt::format("[r15 + 8 * {}]", offset_integer));
        }
    }
    if (frame->is_pointer_parameter(rvalue))
        return memory::registers::available_qword_register.at(index_of);
    else
        return memory::registers::available_dword_register.at(index_of);
}

/**
 * @brief Get the storage device of a stack operand
 */
inline Storage Operand_Inserter::get_operand_storage_from_stack(
    RValue const& rvalue)
{
    auto [operand, operand_inst] =
        accessor_->address_accessor
            .get_lvalue_address_and_insertion_instructions(
                rvalue, accessor_->instruction_accessor->size());
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    assembly::inserter(instructions, operand_inst);
    return operand;
}

/**
 * @brief Get the storage device of a return rvalue in the stack frame
 */
inline Storage Operand_Inserter::get_operand_storage_from_return()
{
    auto& tail_call =
        accessor_->table_accessor.table_->functions[stack_frame_.tail];
    if (tail_call->locals.is_pointer(tail_call->ret->first) or
        type::is_rvalue_data_type_string(tail_call->ret->first))
        return Register::rax;
    else
        return Register::eax;
}

/**
 * @brief Get the storage device of an immediate operand
 */
Storage Operand_Inserter::get_operand_storage_from_immediate(
    RValue const& rvalue)
{
    assembly::Storage storage{};
    auto immediate = type::get_rvalue_datatype_from_string(rvalue);
    auto type = type::get_type_from_rvalue_data_type(immediate);
    if (type == "string") {
        storage = assembly::make_asciz_immediate(accessor_->address_accessor
                .buffer_accessor.get_string_address_offset(
                    type::get_value_from_rvalue_data_type(immediate)));
        return storage;
    }
    if (type == "float") {
        storage = assembly::make_asciz_immediate(accessor_->address_accessor
                .buffer_accessor.get_float_address_offset(
                    type::get_value_from_rvalue_data_type(immediate)));
        return storage;
    }
    if (type == "double") {
        storage = assembly::make_asciz_immediate(accessor_->address_accessor
                .buffer_accessor.get_double_address_offset(
                    type::get_value_from_rvalue_data_type(immediate)));
        return storage;
    }
    storage = type::get_rvalue_datatype_from_string(rvalue);
    return storage;
}

/**
 * @brief Get the storage device of an rvalue operand
 */
Storage Operand_Inserter::get_operand_storage_from_rvalue(RValue const& rvalue)
{
    auto& stack = accessor_->stack;
    auto frame = stack_frame_.get_stack_frame();

    if (frame->is_parameter(rvalue))
        return get_operand_storage_from_parameter(rvalue);

    if (stack->is_allocated(rvalue))
        return get_operand_storage_from_stack(rvalue);

#if defined(CREDENCE_TEST) || defined(__linux__)
    if (!stack_frame_.tail.empty() and
        not common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::X8664))
        return get_operand_storage_from_return();

#elif defined(__APPLE__) || defined(__bsdi__)
    if (!stack_frame_.tail.empty() and
        not common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::X8664))
        return get_operand_storage_from_return();

#endif

    if (type::is_unary_expression(rvalue)) {
        auto unary_inserter =
            Unary_Operator_Inserter{ accessor_, stack_frame_ };
        return unary_inserter.from_temporary_unary_operator_expression(rvalue);
    }

    if (type::is_rvalue_data_type(rvalue))
        return get_operand_storage_from_immediate(rvalue);

    auto [operand, operand_inst] =
        accessor_->address_accessor
            .get_lvalue_address_and_insertion_instructions(
                rvalue, accessor_->instruction_accessor->size());
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    assembly::inserter(instructions, operand_inst);

    return operand;
}

/**
 * @brief IR Instruction Instruction::CALL
 */
void Visitor::from_call_ita(ir::Quadruple const& inst)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto inserter = Invocation_Inserter{ accessor_, stack_frame_ };
    auto function_name = type::get_label_as_human_readable(std::get<1>(inst));
    auto is_syscall_function = [&](Label const& label) {
#if defined(CREDENCE_TEST) || defined(__linux__)
        return common::runtime::is_syscall_function(label,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::X8664);

#elif defined(__APPLE__) || defined(__bsdi__)
        return common::runtime::is_syscall_function(label,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::X8664);
#endif
    };
    auto is_stdlib_function = [&](Label const& label) {
#if defined(CREDENCE_TEST) || defined(__linux__)
        return common::runtime::is_stdlib_function(label,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::X8664);

#elif defined(__APPLE__) || defined(__bsdi__)
        return common::runtime::is_stdlib_function(label,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::X8664);

#endif
    };
    m::match(function_name)(
        m::pattern | m::app(is_syscall_function, true) =
            [&] {
                inserter.insert_from_syscall_function(
                    function_name, instruction_accessor->get_instructions());
            },
        m::pattern | m::app(is_stdlib_function, true) =
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
    syscall_ns::common::make_syscall(
        instructions, routine, operands, &stack_frame_, &accessor_);
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
        if (is_variant(Immediate, operand)) {
            if (type::is_rvalue_data_type_string(
                    std::get<Immediate>(operand))) {
                accessor_->flag_accessor.set_instruction_flag(
                    common::flag::Load,
                    accessor_->instruction_accessor->size());
            }
        }
        if (size == Operand_Size::Qword)
            accessor_->flag_accessor.set_instruction_flag(
                common::flag::Argument,
                accessor_->instruction_accessor->size());
        x8664_add__asm(instructions, mov, storage, operand);
    }
    auto immediate = direct_immediate(routine);
    x8664_add__asm(instructions, call, immediate);
}

/**
 * @brief Invocation inserter for standard library functions and arguments
 */
void Invocation_Inserter::insert_from_standard_library_function(
    std::string_view routine,
    assembly::Instructions& instructions)
{
    auto operands = get_operands_storage_from_argument_stack();
    auto& argument_stack = stack_frame_.argument_stack;
    m::match(routine)(
        m::pattern | sv("putchar") = [&] {},
        m::pattern | sv("getchar") = [&] {},
        m::pattern | sv("print") =
            [&] {
                insert_type_check_stdlib_print_arguments(
                    argument_stack, operands);
            },
        m::pattern | sv("printf") =
            [&] {
                insert_type_check_stdlib_printf_arguments(
                    argument_stack, operands);
            });
    auto library_caller =
        runtime::Library_Call_Inserter{ accessor_, stack_frame_ };

    library_caller.make_library_call(
        instructions, routine, argument_stack, operands);
}

/**
 * @brief Insert and type check the argument instructions for the print
 * function
 */
void Invocation_Inserter::insert_type_check_stdlib_print_arguments(
    common::memory::Locals const& argument_stack,
    syscall_ns::syscall_arguments_t& operands)
{
    auto& table = accessor_->table_accessor.table_;
    auto& address_storage = accessor_->address_accessor;
    auto library_caller =
        runtime::Library_Call_Inserter{ accessor_, stack_frame_ };
    if (argument_stack.front() != "RET" and
        not argument_stack.front().starts_with("&"))
        if (!address_storage.is_lvalue_storage_type(
                argument_stack.front(), "string") and
            not library_caller.is_address_device_pointer_to_buffer(
                operands.front(), table, accessor_->stack))
            throw_compiletime_error(
                fmt::format("argument '{}' is not a valid buffer address",
                    argument_stack.front()),
                "print",
                __source__,
                "function invocation");
    auto buffer = operands.back();
    auto buffer_size =
        address_storage.buffer_accessor.has_bytes()
            ? address_storage.buffer_accessor.get_lvalue_string_size(
                  argument_stack.back(), stack_frame_)
            : address_storage.buffer_accessor.read_bytes();
    operands.emplace_back(u32_int_immediate(buffer_size));
}

/**
 * @brief Insert and type check the argument instructions for the printf
 * function
 */
void Invocation_Inserter::insert_type_check_stdlib_printf_arguments(
    common::memory::Locals const& argument_stack,
    syscall_ns::syscall_arguments_t& operands)
{
    auto& table = accessor_->table_accessor.table_;
    auto& address_storage = accessor_->address_accessor;
    auto library_caller =
        runtime::Library_Call_Inserter{ accessor_, stack_frame_ };

    if (argument_stack.front() == "RET" or
        type::is_rvalue_data_type_string(argument_stack.front()))
        return;
    if (!address_storage.is_lvalue_storage_type(
            argument_stack.front(), "string") and
        not library_caller.is_address_device_pointer_to_buffer(
            operands.front(), table, accessor_->stack))
        throw_compiletime_error(
            fmt::format("invalid format string '{}'", argument_stack.front()),
            "printf",
            __source__,
            "function invocation");
}

/**
 * @brief IR Instruction Instruction::MOV
 */
void Visitor::from_mov_ita(ir::Quadruple const& inst)
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
        // Parameters are prepared in the table, so skip parameter
        // lvalues
        m::pattern | m::ds(m::app(is_parameter, true), m::_) = [] {},
        // Translate an rvalue from a mutual-recursive temporary lvalue
        m::pattern | m::ds(m::app(type::is_temporary, true), m::_) =
            [&] {
                expression_inserter.insert_lvalue_at_temporary_object_address(
                    lhs);
            },
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

// Unused, used as rvalues
void Visitor::from_cmp_ita([[maybe_unused]] ir::Quadruple const& inst) {}
// Unused, read-ahead during relational jumps
void Visitor::from_if_ita([[maybe_unused]] ir::Quadruple const& inst) {}

/**
 * @brief IR Instruction Instruction::JNP_E
 */
void Visitor::from_jmp_e_ita(ir::Quadruple const& inst)
{
    auto [_, of, with, jump] = inst;
    auto frame = stack_frame_.get_stack_frame();
    auto of_comparator = frame->temporary.at(of).substr(4);
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto of_rvalue_storage = accessor_->address_accessor
                                 .get_lvalue_address_and_insertion_instructions(
                                     of_comparator, instructions.size())
                                 .first;
    auto with_rvalue_storage = type::get_rvalue_datatype_from_string(with);
    auto jump_label = assembly::make_label(jump, stack_frame_.symbol);
    auto comparator_instructions = assembly::r_eq(
        of_rvalue_storage, with_rvalue_storage, jump_label, Register::eax);
    assembly::inserter(instructions, comparator_instructions);
}

/**
 * @brief IR Instruction Instruction::GOTO
 */
void Visitor::from_goto_ita(ir::Quadruple const& inst)
{
    auto label = direct_immediate(
        assembly::make_label(std::get<1>(inst), stack_frame_.symbol));
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    x8664_add__asm(instructions, goto_, label);
}

/**
 * @brief Insert into a storage device from the %rip offset address of a string
 */
void Operand_Inserter::insert_from_string_address_operand(LValue const& lhs,
    Storage const& storage,
    RValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto expression_inserter = Expression_Inserter{ accessor_, stack_frame_ };
    auto imm = type::get_rvalue_datatype_from_string(rhs);
    expression_inserter.insert_from_string(
        type::get_value_from_rvalue_data_type(imm));
    if (is_variant(assembly::Stack::Offset, storage)) {
        auto offset = std::get<assembly::Stack::Offset>(storage);
        accessor_->stack->set(offset, Operand_Size::Qword);
    }
    accessor_->flag_accessor.set_instruction_flag(
        common::flag::QWord_Dest, instruction_accessor->size());
    accessor_->stack->set_address_from_accumulator(lhs, Register::rcx);
    x8664_add__asm(instructions, mov, storage, rcx);
}

/**
 * @brief Insert into a storage device from the %rip offset address of a float
 */
void Operand_Inserter::insert_from_float_address_operand(LValue const& lhs,
    Storage const& storage,
    RValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto expression_inserter = Expression_Inserter{ accessor_, stack_frame_ };
    auto imm = type::get_rvalue_datatype_from_string(rhs);
    expression_inserter.insert_from_float(
        type::get_value_from_rvalue_data_type(imm));
    if (is_variant(assembly::Stack::Offset, storage)) {
        auto offset = std::get<assembly::Stack::Offset>(storage);
        accessor_->stack->set(offset, Operand_Size::Qword);
    }
    accessor_->flag_accessor.set_instruction_flag(
        common::flag::QWord_Dest, instruction_accessor->size());
    accessor_->stack->set_address_from_accumulator(lhs, Register::xmm7);
    x8664_add__asm(instructions, movsd, storage, Register::xmm7);
}

/**
 * @brief Insert into a storage device from the %rip offset address of a double
 */
void Operand_Inserter::insert_from_double_address_operand(LValue const& lhs,
    Storage const& storage,
    RValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto expression_inserter = Expression_Inserter{ accessor_, stack_frame_ };
    auto imm = type::get_rvalue_datatype_from_string(rhs);
    expression_inserter.insert_from_double(
        type::get_value_from_rvalue_data_type(imm));
    if (is_variant(assembly::Stack::Offset, storage)) {
        auto offset = std::get<assembly::Stack::Offset>(storage);
        accessor_->stack->set(offset, Operand_Size::Qword);
    }
    accessor_->flag_accessor.set_instruction_flag(
        common::flag::QWord_Dest, instruction_accessor->size());
    accessor_->stack->set_address_from_accumulator(lhs, Register::xmm7);
    x8664_add__asm(instructions, movsd, storage, Register::xmm7);
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
                    address_storage
                        .get_lvalue_address_and_insertion_instructions(
                            lhs, instruction_accessor->size());
                assembly::inserter(instructions, storage_inst);
                if (type::get_type_from_rvalue_data_type(imm) == "string")
                    insert_from_string_address_operand(lhs, lhs_storage, rhs);
                else if (type::get_type_from_rvalue_data_type(imm) == "float")
                    insert_from_float_address_operand(lhs, lhs_storage, rhs);
                else if (type::get_type_from_rvalue_data_type(imm) == "double")
                    insert_from_double_address_operand(lhs, lhs_storage, rhs);
                else
                    x8664_add__asm(instructions, mov, lhs_storage, imm);
            },
        // the expanded temporary rvalue is in a accumulator register,
        // use it
        m::pattern | m::app(is_temporary, true) =
            [&] {
                if (address_storage.address_ir_assignment) {
                    address_storage.address_ir_assignment = false;
                    Storage lhs_storage = stack->get(lhs).first;
                    x8664_add__asm(
                        instructions, mov, lhs_storage, Register::rcx);
                } else {
                    auto acc = accumulator.get_accumulator_register_from_size();
                    if (!type::is_unary_expression(lhs))
                        stack->set_address_from_accumulator(lhs, acc);
                    auto [lhs_storage, storage_inst] =
                        address_storage
                            .get_lvalue_address_and_insertion_instructions(
                                lhs, instruction_accessor->size());
                    assembly::inserter(instructions, storage_inst);
                    x8664_add__asm(instructions, mov, lhs_storage, acc);
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
                x8664_add__asm(instructions, mov, acc, rhs_storage);
                x8664_add__asm(instructions, mov, lhs_storage, acc);
            },
        // Unary expression assignment, including pointers to an address
        m::pattern | m::app(type::is_unary_expression, true) =
            [&] {
                auto [lhs_storage, storage_inst] =
                    address_storage
                        .get_lvalue_address_and_insertion_instructions(
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
Storage Unary_Operator_Inserter::from_temporary_unary_operator_expression(
    RValue const& expr)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto& stack = accessor_->stack;
    auto& address_space = accessor_->address_accessor;
    auto& table_accessor = accessor_->table_accessor;
    auto& register_accessor = accessor_->register_accessor;

    credence_assert(type::is_unary_expression(expr));

    Storage storage{};

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
            return Register::rcx;
        }
        auto size = stack->get(rvalue).second;
        storage = table_accessor.next_ir_instruction_is_temporary() and
                          not table_accessor.last_ir_instruction_is_assignment()
                      ? register_accessor.get_second_register_from_size(size)
                      : accessor_->accumulator_accessor
                            .get_accumulator_register_from_size(size);
        x8664_add__asm(instructions, mov, storage, stack->get(rvalue).first);
        insert_from_unary_expression(op, storage);
    } else if (is_vector(rvalue)) {
        auto [address, address_inst] =
            address_space.get_lvalue_address_and_insertion_instructions(
                rvalue, false);
        assembly::inserter(instructions, address_inst);
        auto size = get_operand_size_from_storage(address, stack);
        storage = table_accessor.next_ir_instruction_is_temporary() and
                          not table_accessor.last_ir_instruction_is_assignment()
                      ? register_accessor.get_second_register_from_size(size)
                      : accessor_->accumulator_accessor
                            .get_accumulator_register_from_size(size);
        address_space.address_ir_assignment = true;
        accessor_->set_signal_register(Register::rax);
        insert_from_unary_expression(op, storage, address);
    } else {
        auto immediate = type::get_rvalue_datatype_from_string(rvalue);
        auto size = assembly::get_operand_size_from_rvalue_datatype(immediate);
        storage = table_accessor.next_ir_instruction_is_temporary() and
                          not table_accessor.last_ir_instruction_is_assignment()
                      ? register_accessor.get_second_register_from_size(size)
                      : accessor_->accumulator_accessor
                            .get_accumulator_register_from_size(size);
        x8664_add__asm(instructions, mov, storage, immediate);
        insert_from_unary_expression(op, storage);
    }
    return storage;
}

/**
 * @brief Construct a pair of Immediates from 2 RValue's
 */
inline auto get_rvalue_pair_as_immediate(RValue const& lhs, RValue const& rhs)
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
    x8664_add__asm(instructions, lea, rcx, location);
}

/**
 * @brief Expression inserter from float rvalue to a .float directive
 *
 * The buffer_accessor holds the %rip offset in the data section
 */
void Expression_Inserter::insert_from_float(RValue const& str)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    credence_assert(
        accessor_->address_accessor.buffer_accessor.is_allocated_float(str));
    auto location = assembly::make_asciz_immediate(
        accessor_->address_accessor.buffer_accessor.get_float_address_offset(
            str));
    x8664_add__asm(instructions, movsd, xmm7, location);
}

/**
 * @brief Expression inserter from double rvalue to a .double directive
 *
 * The buffer_accessor holds the %rip offset in the data section
 */
void Expression_Inserter::insert_from_double(RValue const& str)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    credence_assert(
        accessor_->address_accessor.buffer_accessor.is_allocated_double(str));
    auto location = assembly::make_asciz_immediate(
        accessor_->address_accessor.buffer_accessor.get_double_address_offset(
            str));
    x8664_add__asm(instructions, movsd, xmm7, location);
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
                    x8664_add__asm(
                        instructions, mov, lhs_s, stack->get(lhs).first);
                    rhs_s = stack->get(rhs).first;
                } else {
                    lhs_s = accumulator.get_accumulator_register_from_size(
                        stack->get(lhs).second);
                    x8664_add__asm(
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
                        x8664_add__asm(
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
                    x8664_add__asm(
                        instructions, mov, acc, stack->get(lhs).first);
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
                        x8664_add__asm(
                            instructions, mov, storage, stack->get(lhs).first);
                        lhs_s = storage;
                    } else {
                        if (!type::is_relation_binary_operator(op))
                            lhs_s = accumulator
                                        .get_accumulator_register_from_storage(
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
                    x8664_add__asm(
                        instructions, mov, acc, stack->get(rhs).first);
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
        assembly::Binary_Operands operands = { lhs_s, rhs_s };
        operand_inserter.insert_from_operands(operands, op);
    }
}

/**
 * @brief Operand Inserter mediator for expression mnemonics operands
 */
void Operand_Inserter::insert_from_operands(assembly::Binary_Operands& operands,
    std::string const& op)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    if (is_variant(Immediate, operands.first) and
        not assembly::is_immediate_r15_address_offset(operands.first) and
        not assembly::is_immediate_rip_address_offset(operands.first))
        std::swap(operands.first, operands.second);
    if (type::is_binary_arithmetic_operator(op)) {
        auto arithmetic = Arithemtic_Operator_Inserter{ accessor_ };
        assembly::inserter(instructions,
            arithmetic.from_arithmetic_expression_operands(operands, op)
                .second);
    } else if (type::is_relation_binary_operator(op)) {
        auto relational =
            Relational_Operator_Inserter{ accessor_, stack_frame_ };
        auto& ir_instructions =
            accessor_->table_accessor.table_->ir_instructions;
        auto ir_index = accessor_->table_accessor.index;
        if (ir_instructions->size() > ir_index and
            std::get<0>(ir_instructions->at(ir_index + 1)) ==
                ir::Instruction::IF) {
            auto label = assembly::make_label(
                std::get<3>(ir_instructions->at(ir_index + 1)),
                stack_frame_.symbol);
            assembly::inserter(instructions,
                relational.from_relational_expression_operands(
                    operands, op, label));
        }
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
void Expression_Inserter::insert_lvalue_at_temporary_object_address(
    LValue const& lvalue)
{
    auto frame = stack_frame_.get_stack_frame();
    auto& table = accessor_->table_accessor.table_;
    auto temporary = table->lvalue_at_temporary_object_address(lvalue, frame);
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
                    common::flag::Address, index);
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
                x8664_add__asm(instructions, mov, acc, dest);
                accessor_->flag_accessor.set_instruction_flag(
                    common::flag::Indirect, index);
                x8664_add__asm(instructions, mov, acc, src);
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

    auto is_comparator = [](RValue const& rvalue) {
        return rvalue.starts_with("CMP");
    };

    auto is_stdlib_function = [&](Label const& label) {
#if defined(CREDENCE_TEST) || defined(__linux__)
        return common::runtime::is_stdlib_function(label,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::X8664);

#elif defined(__APPLE__) || defined(__bsdi__)
        return common::runtime::is_stdlib_function(label,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::X8664);

#endif
    };

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
                x8664_add__asm(instructions, mov, acc, immediate);
                auto type = type::get_type_from_rvalue_data_type(rvalue);
                if (type == "string")
                    accessor_->flag_accessor.set_instruction_flag(
                        common::flag::Address, instruction_accessor->size());
            },
        m::pattern | m::app(is_comparator, true) =
            [&] {
                // @TODO nice-to-have, but not necessary
            },
        m::pattern | RValue{ "RET" } =
            [&] {
                if (is_stdlib_function(stack_frame_.tail))
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
                        common::flag::QWord_Dest, instruction_accessor->size());
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
                x8664_add__asm(instructions, mov, acc, immediate);
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
    ir::object::Function::Return_RValue const& ret)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto operand_inserter = Operand_Inserter{ accessor_, stack_frame_ };
    auto immediate =
        operand_inserter.get_operand_storage_from_rvalue(ret->second);
    if (get_operand_size_from_storage(immediate, accessor_->stack) ==
        Operand_Size::Qword)
        x8664_add__asm(instructions, mov, rax, immediate);
    else
        x8664_add__asm(instructions, mov, eax, immediate);
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
        accessor_->address_accessor
            .get_lvalue_address_and_insertion_instructions(
                lhs, instruction_accessor->size());
    assembly::inserter(instructions, lhs_storage_inst);
    auto [rhs_storage, rhs_storage_inst] =
        accessor_->address_accessor
            .get_lvalue_address_and_insertion_instructions(
                rhs, instruction_accessor->size());
    assembly::inserter(instructions, rhs_storage_inst);
    auto acc =
        accessor_->accumulator_accessor.get_accumulator_register_from_storage(
            lhs_storage, accessor_->stack);
    x8664_add__asm(instructions, mov, acc, rhs_storage);
    x8664_add__asm(instructions, mov, lhs_storage, acc);
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

        x8664_add__asm(instructions, mov, acc, rhs_storage);
        accessor_->flag_accessor.set_instruction_flag(
            common::flag::Indirect_Source | common::flag::Address,
            accessor_->instruction_accessor->size());
        x8664_add__asm(instructions, mov, temp, acc);
        x8664_add__asm(instructions, mov, acc, lhs_storage);

        accessor_->flag_accessor.set_instruction_flag(
            common::flag::Indirect, instruction_accessor->size());

        x8664_add__asm(instructions, mov, acc, temp);
    });
}

/**
 * @brief IR Instruction Instruction::RET
 */
void Visitor::from_return_ita()
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
void Visitor::from_leave_ita()
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    // care must be taken in the main function during function epilogue
    if (stack_frame_.symbol == "main") {
        if (accessor_->table_accessor.table_
                ->stack_frame_contains_call_instruction(stack_frame_.symbol,
                    *accessor_->table_accessor.table_->ir_instructions)) {
            auto size = u32_int_immediate(
                accessor_->stack->get_stack_frame_allocation_size());
            x8664_add__asm(accessor_->instruction_accessor->get_instructions(),
                add,
                rsp,
                size);
        }
        syscall_ns::common::exit_syscall(instructions, 0);
    } else {
        x8664_add__asm(instructions, pop, rbp);
        x8664_add__asm(instructions, ret);
    }
}

/**
 * @brief IR Instruction Instruction::LABEL
 */
void Visitor::from_label_ita(ir::Quadruple const& inst)
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
    assembly::Binary_Operands const& operands,
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
                x8664_add__asm(
                    instructions.second, mov, storage, operands.first);
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
                x8664_add__asm(
                    instructions.second, mov, storage, operands.first);
                instructions = assembly::mod(storage, operands.second);
            });
    return instructions;
}

/**
 * @brief Inserter of bitwise expressions and their storage device
 */
Instruction_Pair Bitwise_Operator_Inserter::from_bitwise_expression_operands(
    assembly::Binary_Operands const& operands,
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
assembly::Instructions
Relational_Operator_Inserter::from_relational_expression_operands(
    assembly::Binary_Operands const& operands,
    std::string const& binary_op,
    Label const& jump_label)
{
    auto register_storage = Register::eax;
    if (accessor_->address_accessor.is_qword_storage_size(
            operands.first, stack_frame_) or
        accessor_->address_accessor.is_qword_storage_size(
            operands.second, stack_frame_)) {
        register_storage = Register::rax;
    }

    return m::match(binary_op)(
        m::pattern | std::string{ "==" } =
            [&] {
                return assembly::r_eq(operands.first,
                    operands.second,
                    jump_label,
                    register_storage);
            },
        m::pattern | std::string{ "!=" } =
            [&] {
                return assembly::r_neq(operands.first,
                    operands.second,
                    jump_label,
                    register_storage);
            },
        m::pattern | std::string{ "<" } =
            [&] {
                return assembly::r_lt(operands.first,
                    operands.second,
                    jump_label,
                    register_storage);
            },
        m::pattern | std::string{ ">" } =
            [&] {
                return assembly::r_gt(operands.first,
                    operands.second,
                    jump_label,
                    register_storage);
            },
        m::pattern | std::string{ "<=" } =
            [&] {
                return assembly::r_le(operands.first,
                    operands.second,
                    jump_label,
                    register_storage);
            },
        m::pattern | std::string{ ">=" } =
            [&] {
                return assembly::r_ge(operands.first,
                    operands.second,
                    jump_label,
                    register_storage);
            },
        m::pattern | m::_ = [&] { return assembly::Instructions{}; });
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
    table->build_from_ir_instructions();
    auto stack = std::make_shared<assembly::Stack>();
    auto accessor = std::make_shared<memory::Memory_Accessor>(
        table->get_table_object(), stack);
    auto emitter = Assembly_Emitter{ accessor };
    emitter.text_.test_no_stdlib = no_stdlib;
    emitter.emit(os);
}
#endif

} // namespace x86_64