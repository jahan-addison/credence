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

#include "assembly.h"                        // for newline, tabwidth, oper...
#include "credence/target/common/flags.h"    // for Instruction_Flag, flags
#include "flags.h"                           // for ARM64_Instruction_Flag
#include "inserter.h"                        // for Instruction, Instructio...
#include "memory.h"                          // for Memory_Accessor, Addres...
#include "stack.h"                           // for Stack
#include <credence/error.h>                  // for credence_assert, creden...
#include <credence/ir/ita.h>                 // for make_ita_instructions
#include <credence/ir/object.h>              // for Function, Object, RValue
#include <credence/ir/table.h>               // for Table
#include <credence/symbol.h>                 // for Symbol_Table
#include <credence/target/common/accessor.h> // for Buffer_Accessor
#include <credence/target/common/assembly.h> // for direct_immediate, u32_i...
#include <credence/target/common/runtime.h>  // for get_library_symbols
#include <credence/types.h>                  // for get_value_from_rvalue_d...
#include <credence/util.h>                   // for sv, AST_Node, get_numbe...
#include <cstddef>                           // for size_t
#include <easyjson.h>                        // for JSON
#include <fmt/base.h>                        // for copy
#include <fmt/compile.h>                     // for format, operator""_cf
#include <fmt/format.h>                      // for format
#include <matchit.h>                         // for Id, App, And, app, pattern
#include <memory>                            // for make_shared, shared_ptr
#include <ostream>                           // for basic_ostream, operator<<
#include <ostream>                           // for ostream
#include <string_view>                       // for basic_string_view, oper...
#include <tuple>                             // for get, tuple
#include <utility>                           // for get, pair
#include <variant>                           // for variant, visit, get
#include <vector>                            // for vector

