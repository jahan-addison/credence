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
#include "flags.h"                           // for flags
#include "memory.h"                          // for Memory_Accessor, Instru...
#include "runtime.h"                         // for Library_Call_Inserter
#include "stack.h"                           // for Stack
#include "syscall.h"                         // for syscall_arguments_t
#include <credence/error.h>                  // for credence_assert, creden...
#include <credence/ir/checker.h>             // for Type_Checker
#include <credence/ir/ita.h>                 // for Instruction, Quadruple
#include <credence/ir/object.h>              // for RValue, Object, Label
#include <credence/ir/table.h>               // for Table
#include <credence/map.h>                    // for Ordered_Map
#include <credence/symbol.h>                 // for Symbol_Table
#include <credence/target/common/accessor.h> // for Buffer_Accessor
#include <credence/target/common/assembly.h> // for make_direct_immediate
#include <credence/target/common/memory.h>   // for is_temporary, Operand_Type
#include <credence/target/common/runtime.h>  // for is_stdlib_function, arg...
#include <credence/target/common/types.h>    // for Table_Pointer, Immediate
#include <credence/types.h>                  // for get_rvalue_datatype_fro...
#include <credence/util.h>                   // for is_variant, sv, AST_Node
#include <cstddef>                           // for size_t
#include <deque>                             // for deque
#include <easyjson.h>                        // for JSON
#include <fmt/base.h>                        // for copy
#include <fmt/compile.h>                     // for format, operator""_cf
#include <fmt/format.h>                      // for format
#include <matchit.h>                         // for App, pattern, Id, Wildcard
#include <memory>                            // for shared_ptr, make_shared
#include <ostream>                           // for basic_ostream, operator<<
#include <ostream>                           // for ostream
#include <string_view>                       // for basic_string_view, oper...
#include <tuple>                             // for get, tuple
#include <utility>                           // for get, pair, make_pair
#include <variant>                           // for variant, get, visit
#include <vector>                            // for vector

