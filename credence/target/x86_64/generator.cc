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
#include <credence/ir/table.h>               // for make_table, Table
#include <credence/object.h>                 // for Object, Label, RValue
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

/****************************************************************************
 * x86-64 Code Generator
 *
 * Generates Intel syntax x86-64 assembly for Linux and Darwin. A call to the
 * standard library follows the System V ABI, and a function of the program
 * takes its arguments in rdi, r8, r9, rsi, rdx, and rcx. The IR is
 * translated into the instructions of the text section, then the emitter
 * resolves the frame size of each function and writes the text and data
 * sections.
 *
 *  Example:
 *
 *    add(x, y) {
 *      return(x + y);
 *    }
 *    counter 5;
 *
 *  Becomes:
 *
 *    add:
 *        push rbp
 *        mov rbp, rsp
 *        mov eax, edi
 *        add eax, r8d
 *        mov eax, eax
 *        leave
 *        ret
 *
 *    .data
 *    counter:
 *        .long 5
 *
 ****************************************************************************/

/****************************************************************************
 * Register selection table:
 *
 *   rax      = The default "accumulator" register for expression expansion,
 *              the return value, and the number of a syscall
 *   rdi, r8, r9, rsi, rdx, rcx
 *            = The arguments of a call to a function of the program
 *   rdi, rsi, rdx, rcx, r8, r9
 *            = The arguments of a call to the standard library, System V
 *   rdi, rsi, rdx, r10, r8, r9
 *            = The arguments of a syscall
 *   xmm0 - xmm7
 *            = The float and double arguments of the standard library
 *   xmm7     = The store register of a float or double literal
 *   rcx      = The address register of the address-of operator, and the
 *              shift count of a shift by a register, in cl
 *   rdx      = The high half of the dividend and the remainder of `idiv`
 *   r10      = The address register of a dereference
 *   r11      = The scratch register of the right-hand-side of a binary
 *              expression, the divisor of `idiv`, and the value of a
 *              global argument
 *   r15      = The address of argc, argv on the stack
 *   rbp      = The frame pointer
 *
 *   Locals, parameters, vectors, and vector offsets are always on the
 *   stack below rbp
 *
 *****************************************************************************/

