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

#pragma once

#include "assembly.h"                     // for Operand_Size, Storage, Dir...
#include "memory.h"                       // for Memory_Access, Operand_Size
#include "stack.h"                        // for Stack
#include <credence/ir/ita.h>              // for Instructions
#include <credence/object.h>              // for Label, Object, RValue
#include <credence/target/common/flags.h> // for flags
#include <credence/util.h>                // for AST_Node, CREDENCE_PRIVATE...
#include <cstddef>                        // for size_t
#include <ostream>                        // for ostream
#include <string>                         // for basic_string, string
#include <utility>                        // for move
#include <variant>                        // for variant

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

using Instruction_Pair = assembly::Instruction_Pair;

void emit(std::ostream& os,
    util::AST_Node const& symbols,
    util::AST_Node const& ast,
    bool no_stdlib);

constexpr std::string emit_immediate_storage(
    assembly::Immediate const& immediate);

constexpr std::string emit_stack_storage(assembly::Stack::Offset offset,
    assembly::Operand_Size size,
    common::flag::flags flags);

constexpr std::string emit_register_storage(assembly::Register device,
    assembly::Operand_Size size,
    common::flag::flags flags);

void emit_x86_64_assembly_intel_prologue(std::ostream& os);

class Assembly_Emitter;

/**
 * @brief Emit the operands of an instruction
 *
 * The flags of the instruction are applied to each operand, so an operand
 * that depends on the frame size is resolved here, after the function is
 * translated
 */
class Storage_Emitter
{
  public:
    explicit Storage_Emitter(memory::Memory_Access& accessor,
        std::size_t index,
        Storage& source_storage)
        : accessor_(accessor)
        , instruction_index_(index)
        , source_storage_(source_storage)
    {
    }

    constexpr void set_address_size(Operand_Size address)
    {
        address_size = address;
    }

    constexpr void reset_address_size() { address_size = Operand_Size::Empty; }

    std::string get_storage_device_as_string(assembly::Storage const& storage,
        Operand_Size size);

    void emit(std::ostream& os,
        assembly::Storage const& storage,
        assembly::Mnemonic mnemonic,
        memory::Operand_Type type_);

  private:
    memory::Memory_Access accessor_;
    std::size_t instruction_index_;

  private:
    Operand_Size address_size = Operand_Size::Empty;
    Storage& source_storage_;
};

/**
 * @brief Emit the text section of an x86-64 program
 *
 * The instructions after the `_L1` label of a function are its epilogue,
 * which the IR does not keep at the end, so they are held and emitted after
 * the function's last branch
 */
class Text_Emitter
{
  public:
    explicit Text_Emitter(memory::Memory_Access accessor)
        : accessor_(accessor)
    {
        instructions_ = accessor->instruction_accessor;
    }
    friend class Assembly_Emitter;

    void emit_stdlib_externs(std::ostream& os);
    void emit_text_directives(std::ostream& os);
    void emit_text_section(std::ostream& os);

  private:
    void emit_assembly_instruction(std::ostream& os,
        std::size_t index,
        assembly::Instruction const& s);
    void emit_assembly_label(std::ostream& os,
        Label const& s,
        bool set_label = true);
    void emit_text_instruction(std::ostream& os,
        std::variant<Label, assembly::Instruction> const& instruction,
        std::size_t index,
        bool set_label = true);
    void emit_function_epilogue(std::ostream& os);
    void emit_epilogue_jump(std::ostream& os);

  public:
    bool test_no_stdlib{ false };

  private:
    memory::Memory_Access accessor_;

  private:
    memory::Instruction_Pointer instructions_;
    assembly::Instructions return_instructions_;
    std::size_t label_size_{ 0 };
    Label frame_{};
    Label branch_{};
};

/**
 * @brief Emit the data section of an x86-64 program
 *
 * Floats, doubles, and strings are emitted first, so each global that holds
 * one refers to its label, then the globals
 */
class Data_Emitter
{
  public:
    explicit Data_Emitter(memory::Memory_Access accessor)
        : accessor_(accessor)
    {
    }
    friend class Assembly_Emitter;

  public:
    void emit_data_section(std::ostream& os);

  private:
    void set_data_globals();
    void set_data_strings();
    void set_data_floats();
    void set_data_doubles();
    void set_data_section();

  private:
    assembly::Directives get_instructions_from_directive_type(
        assembly::Directive directive,
        RValue const& rvalue);

  private:
    memory::Memory_Access accessor_;

  private:
    assembly::Directives instructions_;
};

/**
 * @brief Emit an x86-64 program, its text and data sections
 */
class Assembly_Emitter
{
  public:
    explicit Assembly_Emitter(memory::Memory_Access accessor)
        : accessor_(std::move(accessor))
    {
        ir_instructions_ = accessor_->table_accessor.get_ir_instructions();
    }

  public:
    void emit(std::ostream& os);

  private:
    memory::Memory_Access accessor_;

  private:
    ir::Instructions ir_instructions_;

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    Data_Emitter data_{ accessor_ };
    // clang-format on
  public:
    Text_Emitter text_{ accessor_ };
};

} // namespace x86_64
