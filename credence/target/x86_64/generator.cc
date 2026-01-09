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

#include "assembly.h"                        // for newline, operator<<
#include "credence/target/common/flags.h"    // for Instruction_Flag, flags
#include "inserter.h"                        // for Instruction_Inserter
#include "memory.h"                          // for Memory_Accessor, Addres...
#include "stack.h"                           // for Stack
#include <credence/error.h>                  // for credence_assert
#include <credence/ir/ita.h>                 // for make_ita_instructions
#include <credence/ir/object.h>              // for Object, Label, RValue
#include <credence/ir/table.h>               // for Table
#include <credence/symbol.h>                 // for Symbol_Table
#include <credence/target/common/accessor.h> // for Buffer_Accessor
#include <credence/target/common/assembly.h> // for get_storage_as_string
#include <credence/target/common/memory.h>   // for Operand_Type
#include <credence/target/common/runtime.h>  // for get_library_symbols
#include <credence/types.h>                  // for get_value_from_rvalue_d...
#include <credence/util.h>                   // for is_variant, AST_Node
#include <cstddef>                           // for size_t
#include <deque>                             // for deque
#include <easyjson.h>                        // for JSON
#include <fmt/base.h>                        // for copy
#include <fmt/compile.h>                     // for format, operator""_cf
#include <matchit.h>                         // for Id, And, App, pattern
#include <memory>                            // for make_shared, shared_ptr
#include <ostream>                           // for basic_ostream, operator<<
#include <ostream>                           // for ostream
#include <string_view>                       // for basic_string_view
#include <tuple>                             // for get
#include <utility>                           // for get, pair
#include <variant>                           // for variant, visit, monostate
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
    auto inserter = Instruction_Inserter{ accessor_ };
    inserter.from_ir_instructions(ir_instructions_);
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