namespace credence::target::x86_64 {

namespace m = matchit;

/**
 * @brief Emit the x86-64 assembly of a program
 *
 * Emit a complete x86-64 program from an AST and symbols
 */
void emit(std::ostream& os,
    util::AST_Node const& symbols,
    util::AST_Node const& ast,
    bool no_stdlib)
{
    auto table = ir::make_table(symbols, ast);
    auto stack = std::make_shared<assembly::Stack>();
    auto accessor = std::make_shared<memory::Memory_Accessor>(
        table->get_table_object(), table->get_table_instructions(), stack);
    auto emitter = Assembly_Emitter{ accessor };
    emitter.text_.test_no_stdlib = no_stdlib;
    emitter.emit(os);
}

/**
 * @brief Emit an x86-64 program
 *
 * The data section is set first, so the literals have labels when the text
 * section is translated
 */
void Assembly_Emitter::emit(std::ostream& os)
{
    emit_x86_64_assembly_intel_prologue(os);
    data_.set_data_section();
    auto inserter = Instruction_Inserter{ accessor_ };
    inserter.from_ir_instructions(ir_instructions_);
    text_.emit_text_section(os);
    data_.emit_data_section(os);
}

/**
 * @brief Emit an immediate, a char without its quotes
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
 * @brief Emit a stack offset below rbp with its size prefix
 *
 * An argument slot is a quadword, and an address has no size prefix, such
 * as the operand of `lea`
 *
 *  Examples:
 *
 *    emit_stack_storage(4, Operand_Size::Dword, 0)       // dword ptr [rbp - 4]
 *    emit_stack_storage(4, Operand_Size::Dword, Address) // [rbp - 4]
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
 * @brief Emit a register, or the memory it addresses when it is indirect
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
inline void emit_x86_64_assembly_intel_prologue(std::ostream& os)
{
    os << std::endl << ".intel_syntax noprefix" << std::endl << std::endl;
}

/**
 * @brief Emit the alignment directive
 */
inline void emit_alignment_directive(std::ostream& os,
    std::size_t align,
    std::size_t newline)
{
    os << assembly::tabwidth(4) << ".p2align " << align;
    assembly::newline(os, newline);
}

/**
 * @brief Insert the alignment directive into the data section
 */
inline void insert_alignment_directive(assembly::Directives& instructions,
    std::size_t align)
{
    assembly::inserter(instructions, assembly::p2align(std::to_string(align)));
}

/**
 * @brief Get the data directive of a value in the data section
 *
 * A string, float, or double is the address of its label
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
 * @brief Set the data section of the program
 */
void Data_Emitter::set_data_section()
{
    set_data_floats();
    set_data_doubles();
    set_data_strings();
    set_data_globals();
}

/**
 * @brief Set each string in the data section with the .asciz directive
 */
void Data_Emitter::set_data_strings()
{
    auto& table = accessor_->table_accessor.get_table();
    for (auto const& string : table->strings) {
        auto data_instruction =
            assembly::asciz(accessor_->address_accessor.buffer_accessor
                                .get_constant_size_index(),
                string);
        accessor_->address_accessor.buffer_accessor.insert_string_literal(
            string, data_instruction.first);
        assembly::inserter(instructions_, data_instruction.second);
    }
}

/**
 * @brief Set each float in the data section with the .float directive
 */
void Data_Emitter::set_data_floats()
{
    auto& table = accessor_->table_accessor.get_table();
    if (!table->floats.empty())
        insert_alignment_directive(instructions_, 2);
    for (auto const& floatz : table->floats) {
        auto data_instruction =
            assembly::floatz(accessor_->address_accessor.buffer_accessor
                                 .get_constant_size_index(),
                floatz);
        accessor_->address_accessor.buffer_accessor.insert_float_literal(
            floatz, data_instruction.first);
        assembly::inserter(instructions_, data_instruction.second);
    }
}

/**
 * @brief Set each double in the data section with the .double directive
 */
void Data_Emitter::set_data_doubles()
{
    auto& table = accessor_->table_accessor.get_table();
    if (!table->doubles.empty())
        insert_alignment_directive(instructions_, 3);
    for (auto const& doublez : table->doubles) {
        auto data_instruction =
            assembly::doublez(accessor_->address_accessor.buffer_accessor
                                  .get_constant_size_index(),
                doublez);
        accessor_->address_accessor.buffer_accessor.insert_double_literal(
            doublez, data_instruction.first);
        assembly::inserter(instructions_, data_instruction.second);
    }
}

/**
 * @brief Get the `.p2align` of a value of a type, the log2 of its alignment
 */
std::size_t get_alignment_size_from_rvalue_data_type(
    type::semantic::Type const& type)
{
    using T = type::semantic::Type;
    return m::match(type)(
        m::pattern |
            m::or_(T{ "int" }, T{ "float" }, T{ "long" }) = [&] { return 2UL; },
        m::pattern | T{ "char" } = [&] { return 1UL; },
        m::pattern | m::or_(T{ "string" }, T{ "double" }) = [&] { return 3UL; },
        m::pattern | m::_ = [&] { return 3UL; });
}

/**
 * @brief Set each global in the data section
 *
 * A global is a vector, and a global with one value, `counter 5;`, is
 * aligned to its type. The offset of each element is recorded for its
 * loads and stores.
 */
void Data_Emitter::set_data_globals()
{
    auto& table = accessor_->table_accessor.get_table();
    for (auto const& global : table->globals.get_pointers()) {
        credence_assert(table->vectors.contains(global));
        auto& vector = table->vectors.at(global).get();
        if (vector.size == 1) {
            auto align = get_alignment_size_from_rvalue_data_type(
                type::get_type_from_rvalue_data_type(vector.data.at("0")));
            insert_alignment_directive(instructions_, align);
        } else {
            insert_alignment_directive(instructions_, 3);
        }
        instructions_.emplace_back(global);
        auto address = type::semantic::Address{ 0 };
        for (auto const& item : vector.data) {
            auto directive =
                assembly::get_data_directive_from_rvalue_type(item.second);
            auto data = type::get_value_from_rvalue_data_type(item.second);
            vector.set_address_offset(item.first, address);
            address += assembly::get_size_from_operand_size(
                assembly::get_operand_size_from_rvalue_datatype(item.second));

            auto instructions =
                get_instructions_from_directive_type(directive, data);

            assembly::inserter(instructions_, instructions);
        }
    }
}

/**
 * @brief Emit the data section
 */
void Data_Emitter::emit_data_section(std::ostream& os)
{
    assembly::newline(os, 1);
    os << assembly::Directive::data;
    assembly::newline(os, 1);

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
    assembly::newline(os);
}

/**
 * @brief Get a storage device as a string, with the flags of its instruction
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
 * @brief Emit an operand of an instruction with its flags applied
 *
 * The size of a memory operand is the size of the other operand, and the
 * immediate of a stack pointer `sub` or `add` is the frame size, which is
 * known after the function is translated
 *
 *  Example:
 *
 *    sub rsp, 0      // flag::Align
 *
 *  Becomes:
 *
 *    sub rsp, 16
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
            // a `movzx` from the stack widens a char, which is a byte
            if (mnemonic == x64_mn(movzx) and
                is_variant(assembly::Stack::Offset, source))
                set_address_size(Operand_Size::Byte);
            os << ", " << get_storage_device_as_string(source, size);
            flag_accessor.unset_instruction_flag(
                common::flag::Indirect, instruction_index_);
            reset_address_size();
        }
    }
}

/**
 * @brief Emit the jump to the epilogue of the function, `_L1`
 */
void Text_Emitter::emit_epilogue_jump(std::ostream& os)
{
    os << assembly::tabwidth(4) << assembly::Mnemonic::goto_ << " ";
    os << assembly::make_label("_L1", frame_);
    assembly::newline(os, 1);
}

/**
 * @brief Emit a function or branch label in the text section
 *
 * A function label emits the epilogue of the last function, and a branch
 * label is local to its function. The `_L1` label is the epilogue, so a
 * jump to it is emitted in its place.
 */
void Text_Emitter::emit_assembly_label(std::ostream& os,
    Label const& s,
    bool set_label)
{
    auto& table = accessor_->table_accessor.get_table();
    // function labels
    if (table->hoisted_symbols.has_key(s) and
        table->hoisted_symbols[s]["type"].to_string() ==
            "function_definition") {
        // this is a new frame, emit the last frame function epilogue
        if (frame_ != s)
            emit_function_epilogue(os);
        frame_ = s;
        if (set_label)
            label_size_ = accessor_->table_accessor.get_table()
                              ->functions.at(s)
                              .get()
                              .labels.size();
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
            table->functions.at(frame_).get().label_before_reserved;
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
 * @brief Emit an instruction and its operands in the text section
 *
 * An instruction after the `_L1` label is held for the epilogue, and a
 * `mov` of an address is a `lea`
 */
void Text_Emitter::emit_assembly_instruction(std::ostream& os,
    std::size_t index,
    assembly::Instruction const& s)
{
    auto flags = accessor_->flag_accessor.get_instruction_flags_at_index(index);
    auto [mnemonic, dest, src] = s;
    // a memory operand is the size of a register destination, except for
    // the extending moves
    auto is_extended = mnemonic == assembly::Mnemonic::movzx or
                       mnemonic == assembly::Mnemonic::movsxd;
    auto& operand_size_storage =
        is_variant(assembly::Register, dest) and
                is_variant(assembly::Stack::Offset, src) and not is_extended
            ? dest
            : src;
    auto storage_emitter =
        Storage_Emitter{ accessor_, index, operand_size_storage };
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
 * @brief Emit a label or an instruction in the text section
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
 * @brief Emit the epilogue held for the last function
 *
 * The `_L1` label of a function with branches is emitted before its
 * epilogue
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
 * @brief Emit the text section
 *
 * The epilogue of the last function is emitted after its instructions
 */
void Text_Emitter::emit_text_section(std::ostream& os)
{
    auto instructions_accessor = accessor_->instruction_accessor;
    auto& instructions = instructions_accessor->get_instructions();
    emit_text_directives(os);
    for (std::size_t index = 0; index < instructions_accessor->size(); index++)
        emit_text_instruction(os, instructions[index], index);
    if (!return_instructions_.empty())
        emit_function_epilogue(os);
}

/**
 * @brief Emit the Intel syntax, text section, and entry point directives
 */
void Text_Emitter::emit_text_directives(std::ostream& os)
{
    os << assembly::Directive::text << std::endl;
    assembly::newline(os, 1);
    emit_alignment_directive(os, 4, 2);
    os << assembly::tabwidth(4) << assembly::Directive::start;
    assembly::newline(os, 1);
    emit_stdlib_externs(os);
}

/**
 * @brief Emit the global directive of each standard library function
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

} // namespace x86_64