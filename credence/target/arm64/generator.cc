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
#include <credence/ir/table.h>               // for make_table, Table
#include <credence/object.h>                 // for Function, Object, RValue
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

/****************************************************************************
 * ARM64 Code Generator
 *
 * Generates ARM64 assembly for Linux and Darwin under the ARM64 Procedure
 * Call Standard (PCS). The IR is translated into the instructions of the
 * text section, then the emitter resolves the frame size of each function
 * and writes the text and data sections.
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
 *        stp x29, x30, [sp, #-16]!
 *        mov x29, sp
 *        mov w8, w0
 *        add w8, w8, w1
 *        mov w0, w8
 *        ldp x29, x30, [sp], #16
 *        ret
 *
 *    .section	__DATA,__data
 *        .p2align 2
 *    counter:
 *        .long 5
 *
 ****************************************************************************/

/****************************************************************************
 * Register selection table:
 *
 *   x0 - x7  = The arguments of a call, and x0 the return value
 *   x6       = The data section address register, and the second operand
 *              of a binary expression in a temporary
 *      s6    = floating point
 *      d6    = double
 *   x7       = The scratch register of the right-hand-side of a binary
 *              expression
 *   x8       = The default "accumulator" register for expression expansion
 *   x9, x11 - x14, x16, x17
 *            = If there are no function calls in a stack frame, local scope
 *              variables are stored in these registers, after which the
 *              stack is used
 *   x10      = The stack move register, and the address register of a
 *              pointer on the stack
 *   x15      = The address of a vector element on the stack
 *   x18      = The platform register on Darwin, never used
 *   x19      = The argc, argv scratch register
 *
 *   A local whose address is taken, vectors, and vector offsets are always
 *   on the stack
 *
 *****************************************************************************/