namespace credence::target::arm64 {

namespace m = matchit;

assembly::Operand_Size get_operand_size_from_storage(Storage const& storage,
    memory::Stack_Pointer& stack);

/**
 * @brief Assembly Emitter Factory
 *
 * Emit a complete arm64 program from an AST and symbols
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
    auto _start = memory::Stack_Frame{ accessor_ };
    _start.set_stack_frame("main");
    data_.set_data_section();
    auto inserter = IR_Inserter{ accessor_ };
    inserter.insert(ir_instructions_, std::move(_start));

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
    assembly::Storage const& storage)
{
    m::Id<assembly::Stack::Offset> s;
    m::Id<assembly::Register> r;
    m::Id<assembly::Immediate> i;
    auto flags = accessor_->flag_accessor.get_instruction_flags_at_index(
        instruction_index_);
    return m::match(storage)(
        m::pattern | m::as<assembly::Stack::Offset>(
                         s) = [&] { return emit_stack_storage(*s, flags); },
        m::pattern | m::as<assembly::Register>(
                         r) = [&] { return emit_register_storage(*r, flags); },
        m::pattern | m::as<assembly::Immediate>(
                         i) = [&] { return emit_immediate_storage(*i); });
}

void Storage_Emitter::apply_stack_alignment(assembly::Storage& operand,
    assembly::Mnemonic mnemonic,
    Source source,
    common::flag::flags flags)
{
    auto& stack = accessor_->stack;
    if (is_alignment_mnemonic(mnemonic)) {
        if (flags & flag::Align and source == Source::s_2) {
            operand =
                u32_int_immediate(stack->get_stack_frame_allocation_size());
            return;
        }
        if (flags & detail::flags::Align_S3_Folded and source == Source::s_3) {
            operand =
                u32_int_immediate(stack->get_stack_frame_allocation_size());
            return;
        }
        if (flags & detail::flags::Align_Folded and source == Source::s_2) {
            operand = u32_int_immediate(
                stack->get_stack_frame_allocation_size() - 16);
            return;
        }
        if (flags & detail::flags::Align_SP and source == Source::s_2) {
            operand = direct_immediate(fmt::format(
                "[sp, #-{}]!", stack->get_stack_frame_allocation_size()));
            return;
        }
        if (flags & detail::flags::Align_SP_Folded and source == Source::s_2) {
            operand = direct_immediate(fmt::format(
                "[sp, #{}]", stack->get_stack_frame_allocation_size() - 16));
            return;
        }
        if (flags & detail::flags::Align_SP_Local and source == Source::s_2) {
            operand = direct_immediate(fmt::format(
                "[sp, #-{}!]", stack->get_stack_frame_allocation_size() - 16));
            return;
        }
    }
}

void Storage_Emitter::emit_operand(std::ostream& os,
    assembly::Storage const& operand,
    assembly::Mnemonic mnemonic,
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
    assembly::Storage const& storage,
    assembly::Mnemonic mnemonic,
    Source source)
{
    auto& flag_accessor = accessor_->flag_accessor;
    auto flags =
        flag_accessor.get_instruction_flags_at_index(instruction_index_);
    if (is_variant(std::monostate, storage))
        return;

    auto operand = storage;
    apply_stack_alignment(operand, mnemonic, source, flags);

    if (flags & flag::Indirect_Source)
        flag_accessor.set_instruction_flag(flag::Indirect, instruction_index_);

    emit_operand(os, operand, mnemonic, source, flags);

    flag_accessor.unset_instruction_flag(flag::Indirect, instruction_index_);
}

void Text_Emitter::emit_epilogue_jump(std::ostream& os)
{
    os << assembly::tabwidth(4) << assembly::Mnemonic::b << " ";
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
        callee_saved = 0;
        // this is a new frame, emit the last frame function epilogue
        if (frame_ != s) {
            emit_function_epilogue(os);
            accessor_->device_accessor.set_current_frame_symbol(s);
        }
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

void Text_Emitter::emit_callee_saved_registers_stp(std::size_t index)
{
    auto stack_frame = accessor_->table_accessor.table_->get_stack_frame(
        accessor_->device_accessor.get_current_frame_name());
    bool has_x23 = stack_frame->tokens.contains("x23");
    bool has_x26 = stack_frame->tokens.contains("x26");
    auto& flag_accessor = accessor_->flag_accessor;
    auto flags = flag_accessor.get_instruction_flags_at_index(index);
    if (flags & detail::flags::Callee_Saved and callee_saved == 0) {
        if (has_x23 and has_x26) {
            flag_accessor.unset_instruction_flag(
                detail::flags::Callee_Saved, index);
            callee_saved = 1;
        } else if (has_x26) {
            flag_accessor.unset_instruction_flag(
                detail::flags::Callee_Saved, index);
            callee_saved = 1;
        } else if (has_x23) {
            flag_accessor.unset_instruction_flag(
                detail::flags::Callee_Saved, index);
            callee_saved = 1;
        }
    }
}

/**
 * @brief Emit a mnemonic and its possible operands in the text section
 */
void Text_Emitter::emit_assembly_instruction(std::ostream& os,
    std::size_t index,
    assembly::Instruction const& s)
{
    auto [mnemonic, src1, src2, src3, src4] = s;
    auto storage_emitter = Storage_Emitter{ accessor_, index };
    if (branch_ == "_L1" && label_size_ > 0) {
        return_instructions_.emplace_back(s);
        return;
    }
    auto& flag_accessor = accessor_->flag_accessor;
    auto stack_frame = accessor_->table_accessor.table_->get_stack_frame(
        accessor_->device_accessor.get_current_frame_name());
    auto flags = flag_accessor.get_instruction_flags_at_index(index);

    emit_callee_saved_registers_stp(index);

    if (flags & detail::flags::Callee_Saved and callee_saved != 1)
        return;

    os << assembly::tabwidth(4) << mnemonic;

    storage_emitter.emit(os, src1, mnemonic, Storage_Emitter::Source::s_0);
    storage_emitter.emit(os, src2, mnemonic, Storage_Emitter::Source::s_1);
    storage_emitter.emit(os, src3, mnemonic, Storage_Emitter::Source::s_2);
    storage_emitter.emit(os, src4, mnemonic, Storage_Emitter::Source::s_3);

    if (flags & detail::flags::Callee_Saved and callee_saved == 1)
        callee_saved = -1;

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
    const assembly::Instructions instructions =
        instructions_accessor->get_instructions();
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
 * @brief Setup the stack frame for a function during instruction insertion
 *
 *  Note:s x28 is reserved for the argc address and argv offsets in memory
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
    accessor_->device_accessor.set_current_frame_symbol(name);
    if (name == "main") {
        // setup argc, argv
        auto argc_argv = common::runtime::argc_argv_kernel_runtime_access<
            memory::Stack_Frame>(stack_frame);
        if (argc_argv.first) {
            auto& instructions =
                accessor_->instruction_accessor->get_instructions();
            auto argc_address = direct_immediate("[sp]");
            arm64_add__asm(instructions, ldr, x27, argc_address);
        }
    }
    visitor.from_func_start_ita(name);
}

/**
 * @brief IR instruction visitor to map arm64 instructions in memory
 */
void IR_Inserter::insert(ir::Instructions const& ir_instructions,
    memory::Stack_Frame&& initial_frame)
{
    auto stack_frame = initial_frame;
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
    auto& instructions = instruction_accessor->get_instructions();
    auto& stack = accessor_->stack;
    auto& table = accessor_->table_accessor.table_;
    auto& ir_instructions = *accessor_->table_accessor.table_->ir_instructions;
    credence_assert(table->functions.contains(name));
    stack->clear();
    accessor_->device_accessor.reset_storage_devices();
    stack_frame_.symbol = name;
    stack_frame_.set_stack_frame(name);
    auto frame = table->functions[name];
    stack->allocate(16);
    // set_alignment_flag(Align);
    // arm64_add__asm(instructions, sub, sp, sp, alignment__integer());
    set_alignment_flag(Align_SP);
    arm64_add__asm(instructions, stp, x29, x30, alignment__integer());
    arm64_add__asm(instructions, mov, x29, sp);
    if (table->stack_frame_contains_call_instruction(name, ir_instructions)) {
        set_alignment_flag(Align_Folded);
        arm64_add__asm(instructions, add, x29, sp, alignment__integer());
    }
    set_alignment_flag(Callee_Saved);
    arm64_add__asm(instructions, stp, x26, x23, alignment__sp_integer(16));
    auto allocated = 1;
    set_alignment_flag(Callee_Saved);
    arm64_add__asm(
        instructions, str, x26, alignment__sp_integer(16 * allocated));
    set_alignment_flag(Callee_Saved);
    arm64_add__asm(
        instructions, str, x23, alignment__sp_integer(16 * allocated));
}

// Unused
void Visitor::from_func_end_ita() {}

// Unused
void Visitor::from_cmp_ita([[maybe_unused]] ir::Quadruple const& inst) {}

/**
 * @brief IR Instruction Instruction::MOV
 */
void Visitor::from_mov_ita(ir::Quadruple const& inst)
{
    auto& table = accessor_->table_accessor.table_;
    auto lhs = ir::get_lvalue_from_mov_qaudruple(inst);
    auto rhs = ir::get_rvalue_from_mov_qaudruple(inst).first;

    auto expression_inserter = Expression_Inserter{ accessor_, stack_frame_ };
    auto operand_inserter = Operand_Inserter{ accessor_, stack_frame_ };

    auto is_global_vector = [&](RValue const& rvalue) {
        auto rvalue_reference = type::from_lvalue_offset(rvalue);
        return table->vectors.contains(rvalue_reference) and
               table->globals.is_pointer(rvalue_reference);
    };

    m::match(lhs, rhs)(
        m::pattern | m::ds(m::app(is_parameter, true), m::_) = [] {},
        m::pattern | m::ds(m::app(type::is_temporary, true), m::_) =
            [&] {
                expression_inserter.insert_lvalue_at_temporary_object_address(
                    lhs);
            },
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
        m::pattern | m::_ =
            [&] { operand_inserter.insert_from_mnemonic_operand(lhs, rhs); });
}

/**
 * @brief IR Instruction Instruction::PUSH
 */
void Visitor::from_push_ita(ir::Quadruple const& inst)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& table = accessor_->table_accessor.table_;
    auto frame = stack_frame_.get_stack_frame();
    stack_frame_.argument_stack.emplace_front(
        table->lvalue_at_temporary_object_address(std::get<1>(inst), frame));
}