namespace credence::target::arm64 {

namespace m = matchit;

/**
 * @brief Assembly Emitter Factory
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
 * @brief Emit a complete arm64 program
 */
void Assembly_Emitter::emit(std::ostream& os)
{
    data_.set_data_section();
    auto inserter = Instruction_Inserter{ accessor_ };
    inserter.from_ir_instructions(ir_instructions_);
    text_.emit_text_section(os);
    data_.emit_data_section(os);
}

/**
 * @brief Emit the instructions for each directive type supported
 */
assembly::Directives Data_Emitter::get_instructions_from_directive_type(
    assembly::Directive directive,
    RValue const& rvalue)
{
    auto& address_storage = accessor_->address_accessor;
    assembly::Directives instructions;
    m::match(directive)(
        m::pattern | assembly::Directive::dword =
            [&] { instructions = assembly::xword(rvalue); },
        m::pattern | assembly::Directive::word =
            [&] { instructions = assembly::word(rvalue); },
        m::pattern | assembly::Directive::string =
            [&] {
                credence_assert(
                    address_storage.buffer_accessor.is_allocated_string(
                        rvalue));
                instructions = assembly::xword(
                    address_storage.buffer_accessor.get_string_address_offset(
                        rvalue));
            },
        m::pattern | assembly::Directive::space =
            [&] { instructions = assembly::zero(rvalue); },
        m::pattern | assembly::Directive::double_ =
            [&] {
                credence_assert(
                    address_storage.buffer_accessor.is_allocated_double(
                        rvalue));
                instructions = assembly::xword(
                    address_storage.buffer_accessor.get_double_address_offset(
                        rvalue));
            },
        m::pattern | assembly::Directive::float_ =
            [&] {
                credence_assert(
                    address_storage.buffer_accessor.is_allocated_float(rvalue));
                instructions = assembly::xword(
                    address_storage.buffer_accessor.get_float_address_offset(
                        rvalue));
            },
        m::pattern | assembly::Directive::align =
            [&] { instructions = assembly::align(rvalue); },
        m::pattern | m::_ =
            [&] {
                credence_error(fmt::format("unhandled directive type '{}'",
                    static_cast<int>(directive)));
            });
    return instructions;
}

/**
 * @brief Set string in the data section with .asciz directive
 */
void Data_Emitter::set_data_strings()
{
    auto& table = accessor_->table_accessor.table_;
    for (auto const& string : table->get_strings()) {
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
    for (auto const& floatz : table->get_floats()) {
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
    for (auto const& doublez : table->get_doubles()) {
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
    for (auto const& global : table->get_globals().get_pointers()) {
        credence_assert(table->get_vectors().contains(global));
        auto& vector = table->get_vectors().at(global);
        instructions_.emplace_back(global);
        auto address = type::semantic::Address{ 0 };
        for (auto const& item : vector->get_data()) {
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
 * @brief Emit the data section ARM64 instructions of a B language source
 */
void Data_Emitter::emit_data_section(std::ostream& os)
{
    assembly::newline(os, 1);
#if defined(__linux__)
    os << assembly::Directive::data;
#elif defined(CREDENCE_TEST) || defined(__APPLE__) || defined(__bsdi__)

    os << ".section	__TEXT,__cstring,cstring_literals";

#endif
    assembly::newline(os, 2);

    if (!instructions_.empty())
        for (std::size_t index = 0; index < instructions_.size(); index++) {
            auto data_item = instructions_[index];
            std::visit(
                util::overload{
                    [&](Label const& s) { os << s << ":" << std::endl; },
                    [&](assembly::Data_Pair const& s) {
                        os << assembly::tabwidth(4) << s.first;
                        if (s.first == assembly::Directive::asciz)
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
 * @brief Emit from a type::Data_Type as an immediate value
 */
constexpr std::string emit_immediate_storage(Immediate const& immediate)
{
    auto [v_value, v_type, v_size] = immediate;
    m::match(sv(v_type))(m::pattern | sv("char") = [&] {
        v_value = util::strip_char(v_value, '\'');
    });
    // ARM64 immediates use '#', except for labels/strings handled elsewhere
    if (sv(v_type) == sv("int") || sv(v_type) == sv("long"))
        return std::string{ "#" } + v_value;
    return v_value;
}

/**
 * @brief Emit a stack offset based on size, prefix, and instruction flags
 */
constexpr std::string emit_stack_storage(assembly::Stack::Offset offset,
    common::flag::flags flags)
{
    using namespace fmt::literals;
    std::string as_str{};
    if (flags & common::flag::Address)
        as_str = fmt::format("[sp, #{}]"_cf, offset);
    else
        as_str = fmt::format("[sp, #{}]"_cf, offset);
    return as_str;
}

/**
 * @brief Emit a register based on size, prefix, and instruction flags
 */
constexpr std::string emit_register_storage(Register device,
    common::flag::flags flags)
{
    std::string as_str{};
    using namespace fmt::literals;
    if (flags & common::flag::Indirect)
        as_str = fmt::format("[{}]"_cf,
            common::assembly::get_storage_as_string<Register>(device));
    else
        as_str = assembly::register_as_string(device);
    return as_str;
}

/**
 * @brief Emit the ARM64 assembly prologue
 */
void emit_arm64_assembly_prologue(std::ostream& os)
{
    os << ".align 3" << std::endl << std::endl;
}

/**
 * @brief Get the string representation of a storage device
 */
std::string Storage_Emitter::get_storage_device_as_string(
    Storage const& storage)
{
    m::Id<assembly::Stack::Offset> s;
    m::Id<assembly::Register> r;
    m::Id<assembly::Immediate> i;
    auto flags = accessor_->flag_accessor.get_instruction_flags_at_index(
        instruction_index_);
    return m::match(storage)(
        m::pattern | m::as<assembly::Stack::Offset>(
                         s) = [&] { return emit_stack_storage(*s, flags); },
        m::pattern | m::as<Register>(
                         r) = [&] { return emit_register_storage(*r, flags); },
        m::pattern |
            m::as<Immediate>(i) = [&] { return emit_immediate_storage(*i); });
}

/**
 * @brief Apply stack alignment via the flags added during instruction insertion
 */
void Storage_Emitter::apply_stack_alignment(Storage& operand,
    Mnemonic mnemonic,
    Source source,
    common::flag::flags flags)
{
    auto& stack = accessor_->stack;
    constexpr auto instruction_contains_flag = [](auto target_flag) {
        return [target_flag](
                   auto argument) { return (argument & target_flag) != 0; };
    };
    constexpr auto operand_source_is = [](auto target_source) {
        return [target_source](
                   auto argument) { return argument == target_source; };
    };
    auto frame = accessor_->stack_frame.get_stack_frame();
    if (is_alignment_mnemonic(mnemonic)) {
        m::match(flags, source)(

            m::pattern |
                m::ds(m::app(instruction_contains_flag(flag::Align), true),
                    m::app(operand_source_is(Source::s_2), true)) =
                [&] {
                    auto allocation =
                        stack->get_stack_frame_allocation_size(frame);
                    operand = u32_int_immediate(allocation);
                },
            m::pattern | m::ds(m::app(instruction_contains_flag(
                                          detail::flags::Align_S3_Folded),
                                   true),
                             m::app(operand_source_is(Source::s_3), true)) =
                [&] {
                    operand = u32_int_immediate(
                        stack->get_stack_frame_allocation_size(frame));
                },
            m::pattern | m::ds(m::app(instruction_contains_flag(
                                          detail::flags::Align_Folded),
                                   true),
                             m::app(operand_source_is(Source::s_1), true)) =
                [&] {
                    fmt::println("hmm: {}, {}",
                        frame->get_pointers().size(),
                        stack->get_stack_frame_allocation_size(frame) -
                            (frame->get_pointers().size() * 8));
                    operand = direct_immediate(fmt::format("[sp, #{}]",
                        stack->get_stack_frame_allocation_size(frame)));
                },
            m::pattern |
                m::ds(m::app(instruction_contains_flag(detail::flags::Align_SP),
                          true),
                    m::app(operand_source_is(Source::s_2), true)) =
                [&] {
                    auto allocation =
                        stack->get_stack_frame_allocation_size(frame);
                    operand = direct_immediate(
                        fmt::format("[sp, #-{}]!", allocation));
                },
            m::pattern | m::ds(m::app(instruction_contains_flag(
                                          detail::flags::Align_SP_Folded),
                                   true),
                             m::app(operand_source_is(Source::s_2), true)) =
                [&] {
                    operand = direct_immediate(fmt::format("[sp, #{}]",
                        stack->get_stack_frame_allocation_size(frame)));
                },
            m::pattern | m::ds(m::app(instruction_contains_flag(
                                          detail::flags::Align_SP_Local),
                                   true),
                             m::app(operand_source_is(Source::s_2), true)) =
                [&] {
                    operand = direct_immediate(fmt::format("[sp, #-{}!]",
                        stack->get_stack_frame_allocation_size(frame)));
                });
    }
}

/**
 * @brief Emit the operand of an mnemonic, Source controls which operand
 */
void Storage_Emitter::emit_mnemonic_operand(std::ostream& os,
    Storage const& operand,
    Mnemonic mnemonic,
    Source source,
    common::flag::flags flags)
{
    auto delimiter = source == Source::s_0 ? " " : ", ";

    if ((flags & flag::Load) and is_variant(Immediate, operand)) {
        auto [value, type, size] = std::get<Immediate>(operand);
        if (source != Source::s_0 and type == "string" and
            mnemonic == arm_mn(ldr)) {
            os << ", =" << get_storage_device_as_string(operand);
        } else {
            os << delimiter << get_storage_device_as_string(operand);
        }
    } else {
        os << delimiter << get_storage_device_as_string(operand);
    }
}

/**
 * @brief Emit the representation of an mnemonic operand
 *
 *    Apply all flags set on the instruction index during code translation
 */
void Storage_Emitter::emit(std::ostream& os,
    Storage const& storage,
    Mnemonic mnemonic,
    Source source)
{
    auto& flag_accessor = accessor_->flag_accessor;
    auto flags =
        flag_accessor.get_instruction_flags_at_index(instruction_index_);

    if (is_variant(std::monostate, storage))
        return;

    auto operand = storage;

    apply_stack_alignment(operand, mnemonic, source, flags);
    if (flags & flag::Indirect_Source and source == Source::s_1)
        flag_accessor.set_instruction_flag(flag::Indirect, instruction_index_);
    emit_mnemonic_operand(os, operand, mnemonic, source, flags);
    flag_accessor.unset_instruction_flag(flag::Indirect, instruction_index_);
}

/**
 * @brief Emit the  jump to the last branch that ends the function
 */
void Text_Emitter::emit_epilogue_jump(std::ostream& os)
{
    os << assembly::tabwidth(4) << Mnemonic::b << " ";
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
    if (table->get_hoisted_symbols().has_key(s) and
        table->get_hoisted_symbols()[s]["type"].to_string() ==
            "function_definition") {
        // this is a new frame, emit the last frame function epilogue
        if (frame_ != s) {
            emit_function_epilogue(os);
            accessor_->device_accessor.set_current_frame_symbol(s);
        }
        frame_ = s;
        if (set_label)
            label_size_ = accessor_->table_accessor.table_->get_functions()
                              .at(s)
                              ->get_labels()
                              .size();
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
            table->get_functions().at(frame_)->get_label_before_reserved();
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
 * @brief Do we need to save the callee-saved registers on the stack?
 *
 */
bool Text_Emitter::emit_callee_saved_registers_stp(Mnemonic mnemonic,
    std::ostream& os,
    std::size_t index)
{
    auto flags = accessor_->flag_accessor.get_instruction_flags_at_index(index);
    auto stack_frame = accessor_->table_accessor.table_->get_stack_frame(
        accessor_->device_accessor.get_current_frame_name());

    if (flags & detail::flags::Callee_Saved)
        if (!stack_frame->get_tokens().contains("x23") and
            not stack_frame->get_tokens().contains("x26"))
            return false;

    if (flags & detail::flags::Callee_Saved and mnemonic == Mnemonic::stp) {
        if (stack_frame->get_tokens().contains("x23") and
            stack_frame->get_tokens().contains("x26")) {
            // both true, let through
            return true;
        }
        if (stack_frame->get_tokens().contains("x23") and
            not stack_frame->get_tokens().contains("x26")) {
            os << assembly::tabwidth(4);
            os << "str x23, [sp, #16]" << std::endl;
            return false;
        }
        if (stack_frame->get_tokens().contains("x26") and
            not stack_frame->get_tokens().contains("x23")) {
            os << assembly::tabwidth(4);
            os << "str x26, [sp, #16]" << std::endl;
            return false;
        }
    }

    if (flags & detail::flags::Callee_Saved and mnemonic == Mnemonic::ldp) {
        if (stack_frame->get_tokens().contains("x23") and
            stack_frame->get_tokens().contains("x26")) {
            // both true, let through
            return true;
        }
        if (stack_frame->get_tokens().contains("x23") and
            not stack_frame->get_tokens().contains("x26")) {
            os << assembly::tabwidth(4);
            os << "ldr x23, [sp, #16]" << std::endl;
            return false;
        }
        if (stack_frame->get_tokens().contains("x26") and
            not stack_frame->get_tokens().contains("x23")) {
            os << assembly::tabwidth(4);
            os << "ldr x26, [sp, #16]" << std::endl;
            return false;
        }
    }

    return true;
}

/**
 * @brief Emit a mnemonic and its possible operands in the text section
 */
void Text_Emitter::emit_assembly_instruction(std::ostream& os,
    std::size_t index,
    Instruction const& s)
{
    auto [mnemonic, src1, src2, src3, src4] = s;
    auto storage_emitter = Storage_Emitter{ accessor_, index };

    if (branch_ == "_L1" && label_size_ > 0) {
        return_instructions_.emplace_back(s);
        return;
    }
    if (!emit_callee_saved_registers_stp(mnemonic, os, index))
        return;

    os << assembly::tabwidth(4) << mnemonic;

    storage_emitter.emit(os, src1, mnemonic, Storage_Emitter::Source::s_0);
    storage_emitter.emit(os, src2, mnemonic, Storage_Emitter::Source::s_1);
    storage_emitter.emit(os, src3, mnemonic, Storage_Emitter::Source::s_2);
    storage_emitter.emit(os, src4, mnemonic, Storage_Emitter::Source::s_3);

    assembly::newline(os, 1);
}

/**
 * @brief Emit the text instruction for either a label or mnemonic
 */
void Text_Emitter::emit_text_instruction(std::ostream& os,
    std::variant<Label, Instruction> const& instruction,
    std::size_t index,
    bool set_label)
{
    // clang-format off
    std::visit(util::overload{
        [&](Instruction const& s) {
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
    const Instructions instructions = instructions_accessor->get_instructions();
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
    assembly::newline(os, 1);
#if defined(__linux__)
    os << assembly::Directive::text << std::endl;
#elif defined(CREDENCE_TEST) || defined(__APPLE__) || defined(__bsdi__)
    os << ".section	__TEXT,__text,regular,pure_instructions" << std::endl;
#endif
    assembly::newline(os, 1);
    os << assembly::tabwidth(4);
    emit_arm64_assembly_prologue(os);
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
            os << assembly::tabwidth(4) << assembly::Directive::global << " ";
            os << "_" << stdlib_f;
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

} // namespace credence::target::arm64