namespace credence::target::arm64 {

namespace m = matchit;

/**
 * @brief Emit the ARM64 assembly of a program
 *
 * The object table and IR are built by the semantic pass and the ITA, and
 * the standard library symbols are added unless they are disabled
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
 * @brief Emit an ARM64 program
 *
 * The data section is set first, so the literals have labels when the text
 * section is translated
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
 * @brief Get the data directive of a value in the data section
 *
 * A string, float, or double is the address of its label
 *
 *  Example:
 *
 *    mess [2] "too bad", "tough luck";
 *
 *  Becomes:
 *
 *    mess:
 *        .xword ._L_str1__
 *        .xword ._L_str2__
 */
assembly::Directives Data_Emitter::get_instructions_from_directive_type(
    assembly::Directive directive,
    RValue const& rvalue)
{
    using d = enum assembly::Directive;
    auto& address_storage = accessor_->address_accessor;
    assembly::Directives instructions;

    m::match(directive)(
        m::pattern | d::dword = [&] { instructions = assembly::xword(rvalue); },
        m::pattern | d::word = [&] { instructions = assembly::word(rvalue); },
        m::pattern | d::long_ = [&] { instructions = assembly::long_(rvalue); },
        m::pattern | m::or_(d::string, d::xword) =
            [&] {
                credence_assert(
                    address_storage.buffer_accessor.is_allocated_string(
                        rvalue));
                instructions = assembly::xword(
                    address_storage.buffer_accessor.get_string_address_offset(
                        rvalue));
            },
        m::pattern | d::space = [&] { instructions = assembly::zero(rvalue); },
        m::pattern | d::double_ =
            [&] {
                credence_assert(
                    address_storage.buffer_accessor.is_allocated_double(
                        rvalue));
                instructions = assembly::xword(
                    address_storage.buffer_accessor.get_double_address_offset(
                        rvalue));
            },
        m::pattern | d::float_ =
            [&] {
                credence_assert(
                    address_storage.buffer_accessor.is_allocated_float(rvalue));
                instructions = assembly::xword(
                    address_storage.buffer_accessor.get_float_address_offset(
                        rvalue));
            },
        m::pattern | d::align = [&] { instructions = assembly::align(rvalue); },
        m::pattern | m::_ =
            [&] {
                credence_error(fmt::format("unsupported directive type '{}'",
                    static_cast<int>(directive)));
            });
    return instructions;
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
 * @brief Set each string in the data section with the .asciz directive
 *
 * Strings are after the floats and doubles, since they have no alignment
 */
void Data_Emitter::set_data_strings()
{
    index_before_strings = instructions_.size();
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
    index_after_strings = instructions_.size();
}

/**
 * @brief Set each float in the data section with the .float directive
 */
void Data_Emitter::set_data_floats()
{
    auto& table = accessor_->table_accessor.get_table();
    if (!table->floats.empty())
        insert_arm64_alignment_directive(instructions_, 2);
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
        insert_arm64_alignment_directive(instructions_, 3);
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
        // trivial vectors: e.g. `num 1;`
        if (vector.size == 1) {
            auto align = get_alignment_size_from_rvalue_data_type(
                type::get_type_from_rvalue_data_type(vector.data.at("0")));
            insert_arm64_alignment_directive(instructions_, align);
        } else {
            insert_arm64_alignment_directive(instructions_, 3);
        }
        instructions_.emplace_back(global);
        auto address = type::semantic::Address{ 0 };
        for (auto const& item : vector.data) {
            auto directive =
                assembly::get_data_directive_from_rvalue_type(item.second);
            auto data = type::get_value_from_rvalue_data_type(item.second);
            vector.set_address_offset(item.first, address);
            auto type = type::get_type_from_rvalue_data_type(item.second);
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
 *
 * On Darwin the strings are in the cstring section and the globals in the
 * data section
 */
void Data_Emitter::emit_data_section(std::ostream& os)
{
    assembly::newline(os, 1);
#if defined(__linux__)
    os << assembly::Directive::data;
#elif defined(__APPLE__) || defined(__bsdi__)
    os << ".section\t__TEXT,__const";
#elif defined(_WIN32) || defined(_WIN64)
    os << assembly::Directive::data;
#endif

    assembly::newline(os, 2);

    if (!instructions_.empty())
        for (std::size_t index = 0; index < instructions_.size(); index++) {
            auto data_item = instructions_[index];
            if (index == index_before_strings) {
#if defined(__APPLE__) || defined(__bsdi__)
                os << ".section\t__TEXT,__cstring,cstring_literals";
                assembly::newline(os, 2);
#endif
            }
            if (index == index_after_strings) {
#if defined(__APPLE__) || defined(__bsdi__)
                os << ".section\t__DATA,__data";
                assembly::newline(os, 2);
#endif
            }
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
                        if (s.first == Directive::align or
                            s.first == Directive::p2align)
                            assembly::newline(os);
                        if (index < instructions_.size() - 1)
                            assembly::newline(os);
                    },
                },
                data_item);
        }
}

/**
 * @brief Emit an immediate, with `#` before an integer
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
 * @brief Emit a stack offset from the stack pointer, `[sp, #16]`
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
 * @brief Emit a register, or the memory it addresses when it is indirect
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
 * @brief Emit the alignment directive
 */
inline void emit_arm64_alignment_directive(std::ostream& os,
    std::size_t align,
    std::size_t newline)
{
    os << assembly::tabwidth(4) << ".p2align " << align;
    assembly::newline(os, newline);
}

/**
 * @brief Insert the alignment directive into the data section
 */
void insert_arm64_alignment_directive(assembly::Directives& instructions,
    std::size_t align)
{
    assembly::inserter(instructions, assembly::p2align(std::to_string(align)));
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
 * @brief Get a storage device as a string, with the flags of its instruction
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
 * @brief Resolve an operand that depends on the frame size
 *
 * The frame size of a function is known after it is translated, so an
 * operand flagged during insertion is replaced here, such as the
 * pre-indexed store of the frame record
 *
 *  Example:
 *
 *    stp x29, x30, [sp, #0]      // Align_SP
 *
 *  Becomes:
 *
 *    stp x29, x30, [sp, #-32]!
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
    auto& frame =
        accessor_->table_accessor.get_table()->get_stack_frame(frame_name_);
    if (is_alignment_mnemonic(mnemonic)) {
        m::match(flags, source)(

            m::pattern |
                m::ds(m::app(instruction_contains_flag(flag::Align), true),
                    m::app(operand_source_is(Source::s_2), true)) =
                [&] {
                    auto allocation =
                        stack->get_stack_frame_allocation_size(frame_name_);
                    operand = u32_int_immediate(allocation);
                },
            m::pattern | m::ds(m::app(instruction_contains_flag(
                                          detail::flags::Align_S2_Folded),
                                   true),
                             m::app(operand_source_is(Source::s_2), true)) =
                [&] {
                    operand = u32_int_immediate(
                        stack->get_stack_frame_allocation_size(frame_name_));
                },
            m::pattern | m::ds(m::app(instruction_contains_flag(
                                          detail::flags::Align_S3_Folded),
                                   true),
                             m::app(operand_source_is(Source::s_3), true)) =
                [&] {
                    operand = u32_int_immediate(
                        stack->get_stack_frame_allocation_size(frame_name_));
                },
            m::pattern | m::ds(m::app(instruction_contains_flag(
                                          detail::flags::Align_Folded),
                                   true),
                             m::app(operand_source_is(Source::s_1), true)) =
                [&] {
                    auto offset_index = frame.pointers.size() >= 1 ? 1 : 0;
                    if (*address_pointer_index > 0UL) {
                        auto offset =
                            stack->get_stack_frame_allocation_size(
                                frame_name_) -
                            ((frame.pointers.size() - *address_pointer_index +
                                 offset_index) *
                                8);
                        operand =
                            direct_immediate(fmt::format("[sp, #{}]", offset));
                        (*address_pointer_index)--;
                    }
                },
            m::pattern |
                m::ds(m::app(instruction_contains_flag(detail::flags::Align_SP),
                          true),
                    m::app(operand_source_is(Source::s_2), true)) =
                [&] {
                    auto allocation =
                        stack->get_stack_frame_allocation_size(frame_name_);
                    operand = direct_immediate(
                        fmt::format("[sp, #-{}]!", allocation));
                },
            m::pattern | m::ds(m::app(instruction_contains_flag(
                                          detail::flags::Align_SP_Folded),
                                   true),
                             m::app(operand_source_is(Source::s_2), true)) =
                [&] {
                    auto offset =
                        stack->get_stack_frame_allocation_size(frame_name_) -
                        ((frame.pointers.size() - *address_pointer_index) * 8);
                    operand = direct_immediate(fmt::format("#{}", offset));
                });
    }
}

/**
 * @brief Emit an operand of an instruction
 *
 * The first operand follows the mnemonic, and a string literal loaded by
 * `ldr` is a literal pool address, `=label`
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
 * @brief Emit an operand of an instruction with its flags applied
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
 * @brief Emit the jump to the epilogue of the function, `_L1`
 */
void Text_Emitter::emit_epilogue_jump(std::ostream& os)
{
    os << assembly::tabwidth(4) << Mnemonic::b << " ";
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
        // callee saved registers are saved as "tokens" on the frame object
        if (!accessor_->get_frame_in_memory().get_stack_frame(s).tokens.empty())
            accessor_->stack->allocate(16);
        // this is a new frame, emit the last frame function epilogue
        if (frame_ != s) {
            emit_function_epilogue(os);
            accessor_->device_accessor.set_current_frame_symbol(s);
        }
        frame_ = s;
        address_pointer_index = table->functions.at(s).get().pointers.size();
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
 * @brief Emit a store to an element of a vector on the stack, through x15
 *
 * A page relative address is stored after the `add` that computes it
 */
void Text_Emitter::emit_vector_storage_instruction(std::ostream& os,
    std::size_t index,
    Storage const& operand)
{
    auto mnemonic = assembly::Mnemonic::mov;
    auto storage_emitter =
        Storage_Emitter{ accessor_, frame_, index, &address_pointer_index };
    auto size =
        memory::get_operand_size_from_storage(operand, accessor_->stack);

    if (assembly::is_immediate_relative_address(operand)) {
        storage_emitter.emit(
            os, Register::x6, mnemonic, Storage_Emitter::Source::s_0);
        storage_emitter.emit(
            os, operand, mnemonic, Storage_Emitter::Source::s_1);
        assembly::newline(os, 1);
        str_instructions.emplace_back("str x6, [x15]");
        return;
    }

    if (size == Operand_Size::Doubleword) {
        storage_emitter.emit(
            os, Register::x8, mnemonic, Storage_Emitter::Source::s_0);
        storage_emitter.emit(
            os, operand, mnemonic, Storage_Emitter::Source::s_1);
        assembly::newline(os, 1);
        os << assembly::tabwidth(4) << "str x8, [x15]" << std::endl;
    } else {
        storage_emitter.emit(
            os, Register::w8, mnemonic, Storage_Emitter::Source::s_0);
        storage_emitter.emit(
            os, operand, mnemonic, Storage_Emitter::Source::s_1);
        assembly::newline(os, 1);
        os << assembly::tabwidth(4) << "str w8, [x15]" << std::endl;
    }
}

/**
 * @brief Emit an instruction and its operands in the text section
 *
 * An instruction after the `_L1` label is held for the epilogue
 */
void Text_Emitter::emit_assembly_instruction(std::ostream& os,
    std::size_t index,
    Instruction const& s)
{
    auto& flag_accessor = accessor_->flag_accessor;
    auto [mnemonic, src1, src2, src3, src4] = s;
    auto storage_emitter =
        Storage_Emitter{ accessor_, frame_, index, &address_pointer_index };

    if (branch_ == "_L1" && label_size_ > 0) {
        return_instructions_.emplace_back(s);
        return;
    }
    if (flag_accessor.index_contains_flag(
            index, detail::flags::Vector_Storage)) {
        if (!flag_accessor.index_contains_flag(index, common::flag::Argument)) {
            os << assembly::tabwidth(4) << mnemonic;
            emit_vector_storage_instruction(os, index, src2);
            return;
        }
    }

    if (is_variant(Immediate, src4) and type::get_value_from_rvalue_data_type(
                                            std::get<Immediate>(src4)) == "0x0")
        set_alignment_flag_inline(Align_S3_Folded, index);

    os << assembly::tabwidth(4) << mnemonic;

    storage_emitter.emit(os, src1, mnemonic, Storage_Emitter::Source::s_0);
    storage_emitter.emit(os, src2, mnemonic, Storage_Emitter::Source::s_1);
    storage_emitter.emit(os, src3, mnemonic, Storage_Emitter::Source::s_2);
    storage_emitter.emit(os, src4, mnemonic, Storage_Emitter::Source::s_3);

    if (mnemonic == Mnemonic::add and
        assembly::is_immediate_relative_address(src3) and
        not str_instructions.empty()) {
        assembly::newline(os, 1);
        os << assembly::tabwidth(4) << str_instructions.back() << std::endl;
        str_instructions.pop_back();
    } else
        assembly::newline(os, 1);
}

/**
 * @brief Emit a label or an instruction in the text section
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
    const Instructions instructions = instructions_accessor->get_instructions();
    emit_text_directives(os);
    for (std::size_t index = 0; index < instructions_accessor->size(); index++)
        emit_text_instruction(os, instructions[index], index);
    if (!return_instructions_.empty())
        emit_function_epilogue(os);
}

/**
 * @brief Emit the directives of the text section and the entry point
 */
void Text_Emitter::emit_text_directives(std::ostream& os)
{
    assembly::newline(os, 1);
#if defined(__linux__)
    os << assembly::Directive::text << std::endl;
#elif defined(__APPLE__) || defined(__bsdi__)
    os << ".section	__TEXT,__text,regular,pure_instructions" << std::endl;
#elif defined(_WIN32) || defined(_WIN64)
    os << assembly::Directive::text << std::endl;
#endif
    assembly::newline(os, 1);
    emit_arm64_alignment_directive(os, 3, 2);
    os << assembly::tabwidth(4) << assembly::Directive::start;
    assembly::newline(os, 1);
    emit_stdlib_externs(os);
}

/**
 * @brief Emit the global directive of each standard library function
 *
 * A function of the standard library has a leading underscore on Darwin
 */
void Text_Emitter::emit_stdlib_externs(std::ostream& os)
{
    if (!test_no_stdlib)
        for (auto const& stdlib_f : common::runtime::get_library_symbols()) {
            os << assembly::tabwidth(4) << assembly::Directive::global << " ";
#if defined(__APPLE__) || defined(__bsdi__)
            os << "_" << stdlib_f;
#else
            os << stdlib_f;
#endif
            assembly::newline(os);
        }
    assembly::newline(os);
}
} // namespace credence::target::arm64