/**
 * @brief IR Instruction Instruction::RETURN
 */
void Visitor::from_return_ita()
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    arm64_add__asm(instructions, ret);
}

/**
 * @brief IR Instruction Instruction::LEAVE
 */
void Visitor::from_leave_ita()
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();

    arm64_add__asm(instructions, ldp, x26, x23, alignment__sp_integer(16));

    auto sp_imm = direct_immediate("[sp]");
    set_alignment_flag(Align_S3_Folded);
    arm64_add__asm(instructions, ldp, x29, x30, sp_imm, alignment__integer());

    if (stack_frame_.symbol == "main")
        syscall_ns::exit_syscall(instructions, 0);
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
    operands.emplace_back(
        common::assembly::make_u32_int_immediate(buffer_size));
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
 * @brief Unary address-of expression inserter
 */
void Unary_Operator_Inserter::from_lvalue_address_of_expression(
    RValue const& expr)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto& address_space = accessor_->address_accessor;

    credence_assert(type::is_unary_expression(expr));

    auto op = type::get_unary_operator(expr);
    RValue rvalue = type::get_unary_rvalue_reference(expr);
    auto offset =
        accessor_->device_accessor.get_device_by_lvalue_reference(rvalue);

    if (op == "&") {
        address_space.address_ir_assignment = true;
        auto frame = stack_frame_.get_stack_frame();
        auto acc =
            accessor_->accumulator_accessor.get_accumulator_register_from_size(
                assembly::Operand_Size::Doubleword);
        frame->tokens.insert("x26");
        arm64_add__asm(instructions, add, acc, x26, offset);
        accessor_->set_signal_register(Register::x26);
    } else {
        credence_error("unreachable");
    }
}

/**
 * @brief Unary reference expression inserter
 */
void Unary_Operator_Inserter::from_lvalue_reference_expression(
    RValue const& expr)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto& address_space = accessor_->address_accessor;

    credence_assert(type::is_unary_expression(expr));

    auto op = type::get_unary_operator(expr);
    RValue rvalue = type::get_unary_rvalue_reference(expr);
    auto offset =
        accessor_->device_accessor.get_device_by_lvalue_reference(rvalue);

    if (op == "*") {
        address_space.address_ir_assignment = true;
        auto acc =
            accessor_->accumulator_accessor.get_accumulator_register_from_size(
                assembly::Operand_Size::Doubleword);
        auto access = fmt::format(
            "[{}]", common::assembly::get_storage_as_string(offset));
        arm64_add__asm(instructions, ldr, acc, direct_immediate(access));
        accessor_->set_signal_register(acc);
    } else {
        credence_error("unreachable");
    }
}

/**
 * @brief Clean up argument stack after function call
 */
void Visitor::from_pop_ita()
{
    stack_frame_.size = 0;
    stack_frame_.argument_stack.clear();
    if (!stack_frame_.call_stack.empty())
        stack_frame_.call_stack.pop_back();
}

/**
 * @brief IR Instruction Instruction::CALL
 */
void Visitor::from_call_ita(ir::Quadruple const& inst)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto inserter = Invocation_Inserter{ accessor_, stack_frame_ };
    auto& instructions = instruction_accessor->get_instructions();
    auto function_name = type::get_label_as_human_readable(std::get<1>(inst));

    auto is_syscall_function = [&](Label const& label) {
#if defined(__linux__)
        return common::runtime::is_syscall_function(label,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::ARM64);

#elif defined(CREDENCE_TEST) || defined(__APPLE__) || defined(__bsdi__)
        return common::runtime::is_syscall_function(label,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::ARM64);

#endif
    };

    auto is_stdlib_function = [&](Label const& label) {
#if defined(__linux__)
        return common::runtime::is_stdlib_function(label,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::ARM64);

#elif defined(CREDENCE_TEST) || defined(__APPLE__) || defined(__bsdi__)
        return common::runtime::is_stdlib_function(label,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::ARM64);

#endif
    };
    accessor_->device_accessor.save_and_allocate_before_instruction_jump(
        instructions);
    m::match(function_name)(
        m::pattern | m::app(is_syscall_function, true) =
            [&] {
                inserter.insert_from_syscall_function(
                    function_name, instructions);
            },
        m::pattern | m::app(is_stdlib_function, true) =
            [&] {
                inserter.insert_from_standard_library_function(
                    function_name, instructions);
            },
        m::pattern | m::_ =
            [&] {
                inserter.insert_from_user_defined_function(
                    function_name, instructions);
            });
    stack_frame_.call_stack.emplace_back(function_name);
    stack_frame_.tail = function_name;
    // accessor_->register_accessor.reset_available_registers();
    accessor_->device_accessor.restore_and_deallocate_after_instruction_jump(
        instructions);
}

/**
 * @brief IR Instruction Instruction::GOTO
 */
void Visitor::from_goto_ita(ir::Quadruple const& inst)
{
    auto label = common::assembly::make_direct_immediate(
        assembly::make_label(std::get<1>(inst), stack_frame_.symbol));
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    arm64_add__asm(instructions, b, label);
}

/**
 * @brief IR Instruction Instruction::LOCL
 */
void Visitor::from_locl_ita(ir::Quadruple const& inst)
{
    auto locl_lvalue = std::get<1>(inst);
    auto frame = stack_frame_.get_stack_frame();
    auto& table = accessor_->table_accessor.table_;
    auto& stack = accessor_->stack;
    auto is_vector = [&](RValue const& rvalue) {
        return table->vectors.contains(type::from_lvalue_offset(rvalue));
    };
    m::match(locl_lvalue)(
        m::pattern | m::app(type::is_dereference_expression, true) =
            [&] {
                auto lvalue =
                    type::get_unary_rvalue_reference(std::get<1>(inst));
                accessor_->device_accessor.insert_lvalue_to_device(
                    lvalue, stack_frame_);
            },
        m::pattern | m::app(is_vector, true) =
            [&] {
                auto vector = table->vectors.at(locl_lvalue);
                auto size = stack->get_stack_size_from_table_vector(*vector);
                stack->set_address_from_size(locl_lvalue, size);
            },
        m::pattern | m::_ =
            [&] {
                accessor_->device_accessor.insert_lvalue_to_device(
                    locl_lvalue, stack_frame_);
            }

    );
}

/**
 * @brief IR Instruction Instruction::JMP_E
 */
void Visitor::from_jmp_e_ita(ir::Quadruple const& inst)
{
    auto [_, of, with, jump] = inst;
    auto frame = stack_frame_.get_stack_frame();
    auto of_comparator = frame->temporary.at(of).substr(4);
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto of_rvalue_storage =
        accessor_->address_accessor
            .get_arm64_lvalue_and_insertion_instructions(
                of_comparator, accessor_->device_accessor, instructions.size())
            .first;
    auto with_rvalue_storage = type::get_rvalue_datatype_from_string(with);
    auto jump_label = assembly::make_label(jump, stack_frame_.symbol);
    auto comparator_instructions = assembly::r_eq(
        of_rvalue_storage, with_rvalue_storage, jump_label, Register::w8);
    assembly::inserter(instructions, comparator_instructions);
}

/**
 * @brief Unused - read-ahead during relational jumps
 */
void Visitor::from_if_ita([[maybe_unused]] ir::Quadruple const& inst) {}

inline auto get_rvalue_pair_as_immediate(RValue const& lhs, RValue const& rhs)
{
    return std::make_pair(type::get_rvalue_datatype_from_string(lhs),
        type::get_rvalue_datatype_from_string(rhs));
}

/**
 * @brief Binary operator inserter of expression operands
 */
void Binary_Operator_Inserter::from_binary_operator_expression(
    RValue const& rvalue)
{
    credence_assert(type::is_binary_expression(rvalue));
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto& table_accessor = accessor_->table_accessor;
    auto& addresses = accessor_->address_accessor;
    auto& devices = accessor_->device_accessor;
    auto registers = accessor_->register_accessor;
    auto accumulator = accessor_->accumulator_accessor;

    Storage lhs_s{};
    Storage rhs_s{};

    Operand_Inserter operand_inserter(accessor_, stack_frame_);
    auto expression = type::from_rvalue_binary_expression(rvalue);
    auto [lhs, rhs, op] = expression;
    auto immediate = false;
    auto is_address = [&](RValue const& rvalue) {
        return devices.is_lvalue_allocated_in_memory(rvalue);
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
                    lhs_s = devices.get_device_by_lvalue(lhs);
                    rhs_s = devices.get_device_by_lvalue(rhs);
                } else {
                    lhs_s = accumulator.get_accumulator_register_from_size();
                    rhs_s = devices.get_device_by_lvalue(rhs);
                }
            },
        m::pattern |
            m::ds(m::app(is_temporary, true), m::app(is_temporary, true)) =
            [&] {
                auto size = assembly::get_operand_size_from_register(
                    accumulator.get_accumulator_register_from_size());
                auto acc = accumulator.get_accumulator_register_from_size();
                lhs_s = acc;
                auto& immediate_stack = addresses.immediate_stack;
                if (!immediate_stack.empty()) {
                    rhs_s = immediate_stack.back();
                    immediate_stack.pop_back();
                    if (!immediate_stack.empty()) {
                        arm64_add__asm(
                            instructions, mov, acc, immediate_stack.back());
                        immediate_stack.pop_back();
                    }
                } else {
                    auto intermediate = registers.get_second_register_from_size(
                        assembly::get_size_from_operand_size(size));
                    rhs_s = intermediate;
                }
            },
        m::pattern |
            m::ds(m::app(is_address, true), m::app(is_address, false)) =
            [&] {
                lhs_s = devices.get_device_by_lvalue(lhs);
                rhs_s = devices.get_device_for_binary_operand(rhs);

                if (table_accessor.last_ir_instruction_is_assignment()) {
                    auto rvalue = devices.get_device_by_lvalue(lhs);
                    auto size = devices.get_word_size_from_storage(
                        rvalue, stack_frame_);
                    auto acc =
                        accumulator.get_accumulator_register_from_size(size);
                    arm64_add__asm(instructions, mov, acc, rvalue);
                }
                if (is_temporary(rhs) or
                    table_accessor.is_ir_instruction_temporary())
                    lhs_s = accumulator.get_accumulator_register_from_size(
                        devices.get_word_size_from_lvalue(lhs, stack_frame_));
            },
        m::pattern |
            m::ds(m::app(is_address, false), m::app(is_address, true)) =
            [&] {
                lhs_s = devices.get_device_for_binary_operand(lhs);
                rhs_s = devices.get_device_by_lvalue(rhs);

                if (table_accessor.last_ir_instruction_is_assignment()) {
                    auto acc = accumulator.get_accumulator_register_from_size(
                        devices.get_word_size_from_lvalue(rhs, stack_frame_));
                    arm64_add__asm(instructions,
                        mov,
                        acc,
                        devices.get_device_by_lvalue(rhs));
                }

                if (is_temporary(lhs) or
                    table_accessor.is_ir_instruction_temporary())
                    rhs_s = accumulator.get_accumulator_register_from_size(
                        devices.get_word_size_from_lvalue(rhs, stack_frame_));
            },
        m::pattern |
            m::ds(m::app(is_temporary, true), m::app(is_temporary, false)) =
            [&] {
                lhs_s = accumulator.get_accumulator_register_from_size();
                rhs_s = devices.get_device_for_binary_operand(rhs);
            },
        m::pattern |
            m::ds(m::app(is_temporary, false), m::app(is_temporary, true)) =
            [&] {
                lhs_s = devices.get_device_for_binary_operand(lhs);
                rhs_s = accumulator.get_accumulator_register_from_size();
            },
        m::pattern | m::ds(m::_, m::_) =
            [&] {
                lhs_s = devices.get_device_for_binary_operand(lhs);
                rhs_s = devices.get_device_for_binary_operand(rhs);
            });
    if (!immediate) {
        auto operand_inserter = Operand_Inserter{ accessor_, stack_frame_ };
        assembly::Binary_Operands operands = { lhs_s, rhs_s };
        operand_inserter.insert_from_operands(operands, op);
    }
}

/**
 * @brief Inserter from ir unary expression types
 */
void Unary_Operator_Inserter::from_temporary_unary_operator_expression(
    RValue const& expr)
{
    credence_assert(type::is_unary_expression(expr));
    auto operand_inserter = Operand_Inserter{ accessor_, stack_frame_ };
    auto op = type::get_unary_operator(expr);
    RValue temporary = type::get_unary_rvalue_reference(expr);
    auto storage = operand_inserter.get_operand_storage_from_rvalue(temporary);
    insert_from_unary_expression(op, storage);
}

/**
 * @brief Inserter from unary expression
 */
void Unary_Operator_Inserter::insert_from_unary_expression(
    std::string const& op,
    Storage const& dest,
    Storage const& src)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto index = accessor_->instruction_accessor->size();
    auto is_stack = [&](assembly::Storage const& st) {
        return is_variant(assembly::Stack::Offset, st);
    };

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
                auto acc = Register::x2;
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
                if (is_stack(dest)) {
                    arm64_add__asm(instructions, ldr, acc, dest);
                } else {
                    arm64_add__asm(instructions, mov, acc, dest);
                }
                accessor_->flag_accessor.set_instruction_flag(
                    flag::Indirect, index);
                arm64_add__asm(instructions, mov, acc, src);
            },
        m::pattern | std::string{ "-" } =
            [&] {
                assembly::inserter(instructions, assembly::neg(dest).second);
            },
        m::pattern | std::string{ "+" } = [&] {});
}

/**
 * @brief Expression inserter for lvalue at temporary object address
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
 * @brief Expression inserter from rvalue
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
                arm64_add__asm(instructions, mov, acc, immediate);
                auto type = type::get_type_from_rvalue_data_type(rvalue);
                if (type == "string")
                    accessor_->flag_accessor.set_instruction_flag(
                        flag::Address, instruction_accessor->size());
            },
        m::pattern | m::app(is_comparator, true) = [&] {},
        m::pattern | RValue{ "RET" } =
            [&] {

#if defined(__linux__)
                if (common::runtime::is_stdlib_function(stack_frame_.tail,
                        common::assembly::OS_Type::Linux,
                        common::assembly::Arch_Type::ARM64))
                    return;
#elif defined(CREDENCE_TEST) || defined(__APPLE__) || defined(__bsdi__)
                if (common::runtime::is_stdlib_function(stack_frame_.tail,
                        common::assembly::OS_Type::BSD,
                        common::assembly::Arch_Type::ARM64))
                    return;
#endif
                credence_assert(table->functions.contains(stack_frame_.tail));
                auto frame = table->functions[stack_frame_.tail];
                credence_assert(frame->ret.has_value());
                auto immediate =
                    operand_inserter.get_operand_storage_from_rvalue(
                        frame->ret->first);
                if (get_operand_size_from_storage(
                        immediate, accessor_->stack) ==
                    assembly::Operand_Size::Doubleword) {
                    accessor_->set_signal_register(Register::x0);
                }
            },
        m::pattern | m::_ =
            [&] {
                auto symbols = table->get_stack_frame_symbols();
                Storage immediate = symbols.get_symbol_by_name(rvalue);
                auto acc = accessor_->accumulator_accessor
                               .get_accumulator_register_from_storage(
                                   immediate, accessor_->stack);
                arm64_add__asm(instructions, mov, acc, immediate);
            });
}

/**
 * @brief Expression inserter for global vector assignment
 */
void Expression_Inserter::insert_from_global_vector_assignment(
    LValue const& lhs,
    LValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto [lhs_storage, lhs_storage_inst] =
        accessor_->address_accessor.get_arm64_lvalue_and_insertion_instructions(
            lhs, accessor_->device_accessor, instruction_accessor->size());
    assembly::inserter(instructions, lhs_storage_inst);
    auto [rhs_storage, rhs_storage_inst] =
        accessor_->address_accessor.get_arm64_lvalue_and_insertion_instructions(
            rhs, accessor_->device_accessor, instruction_accessor->size());
    assembly::inserter(instructions, rhs_storage_inst);
    auto acc =
        accessor_->accumulator_accessor.get_accumulator_register_from_storage(
            lhs_storage, accessor_->stack);
    arm64_add__asm(instructions, ldr, acc, rhs_storage);
    arm64_add__asm(instructions, str, acc, lhs_storage);
}

/**
 * @brief Operand Inserter mediator for expression mnemonics operands
 */
void Operand_Inserter::insert_from_operands(assembly::Binary_Operands& operands,
    std::string const& op)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    if (is_variant(Immediate, operands.first) and
        not assembly::is_immediate_x28_address_offset(operands.first) and
        not assembly::is_immediate_relative_address(operands.first))
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
 * @brief Operand inserter for immediate rvalues
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
                arm64_add__asm(instructions, mov, acc, imm);
            },
        m::pattern | m::app(type::is_relation_binary_operator, true) =
            [&] {
                auto imm = common::assembly::
                    get_result_from_trivial_relational_expression(lhs, op, rhs);
                auto acc = accumulator.get_accumulator_register_from_size(
                    assembly::Operand_Size::Byte);
                accessor_->set_signal_register(acc);
                arm64_add__asm(instructions, mov, acc, imm);
            },
        m::pattern | m::app(type::is_bitwise_binary_operator, true) =
            [&] {
                auto imm = common::assembly::
                    get_result_from_trivial_bitwise_expression(lhs, op, rhs);
                auto acc = accumulator.get_accumulator_register_from_size(
                    assembly::get_operand_size_from_rvalue_datatype(lhs));
                if (!accessor_->table_accessor.is_ir_instruction_temporary())
                    arm64_add__asm(instructions, mov, acc, imm);
                else
                    immediate_stack.emplace_back(imm);
            },
        m::pattern | m::_ = [&] { credence_error("unreachable"); });
}

/**
 * @brief Operand inserter for mnemonic operand
 */
void Operand_Inserter::insert_from_mnemonic_operand(LValue const& lhs,
    RValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto& stack = accessor_->stack;
    auto& address_storage = accessor_->address_accessor;
    auto accumulator = accessor_->accumulator_accessor;
    auto is_address = [&](RValue const& rvalue) {
        return accessor_->device_accessor.is_lvalue_allocated_in_memory(rvalue);
    };
    auto is_stack = [&](assembly::Storage const& st) {
        return is_variant(assembly::Stack::Offset, st);
    };
    m::match(rhs)(
        m::pattern | m::app(is_immediate, true) =
            [&] {
                auto imm = type::get_rvalue_datatype_from_string(rhs);
                auto [lhs_storage, storage_inst] =
                    accessor_->address_accessor
                        .get_arm64_lvalue_and_insertion_instructions(lhs,
                            accessor_->device_accessor,
                            instruction_accessor->size());
                assembly::inserter(instructions, storage_inst);
                if (is_stack(lhs_storage)) {
                    auto size = get_operand_size_from_storage(
                        lhs_storage, accessor_->stack);
                    auto work = size == assembly::Operand_Size::Doubleword
                                    ? assembly::Register::x8
                                    : assembly::Register::w8;
                    arm64_add__asm(instructions, mov, work, imm);
                    arm64_add__asm(instructions, str, work, lhs_storage);
                } else {
                    arm64_add__asm(instructions, mov, lhs_storage, imm);
                }
            },
        m::pattern | m::app(is_temporary, true) =
            [&] {
                if (address_storage.address_ir_assignment) {
                    address_storage.address_ir_assignment = false;
                    Storage lhs_storage =
                        accessor_->device_accessor.get_device_by_lvalue(lhs);
                    auto size = get_operand_size_from_storage(
                        lhs_storage, accessor_->stack);
                    auto work = size == assembly::Operand_Size::Doubleword
                                    ? assembly::Register::x8
                                    : assembly::Register::w8;
                    arm64_add__asm(instructions, mov, lhs_storage, work);
                } else {
                    auto acc = accumulator.get_accumulator_register_from_size();
                    if (!type::is_unary_expression(lhs))
                        stack->set_address_from_accumulator(lhs, acc);
                    auto [lhs_storage, storage_inst] =
                        address_storage
                            .get_arm64_lvalue_and_insertion_instructions(lhs,
                                accessor_->device_accessor,
                                instruction_accessor->size());
                    assembly::inserter(instructions, storage_inst);
                    arm64_add__asm(instructions, mov, lhs_storage, acc);
                }
            },
        m::pattern | m::app(is_address, true) =
            [&] {
                auto [lhs_storage, lhs_inst] =
                    address_storage.get_arm64_lvalue_and_insertion_instructions(
                        lhs,
                        accessor_->device_accessor,
                        instruction_accessor->size());
                assembly::inserter(instructions, lhs_inst);
                auto [rhs_storage, rhs_inst] =
                    address_storage.get_arm64_lvalue_and_insertion_instructions(
                        rhs,
                        accessor_->device_accessor,
                        instruction_accessor->size());
                assembly::inserter(instructions, rhs_inst);
                auto acc = accumulator.get_accumulator_register_from_size(
                    stack->get(rhs).second);
                arm64_add__asm(instructions, ldr, acc, rhs_storage);
                arm64_add__asm(instructions, str, acc, lhs_storage);
            },
        m::pattern | m::_ = [&] {});
}

/**
 * @brief Get the storage device of a stack operand
 */
inline Storage Operand_Inserter::get_operand_storage_from_stack(
    RValue const& rvalue)
{
    auto [operand, operand_inst] =
        accessor_->address_accessor.get_arm64_lvalue_and_insertion_instructions(
            rvalue,
            accessor_->device_accessor,
            accessor_->instruction_accessor->size());
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
        return Register::x8;
    else
        return Register::w8;
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
            return direct_immediate("[x28]");
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
                fmt::format("[x28, #{}]", 8 * offset_integer));
        }
    }
    if (frame->is_pointer_parameter(rvalue))
        return memory::registers::available_doubleword.at(index_of);
    else
        return memory::registers::available_word.at(index_of);
}

Storage Operand_Inserter::get_operand_storage_from_rvalue(RValue const& rvalue)
{
    auto& stack = accessor_->stack;
    auto frame = stack_frame_.get_stack_frame();

    if (frame->is_parameter(rvalue))
        return get_operand_storage_from_parameter(rvalue);

    if (stack->is_allocated(rvalue))
        return get_operand_storage_from_stack(rvalue);

#if defined(__linux__)
    if (!stack_frame_.tail.empty() and
        not common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::ARM64))
        return get_operand_storage_from_return();

#elif defined(CREDENCE_TEST) || defined(__APPLE__) || defined(__bsdi__)
    if (!stack_frame_.tail.empty() and
        not common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::ARM64))
        return get_operand_storage_from_return();

#endif

    if (type::is_rvalue_data_type(rvalue))
        return get_operand_storage_from_immediate(rvalue);

    auto [operand, operand_inst] =
        accessor_->address_accessor.get_arm64_lvalue_and_insertion_instructions(
            rvalue,
            accessor_->device_accessor,
            accessor_->instruction_accessor->size());
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    assembly::inserter(instructions, operand_inst);

    return operand;
}

/**
 * @brief Get operands storage from argument stack
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
                arguments.emplace_back(Register::x0);
            else
                arguments.emplace_back(Register::w0);
        } else {
            arguments.emplace_back(
                operands.get_operand_storage_from_rvalue(rvalue));
        }
    }
    return arguments;
}

/**
 * @brief Invocation inserter for syscall function
 */
void Invocation_Inserter::insert_from_syscall_function(std::string_view routine,
    assembly::Instructions& instructions)
{
    accessor_->address_accessor.buffer_accessor.set_buffer_size_from_syscall(
        routine, stack_frame_.argument_stack);
    auto operands = this->get_operands_storage_from_argument_stack();

    syscall_ns::make_syscall(
        instructions, routine, operands, &stack_frame_, &accessor_);
}

/**
 * @brief Invocation inserter for user defined functions
 */
void Invocation_Inserter::insert_from_user_defined_function(
    std::string_view routine,
    assembly::Instructions& instructions)
{
    auto operands = this->get_operands_storage_from_argument_stack();
    for (std::size_t i = 0; i < operands.size(); i++) {
        auto const& operand = operands[i];
        auto size = get_operand_size_from_storage(operand, accessor_->stack);
        auto arg_register = assembly::get_register_from_integer_argument(i);
        if (is_variant(Immediate, operand)) {
            if (type::is_rvalue_data_type_string(
                    std::get<Immediate>(operand))) {
                accessor_->flag_accessor.set_instruction_flag(
                    flag::Load, accessor_->instruction_accessor->size());
            }
        }
        if (size == assembly::Operand_Size::Doubleword)
            accessor_->flag_accessor.set_instruction_flag(
                common::flag::Argument,
                accessor_->instruction_accessor->size());
        arm64_add__asm(instructions, mov, arg_register, operand);
    }
    arm64_add__asm(instructions, bl, direct_immediate(routine));
}

/**
 * @brief Invocation inserter for standard library function
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
        m::pattern | m::_ = [&] { return assembly::Operand_Size::Empty; });
}

/**
 * @brief Arithmetic operator inserter for expression operands
 */
assembly::Instruction_Pair
Arithemtic_Operator_Inserter::from_arithmetic_expression_operands(
    assembly::Binary_Operands const& operands,
    std::string const& binary_op)
{
    assembly::Instruction_Pair instructions{ Register::w8, {} };
    m::match(binary_op)(
        m::pattern | std::string{ "*" } =
            [&] {
                auto frame =
                    accessor_->table_accessor.table_->get_stack_frame();
                frame->tokens.insert("x23");
                instructions = assembly::mul(operands.first, operands.second);
            },
        m::pattern | std::string{ "/" } =
            [&] {
                auto storage =
                    accessor_->device_accessor.get_available_storage_register(
                        assembly::Operand_Size::Doubleword);
                arm64_add__asm(
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
                    accessor_->device_accessor.get_available_storage_register(
                        assembly::Operand_Size::Doubleword);
                accessor_->set_signal_register(Register::x1);
                arm64_add__asm(
                    instructions.second, mov, storage, operands.first);
                instructions = assembly::mod(storage, operands.second);
            });
    return instructions;
}

/**
 * @brief Bitwise operator inserter for expression operands
 */
assembly::Instruction_Pair
Bitwise_Operator_Inserter::from_bitwise_expression_operands(
    assembly::Binary_Operands const& operands,
    std::string const& binary_op)
{
    assembly::Instruction_Pair instructions{ Register::w8, {} };
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
 * @brief Relational operator inserter for expression operands
 */
assembly::Instructions
Relational_Operator_Inserter::from_relational_expression_operands(
    assembly::Binary_Operands const& operands,
    std::string const& binary_op,
    Label const& jump_label)
{
    auto register_storage = Register::w8;
    if (accessor_->address_accessor.is_doubleword_storage_size(
            operands.first, stack_frame_) or
        accessor_->address_accessor.is_doubleword_storage_size(
            operands.second, stack_frame_)) {
        register_storage = Register::x8;
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